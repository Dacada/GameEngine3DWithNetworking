#include <networkController.h>
#include <timeutil.h>
#include <events.h>
#include <thirty/util.h>

static bool shouldSendPacket(bool *const sentMovementPacket, struct timespec *const lastMovementPacket) {
        if (!*sentMovementPacket) {
                *lastMovementPacket = monotonic();
                *sentMovementPacket = true;
                return true;
        } else {
                struct timespec t = monotonic();
                unsigned long diff_ns = monotonic_difference(t, *lastMovementPacket);
                unsigned long diff_ms = diff_ns / 1000000;
                if (diff_ms >= PACKET_SEND_RATELIMIT_MS) {
                        *lastMovementPacket = t;
                        return true;
                }
                return false;
        }
}

////////////////////////////////////////////////////////////////////////////////

static void onPositionCorrectionPacket(struct networkController *const controller,
                                 const struct networkPacketPositionCorrection *const packet) {
        (void)controller;
        struct eventPlayerPositionCorrected args;
        args.position = packet->position;
        args.jumping = packet->jumpFall & 0x1;
        args.falling = packet->jumpFall & 0x2;
        eventBroker_fire((enum eventBrokerEvent)EVENT_SERVER_CORRECTED_PLAYER_POSITION, &args);
}
static void onWelcomePacket(struct networkController *const controller,
                            const struct networkPacketWelcome *const packet) {
        controller->connected = true;
        controller->id = packet->id;
        for (size_t i=0; i<packet->count; i++) {
                struct eventNetworkEntityNew args;
                args.idx = packet->currentEntities[i].idx;
                args.position = packet->currentEntities[i].position;
                args.rotation = packet->currentEntities[i].rotation;
                eventBroker_fire((enum eventBrokerEvent)EVENT_NETWORK_ENTITY_NEW, &args);
        }
}
static void onEntityChangesUpdate(struct networkController *const controller,
                                  const struct networkPacketEntityChangesUpdate *const packet) {
        (void)controller;
        
        if (packet->count == 0) {
                fprintf(stderr, "WARNING: SPURIOUS PACKET RECEIVED!!\n");
                return;
        }
        
        for (size_t i=0; i<packet->count; i++) {
                const struct networkPacketEntityChange *entity = &packet->entities[i];
                if (entity->idx == controller->id) {
                        continue;
                }
                fprintf(stderr, "onEntityChangesUpdate %u\n", entity->idx);
                struct eventNetworkEntityUpdate args;
                args.idx = entity->idx;
                args.position = entity->position;
                args.rotation = entity->rotation;
                eventBroker_fire((enum eventBrokerEvent)EVENT_NETWORK_ENTITY_UPDATE, &args);
        }
}

static void onEntityNew(struct networkController *const controller,
                        const struct networkPacketNewEntity *const packet) {
        if (packet->idx == controller->id) {
                return;
        }
        fprintf(stderr, "onEntityNew %u\n", packet->idx);

        struct eventNetworkEntityNew args;
        args.idx = packet->idx;
        args.position = packet->position;
        args.rotation = packet->rotation;
        eventBroker_fire((enum eventBrokerEvent)EVENT_NETWORK_ENTITY_NEW, &args);
}

static void onEntityDel(struct networkController *const controller,
                        const struct networkPacketDelEntity *const packet) {
        if (packet->idx == controller->id) {
                return;
        }
        fprintf(stderr, "onEntityDel %u\n", packet->idx);
        
        struct eventNetworkEntityDel args;
        args.idx = packet->idx;
        eventBroker_fire((enum eventBrokerEvent)EVENT_NETWORK_ENTITY_DEL, &args);
}

////////////////////////////////////////////////////////////////////////////////

static void onControlPacket(struct networkController *const controller,
                            const struct networkPacket *const packet) {
        switch (packet->type) {
        case PACKET_TYPE_WELCOME:
                onWelcomePacket(controller, (const void*)packet);
                break;
        default:
                fprintf(stderr, "unexpected control packet type %u\n", packet->type);
                break;
        }
}
static void onMovementPacket(struct networkController *const controller,
                             const struct networkPacket *const packet) {
        switch (packet->type) {
        case PACKET_TYPE_POSITION_CORRECTION:
                onPositionCorrectionPacket(controller, (const void*)packet);
                break;
        default:
                fprintf(stderr, "unexpected control packet type %u\n", packet->type);
                break;
        }
}
static void onServerUpdatePacket(struct networkController *const controller,
                                 const struct networkPacket *const packet) {
        switch (packet->type) {
        case PACKET_TYPE_ENTITY_CHANGES_UPDATE:
                onEntityChangesUpdate(controller, (const void*)packet);
                break;
        case PACKET_TYPE_NEW_ENTITY:
                onEntityNew(controller, (const void*)packet);
                break;
        case PACKET_TYPE_DEL_ENTITY:
                onEntityDel(controller, (const void*)packet);
                break;
        default:
                fprintf(stderr, "unexpected server update packet type %u\n", packet->type);
                break;
        }
}

////////////////////////////////////////////////////////////////////////////////

static void onConnected(void *registerArgs, void *fireArgs) {
        struct networkController *controller = registerArgs;
        ENetEvent *event = ((ENetEvent*)fireArgs);

        (void)event;
        (void)controller;
        fprintf(stderr, "connected to server\n");
}
static void onDisconnected(void *registerArgs, void *fireArgs) {
        struct networkController *controller = registerArgs;
        ENetEvent *event = ((ENetEvent*)fireArgs);

        (void)controller;
        (void)event;
        fprintf(stderr, "disconnected from server\n");
}
static void onReceived(void *registerArgs, void *fireArgs) {
        struct networkController *controller = registerArgs;
        ENetEvent *event = ((ENetEvent*)fireArgs);

        switch (event->channelID) {
        case NETWORK_CHANNEL_CONTROL:
                onControlPacket(controller, (void*)event->packet->data);
                break;
        case NETWORK_CHANNEL_MOVEMENT:
                onMovementPacket(controller, (void*)event->packet->data);
                break;
        case NETWORK_CHANNEL_SERVER_UPDATES:
                onServerUpdatePacket(controller, (void*)event->packet->data);
                break;
        default:
                fprintf(stderr, "PACKET RECEIVED ON UNEXPECTED CHANNEL %u\n",
                        event->channelID);
                break;
        }

        enet_packet_destroy(event->packet);
}

////////////////////////////////////////////////////////////////////////////////

static void onPlayerJumped(void *registerArgs, void *fireArgs) {
        struct networkController *controller = registerArgs;
        struct eventPlayerJumped *jumped = fireArgs;

        if (!controller->connected) {
                return;
        }

        (void)jumped;
        struct networkPacketJump data;
        data.base.type = PACKET_TYPE_JUMP_UPDATE;

        ENetPacket *packet = enet_packet_create(&data, sizeof(data), 0);
        enet_peer_send(controller->game->server, NETWORK_CHANNEL_MOVEMENT, packet);
}
static void onPlayerPositionChanged(void *registerArgs, void *fireArgs) {
        struct networkController *controller = registerArgs;
        struct eventPlayerPositionChanged *pos = fireArgs;

        if (!controller->connected) {
                return;
        }

        if (!shouldSendPacket(&controller->sentPosPacket,
                              &controller->lastTimeSentPosPacket)) {
                return;
        }

        struct networkPacketPosition data;
        data.base.type = PACKET_TYPE_POSITION_UPDATE;
        data.position = pos->position;

        ENetPacket *packet = enet_packet_create(&data, sizeof(data), 0);
        enet_peer_send(controller->game->server, NETWORK_CHANNEL_MOVEMENT, packet);
}
static void onPlayerRotationChanged(void *registerArgs, void *fireArgs) {
        struct networkController *controller = registerArgs;
        struct eventPlayerRotationChanged *rot = fireArgs;

        if (!controller->connected) {
                return;
        }

        if (!shouldSendPacket(&controller->sentRotPacket,
                              &controller->lastTimeSentRotPacket)) {
                return;
        }

        struct networkPacketRotation data;
        data.base.type = PACKET_TYPE_ROTATION_UPDATE;
        data.rotation = rot->rotation;

        ENetPacket *packet = enet_packet_create(&data, sizeof(data), 0);
        enet_peer_send(controller->game->server, NETWORK_CHANNEL_MOVEMENT, packet);
}

////////////////////////////////////////////////////////////////////////////////

void networkController_setup(struct networkController *controller, struct game *game, const char *host, unsigned short port) {
        controller->game = game;
        controller->connected = false;
        
        controller->sentPosPacket = false;
        controller->sentRotPacket = false;
        
        game_connect(game, NETWORK_CHANNELS_TOTAL, 0, 0, host, port, 0);

        eventBroker_register(onConnected, EVENT_BROKER_PRIORITY_HIGH,
                             EVENT_BROKER_NETWORK_CONNECTED, controller);
        eventBroker_register(onDisconnected, EVENT_BROKER_PRIORITY_HIGH,
                             EVENT_BROKER_NETWORK_DISCONNECTED, controller);
        eventBroker_register(onReceived, EVENT_BROKER_PRIORITY_HIGH,
                             EVENT_BROKER_NETWORK_RECV, controller);

        eventBroker_register(onPlayerJumped, EVENT_BROKER_PRIORITY_HIGH,
                             (enum eventBrokerEvent)EVENT_PLAYER_JUMPED, controller);
        eventBroker_register(onPlayerPositionChanged, EVENT_BROKER_PRIORITY_HIGH,
                             (enum eventBrokerEvent)EVENT_PLAYER_POSITION_CHANGED, controller);
        eventBroker_register(onPlayerRotationChanged, EVENT_BROKER_PRIORITY_HIGH,
                             (enum eventBrokerEvent)EVENT_PLAYER_ROTATION_CHANGED, controller);
}
