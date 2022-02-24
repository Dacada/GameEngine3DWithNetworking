#define _GNU_SOURCE

#include <networkController.h>
#include <events.h>
#include <thirty/util.h>
#include <time.h>

#define PACKET_SEND_RATELIMIT 50

// TODO: related function in server
static unsigned usign(float num) {
        if (ABS(num) < 1e-9) {
                return 0;
        } else if (num > 0) {
                return 1;
        } else {
                return 2;
        }
}

// TODO: repeated in server and client
static struct timespec monotonic(void) {
        struct timespec tp = {0};
        if (clock_gettime(CLOCK_MONOTONIC, &tp) == -1) {
                perror("clock_gettime");
        }
        
        return tp;
}

// TODO: repeated in server and client
static unsigned long monotonic_difference(struct timespec a, struct timespec b) {
        long sec = a.tv_sec - b.tv_sec;
        if (sec < 0) {
                return 0;
        }
        
        long nsec = a.tv_nsec - b.tv_nsec;
        if (nsec < 0) {
                sec -= 1;
                nsec = (long)1e9 + nsec;
        }
        
        nsec += sec * (long)1e9;
        if (nsec < 0) {
                return 0;
        }
        return (unsigned long)nsec;
}

static int send_packet(ENetPeer *peer, enet_uint8 channel, const void *data, size_t dataSize, enet_uint32 flags, bool rateLimit) {
        static bool first = true;
        static struct timespec last_send;

        bool should_send;
        if (first) {
                last_send = monotonic();
                first = false;
                should_send = true;
        } else {
                struct timespec t = monotonic();
                should_send = !rateLimit || monotonic_difference(t, last_send)/1000000 >= PACKET_SEND_RATELIMIT;
                if (should_send) {
                        last_send = t;
                }
        }

        if (should_send) {
                ENetPacket *packet = enet_packet_create(data, dataSize, flags);
                return enet_peer_send(peer, channel, packet);
        } else {
                return 0;
        }
}

static void onConnected(void *registerArgs, void *fireArgs) {
        struct networkController *controller = registerArgs;
        ENetEvent *event = ((ENetEvent*)fireArgs);

        (void)event;
        (void)controller;
        fprintf(stderr, "connected to server\n");
}

static void onPositionCorrection(struct networkController *const controller, const struct networkPacketPositionCorrection *const packet) {
        (void)controller;
        struct eventPlayerPositionCorrected args;
        args.position = packet->position;
        eventBroker_fire((enum eventBrokerEvent)EVENT_PLAYER_POSITION_CORRECTED, &args);
}

static void onMovementPacket(struct networkController *const controller, const struct networkPacketMovement *const packet) {
        switch (packet->base.type) {
        case PACKET_TYPE_POSITION_CORRECTION:
                onPositionCorrection(controller, (const void*)packet);
                break;
        default:
                break;
        }
}

static void onDisconnected(void *registerArgs, void *fireArgs) {
        struct networkController *controller = registerArgs;
        ENetEvent *event = ((ENetEvent*)fireArgs);

        (void)controller;
        (void)event;
        fprintf(stderr, "disconnected from server\n");
}

static void onPlayerIdPacket(struct networkController *const controller, const struct networkPacketPlayerId *const packet) {
        controller->connected = true;
        controller->id = packet->id;
        for (size_t i=0; i<packet->num_players; i++) {
                const struct networkedPlayerStatus *nwPlayer = packet->players + i;
                if (nwPlayer->id == packet->id) {
                        continue;
                }
                
                struct otherPlayer *player = growingArray_append(&controller->otherPlayers);
                player->id = nwPlayer->id;
                player->position = nwPlayer->position;
                player->direction = nwPlayer->direction;
                player->orientation = nwPlayer->orientation;
                player->airtime = nwPlayer->airtime;
                player->jumping = nwPlayer->jumpingFalling & 0x1;
                player->falling = nwPlayer->jumpingFalling & 0x2;
        }
        fprintf(stderr, "id: %u and %lu players\n", controller->id, controller->otherPlayers.length);
}

static void onControlPacket(struct networkController *const controller, const struct networkPacket *const packet) {
        switch (packet->type) {
        case PACKET_TYPE_PLAYER_ID:
                onPlayerIdPacket(controller, (const void*)packet);
                break;
        default:
                break;
        }
}

static void onReceived(void *registerArgs, void *fireArgs) {
        struct networkController *controller = registerArgs;
        ENetEvent *event = ((ENetEvent*)fireArgs);

        switch (event->channelID) {
        case 0:
                onControlPacket(controller, (void*)event->packet->data);
                break;
        case 1:
                onMovementPacket(controller, (void*)event->packet->data);
                break;
        default:
                break;
        }

        enet_packet_destroy(event->packet);
}

static void onPlayerJumped(void *registerArgs, void *fireArgs) {
        struct networkController *controller = registerArgs;
        struct eventPlayerJumped *jumped = fireArgs;

        if (!controller->connected) {
                return;
        }

        struct networkPacketJump data;
        data.base.base.type = PACKET_TYPE_JUMP;
        data.base.position = jumped->position;
        data.base.orientation = jumped->orientation;
        
        send_packet(controller->game->server, 1, &data, sizeof(data), ENET_PACKET_FLAG_RELIABLE, false);
}

static void onPlayerDirectionChange(void *registerArgs, void *fireArgs) {
        struct networkController *controller = registerArgs;
        struct eventPlayerDirectionChange *direction = fireArgs;

        if (!controller->connected) {
                return;
        }

        struct networkPacketDirection data;
        data.base.base.type = PACKET_TYPE_DIRECTION;
        data.base.position = direction->position;
        data.base.orientation = direction->orientation;

        data.direction = (uint8_t)(usign(direction->direction.x) & 0xF);
        data.direction |= (uint8_t)((usign(direction->direction.y) & 0xF) << 4);

        send_packet(controller->game->server, 1, &data, sizeof(data), ENET_PACKET_FLAG_RELIABLE, true);
}

static void onPlayerRotationChange(void *registerArgs, void *fireArgs) {
        struct networkController *controller = registerArgs;
        struct eventPlayerRotationChange *rotation = fireArgs;

        if (!controller->connected) {
                return;
        }

        struct networkPacketRotation data;
        data.base.base.type = PACKET_TYPE_ROTATION;
        data.base.position = rotation->position;
        data.base.orientation = rotation->orientation;

        send_packet(controller->game->server, 1, &data, sizeof(data), ENET_PACKET_FLAG_RELIABLE, true);
}

static void onUpdate(void *registerArgs, void *fireArgs) {
        struct networkController *controller = registerArgs;
        float timeDelta = ((struct eventBrokerUpdate*)fireArgs)->timeDelta;

        (void)controller;
        (void)timeDelta;
}

void networkController_setup(struct networkController *controller, struct game *game) {
        controller->game = game;
        controller->connected = false;
        growingArray_init(&controller->otherPlayers, sizeof(struct otherPlayer), 8);
        
        game_connect(game, 2, 0, 0, SERVER_HOST, SERVER_PORT, 0);

        eventBroker_register(onConnected, EVENT_BROKER_PRIORITY_HIGH,
                             EVENT_BROKER_NETWORK_CONNECTED, controller);
        eventBroker_register(onDisconnected, EVENT_BROKER_PRIORITY_HIGH,
                             EVENT_BROKER_NETWORK_DISCONNECTED, controller);
        eventBroker_register(onReceived, EVENT_BROKER_PRIORITY_HIGH,
                             EVENT_BROKER_NETWORK_RECV, controller);

        eventBroker_register(onPlayerJumped, EVENT_BROKER_PRIORITY_HIGH,
                             (enum eventBrokerEvent)EVENT_PLAYER_JUMPED, controller);
        eventBroker_register(onPlayerDirectionChange, EVENT_BROKER_PRIORITY_HIGH,
                             (enum eventBrokerEvent)EVENT_PLAYER_DIRECTION_CHANGED, controller);
        eventBroker_register(onPlayerRotationChange, EVENT_BROKER_PRIORITY_HIGH,
                             (enum eventBrokerEvent)EVENT_PLAYER_ROTATION_CHANGED, controller);
        eventBroker_register(onUpdate, EVENT_BROKER_PRIORITY_HIGH,
                             EVENT_BROKER_UPDATE, controller);
}

void networkController_unsetup(struct networkController *controller) {
  growingArray_destroy(&controller->otherPlayers);
}
