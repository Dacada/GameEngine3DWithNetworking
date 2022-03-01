#include <timeutil.h>
#include <entityUtils.h>
#include <networkController.h>
#include <curve.h>
#include <enet/enet.h>
#include <cglm/struct.h>
#include <errno.h>

#define TICK_PERIOD_NS 100000000L
#define TICK_PERIOD 0.1f
#define MAX_PLAYERS 512

#ifndef ABS
#define ABS(x) ((x)<0?-(x):(x))
#endif

struct player {
        bool init;
        size_t idx;
        
        ENetAddress address;

        vec3s position;        
        float rotation;

        bool jumping;
        bool falling;
        float airtime;
};

struct changedEntity {
        bool init;
        struct player *entity;
};

struct changedEntitySet {
        size_t count;
        size_t end;
        struct changedEntity entities[MAX_ENTITIES];
};

struct world {
        struct player *entities;
        size_t num_players;
        size_t lowest_free_player_slot;

        struct changedEntitySet changed_entities;
};

////////////////////////////////////////////////////////////////////////////////

static struct world world;
static ENetHost *server = NULL;

////////////////////////////////////////////////////////////////////////////////

static void player_init(struct player *const player, ENetAddress addr) {
        player->init = true;
        
        player->address = addr;

        player->position = GLMS_VEC3_ZERO;
        player->rotation = 0;

        player->jumping = false;
        player->falling = false;
        player->airtime = 0;
}

static void player_deinit(struct player *const player) {
        player->init = false;
}

////////////////////////////////////////////////////////////////////////////////

static void changedEntitySet_init(struct changedEntitySet *const set) {
        set->count = 0;
        set->end = 0;
}

static void changedEntitySet_add(struct changedEntitySet *const set,
                                 struct player *const entity) {
        size_t idx = 0;
        while (idx < MAX_ENTITIES && set->entities[idx].init) {
                if (entity->idx < set->entities[idx].entity->idx) {
                        idx = idx*2 + 1;
                } else if (entity->idx > set->entities[idx].entity->idx) {
                        idx = idx*2 + 2;
                } else {
                        return; // already added
                }
        }
        if (idx >= MAX_ENTITIES) {
                return;
        }
        set->entities[idx].entity = entity;
        set->entities[idx].init = true;
        set->count++;
        if (idx > set->end) {
                set->end = idx;
        }
}

static inline size_t changedEntitySet_count(const struct changedEntitySet *const set) {
        return set->count;
}

static void changedEntity_iter(const struct changedEntity *array,
                               const size_t start, const size_t end,
                               void(*const func)(const struct player *entity, void *args),
                               void *const args) {
        if (start > end) {
                return;
        }
        
        changedEntity_iter(array, start*2+1, end, func, args);
        func(array[start].entity, args);
        changedEntity_iter(array, start*2+2, end, func, args);
}

static void changedEntitySet_iter(const struct changedEntitySet *const set,
                                  void(*const func)(const struct player *entity, void *args),
                                  void *const args) {
        if (set->count == 0) {
                return;
        }
        
        changedEntity_iter(set->entities, 0, set->end, func, args);
}

static void changedEntitySet_clear(struct changedEntitySet *const set) {
        if (set->count == 0) {
                return;
        }

        for (size_t i=0; i<=set->end; i++) {
                set->entities[i].init = false;
                set->entities[i].entity = NULL;
        }

        set->end = 0;
        set->count = 0;
}

////////////////////////////////////////////////////////////////////////////////

static void world_init(void) {
        world.entities = malloc(MAX_ENTITIES * sizeof(*world.entities));
        for (size_t i=0; i<MAX_ENTITIES; i++) {
                world.entities[i].init = false;
                world.entities[i].idx = i;
        }
        
        world.lowest_free_player_slot = 0;
        world.num_players = 0;
        changedEntitySet_init(&world.changed_entities);
}

static void world_deinit(void) {
        free(world.entities);
}

////////////////////////////////////////////////////////////////////////////////

static void networking_deinit(void);
static void networking_init(unsigned short port) {
        enet_initialize();
        atexit(networking_deinit);

        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = port;

        server = enet_host_create(&address, MAX_PLAYERS, NETWORK_CHANNELS_TOTAL, 0, 0);
        if (server == NULL) {
                fprintf(stderr, "could not create server\n");
                exit(EXIT_FAILURE);
        }
}

static void networking_deinit(void) {
        world_deinit();
        if (server != NULL) {
                enet_host_destroy(server);
        }
        enet_deinitialize();
}

////////////////////////////////////////////////////////////////////////////////

static void sendCorrectionPacket(ENetPeer *const client,
                                 const struct player *const player) {
        struct networkPacketPositionCorrection data;
        data.base.type = PACKET_TYPE_POSITION_CORRECTION;
        data.position = player->position;
        data.jumpFall = (uint8_t)player->jumping;
        data.jumpFall |= (uint8_t)((uint8_t)player->falling << 1);

        ENetPacket *packet = enet_packet_create(&data, sizeof(data), 0);
        enet_peer_send(client, NETWORK_CHANNEL_MOVEMENT, packet);
}

static bool validateNewPlayerPosition(struct player *const player,
                                      const vec3s clientPosition, unsigned ping) {
        if (player->position.z > JUMP_HEIGHT) {
                return false;
        }

        vec2s serverPosition2 = glms_vec2(player->position);
        vec2s clientPosition2 = glms_vec2(clientPosition);
        
        vec2s difference = glms_vec2_sub(clientPosition2, serverPosition2);
        float magnitude = glms_vec2_norm(difference);

        double maxTime = ping/1000.0 + PACKET_SEND_RATELIMIT + TICK_PERIOD;
        double tolerance = maxTime * PLAYER_SPEED;
        
        if (magnitude <= tolerance) {
                return true;
        } else {
                fprintf(stderr, "magnitude: %f\ntolerance: %f\n\n", magnitude, tolerance);
                return false;
        }
}

////////////////////////////////////////////////////////////////////////////////

static void onPositionPacket(ENetPeer *const client, struct player *const player,
                             const struct networkPacketPosition *const packet) {
        if (!validateNewPlayerPosition(player, packet->position, client->roundTripTime)) {
                sendCorrectionPacket(client, player);
        } else {
                player->position = packet->position;
                changedEntitySet_add(&world.changed_entities, player);
        }
}
static void onRotationPacket(struct player *const player,
                             const struct networkPacketRotation *const packet) {
        player->rotation = packet->rotation;
}
static void onJumpPacket(ENetPeer *const client, struct player *const player) {
        if (player->jumping || player->falling) {
                sendCorrectionPacket(client, player);
        } else {
                player->jumping = true;
                player->falling = false;
                player->airtime = 0;
                changedEntitySet_add(&world.changed_entities, player);
        }
}

////////////////////////////////////////////////////////////////////////////////

static void onControlPacket(ENetPeer *client, struct player *const player,
                            const void *const data) {
        (void)client;
        (void)player;
        (void)data;
}
static void onMovementPacket(ENetPeer *const client, struct player *const player,
                             const struct networkPacket *const packet) {
        switch (packet->type) {
        case PACKET_TYPE_POSITION_UPDATE:
                onPositionPacket(client, player, (const void*)packet);
                break;
        case PACKET_TYPE_ROTATION_UPDATE:
                onRotationPacket(player, (const void*)packet);
                break;
        case PACKET_TYPE_JUMP_UPDATE:
                onJumpPacket(client, player);
                break;
        default:
                break;
        }
}

////////////////////////////////////////////////////////////////////////////////

static void onNewConnection(const ENetEvent *const event) {
        size_t idx = world.lowest_free_player_slot;
        struct player *player = &world.entities[idx];
        player_init(player, event->peer->address);
        event->peer->data = player;

        do {
                world.lowest_free_player_slot++;
        } while (world.entities[world.lowest_free_player_slot].init);
        world.num_players++;

        size_t size = sizeof(struct networkPacketWelcome) + sizeof(struct networkPacketEntityChange) * (world.num_players-1);
        struct networkPacketWelcome *data = malloc(size);
        
        data->base.type = PACKET_TYPE_WELCOME;
        data->id = (uint16_t)idx;
        data->count = 0;
        for (size_t i=0; i<MAX_PLAYERS; i++) {
                if (i != idx && world.entities[i].init) {
                        data->currentEntities[data->count].idx = (uint16_t)i;
                        data->currentEntities[data->count].position = world.entities[i].position;
                        data->currentEntities[data->count].rotation = world.entities[i].rotation;
                        data->count++;
                        if (data->count >= world.num_players-1) {
                                break;
                        }
                }
        }
        ENetPacket *packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(event->peer, NETWORK_CHANNEL_CONTROL, packet);

        struct networkPacketNewEntity data2;
        data2.base.type = PACKET_TYPE_NEW_ENTITY;
        data2.idx = (uint16_t)idx;
        ENetPacket *packet2 = enet_packet_create(&data2, sizeof(data2), ENET_PACKET_FLAG_RELIABLE);
        enet_host_broadcast(server, NETWORK_CHANNEL_SERVER_UPDATES, packet2);
}
static void onDisconnection(const ENetEvent *const event) {
        struct player *player = event->peer->data;
        size_t idx = player->idx;
        if (player == NULL) {
                return;
        }

        if (idx < world.lowest_free_player_slot) {
                world.lowest_free_player_slot = player->idx;
        }
        world.num_players--;
        
        player_deinit(player);
        event->peer->data = NULL;

        struct networkPacketDelEntity data;
        data.base.type = PACKET_TYPE_DEL_ENTITY;
        data.idx = (uint16_t)idx;
        ENetPacket *packet = enet_packet_create(&data, sizeof(data), ENET_PACKET_FLAG_RELIABLE);
        enet_host_broadcast(server, NETWORK_CHANNEL_SERVER_UPDATES, packet);
}
static void onReceived(const ENetEvent *const event) {
        struct player *player = event->peer->data;
        if (player == NULL) {
                return;
        }

        switch (event->channelID) {
        case NETWORK_CHANNEL_CONTROL:
                onControlPacket(event->peer, player, (void*)event->packet->data);
                break;
        case NETWORK_CHANNEL_MOVEMENT:
                onMovementPacket(event->peer, player, (void*)event->packet->data);
                break;
        case NETWORK_CHANNEL_SERVER_UPDATES:
        default:
                fprintf(stderr, "ignoring packet received on channel %u\n", event->channelID);
                break;
        }

        enet_packet_destroy(event->packet);
}

////////////////////////////////////////////////////////////////////////////////

static void poll_events(unsigned time_passed) {
        unsigned long period_ns;
        if (time_passed > TICK_PERIOD_NS) {
                period_ns = 0;
        }
        period_ns = TICK_PERIOD_NS - time_passed;
        unsigned period = (unsigned)(period_ns/1000000UL);
        
        struct timespec t1 = monotonic();
        
        ENetEvent event;
        while (enet_host_service(server, &event, period) > 0) {
                switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                        onNewConnection(&event);
                        break;
                case ENET_EVENT_TYPE_DISCONNECT:
                        onDisconnection(&event);
                        break;
                case ENET_EVENT_TYPE_RECEIVE:
                        onReceived(&event);
                        break;
                case ENET_EVENT_TYPE_NONE:
                default:
                        break;
                }

                struct timespec t2 = monotonic();
                unsigned long t = monotonic_difference(t2, t1)/1000000UL;
                if (t >= period) {
                        period = 0;
                } else {
                        period -= (unsigned)t;
                }
                t1 = t2;
        }
}

////////////////////////////////////////////////////////////////////////////////

static void step_player_jump(struct player *const player) {
        if (!player->jumping && !player->falling) {
                player->position.z = 0;
                return;
        }
        
        player->airtime += TICK_PERIOD;
        player->position.z = jump_fall_animation(&player->jumping,
                                                 &player->falling,
                                                 player->airtime);

        changedEntitySet_add(&world.changed_entities, player);
}

////////////////////////////////////////////////////////////////////////////////

static void step_player(struct player *const player) {
        step_player_jump(player);
        //fprintf(stderr, "player %lu - pos:%f,%f,%f - rot:%f\n", player->idx, player->position.x, player->position.y, player->position.z, player->orientation);
}

////////////////////////////////////////////////////////////////////////////////

static void add_entity_change(const struct player *const entity, void *args) {
        struct networkPacketEntityChangesUpdate *data = args;
        size_t i = data->count;
        struct networkPacketEntityChange *nwEntity = &data->entities[i];
        nwEntity->idx = (uint16_t)entity->idx;
        nwEntity->position = entity->position;
        nwEntity->rotation = entity->rotation;
        data->count += 1;
}

static void broadcast_changes(void) {
        struct networkPacketEntityChangesUpdate *data;
        size_t count = changedEntitySet_count(&world.changed_entities);
        if (count == 0) {
                return;
        }
        
        size_t size = sizeof(*data) + count * sizeof(struct networkPacketEntityChange);
        data = malloc(size);

        data->base.type = PACKET_TYPE_ENTITY_CHANGES_UPDATE;
        data->count = 0;
        changedEntitySet_iter(&world.changed_entities, add_entity_change, data);

        ENetPacket *packet = enet_packet_create(data, size, 0);
        enet_host_broadcast(server, NETWORK_CHANNEL_SERVER_UPDATES, packet);
        
        changedEntitySet_clear(&world.changed_entities);
}

////////////////////////////////////////////////////////////////////////////////

static void step_world(void) {
        for (size_t i=0; i<MAX_PLAYERS; i++) {
                if (world.entities[i].init) {
                        step_player(&world.entities[i]);
                }
        }
}

int main(int argc, char *argv[]) {
        if (argc != 2) {
                fprintf(stderr, "usage:\n\t%s port\n", argv[0]);
                return 1;
        }
        const unsigned short port = (unsigned short)atoi(argv[1]);
        
        world_init();
        networking_init(port);

        printf("Server start.\n");
        for (;;) {
                struct timespec t1 = monotonic();
                step_world();
                broadcast_changes();
                struct timespec t2 = monotonic();
                
                poll_events((unsigned)monotonic_difference(t2, t1)/1000000);
        }

}
