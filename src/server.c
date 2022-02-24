#define _GNU_SOURCE

#include <networkController.h>
#include <curve.h>
#include <enet/enet.h>
#include <cglm/struct.h>
#include <time.h>
#include <errno.h>

#define TICK_PERIOD_MS 100
#define TICK_PERIOD 0.1f
#define MAX_PLAYERS 1024

#ifndef ABS
#define ABS(x) ((x)<0?-(x):(x))
#endif

#include <movementConstants.h>

// TODO: related function in client
static float fsign(unsigned sign) {
        if (sign == 0) {
                return 0.0f;
        } else if (sign == 1) {
                return 1.0f;
        } else {
                return -1.0f;
        }
}

struct player {
        struct player *next;
        struct player *prev;
        ENetAddress address;
        unsigned long idx;

        bool lastReceivedPositionIsValid;
        vec3s position;
        float orientation;
        
        vec2s direction;

        bool jumping;
        bool falling;
        float airtime;
};

static void player_init(struct player *const player, struct player *const prev, ENetAddress addr) {
        player->next = NULL;
        player->prev = prev;
        player->address = addr;

        static unsigned long idx = 0;
        player->idx = idx;
        idx++;

        player->lastReceivedPositionIsValid = true;
        player->position = GLMS_VEC3_ZERO;
        player->orientation = 0;
        
        player->direction = GLMS_VEC2_ZERO;

        player->jumping = false;
        player->falling = false;
        player->airtime = 0;
}

struct world {
        struct player *first;
        size_t num_players;
} world;

static ENetHost *server = NULL;

static struct timespec monotonic(void) {
        struct timespec tp = {0};
        if (clock_gettime(CLOCK_MONOTONIC, &tp) == -1) {
                perror("clock_gettime");
        }
        
        return tp;
}

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

static void init_world(void) {
        world.first = NULL;
        world.num_players = 0;
}

static void deinit_networking(void) {
        if (server != NULL) {
                enet_host_destroy(server);
        }
        enet_deinitialize();
}

static void init_networking(void) {
        enet_initialize();
        atexit(deinit_networking);

        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = SERVER_PORT;

        server = enet_host_create(&address, MAX_PLAYERS, 2, 0, 0);
        if (server == NULL) {
                fprintf(stderr, "could not create server\n");
                exit(EXIT_FAILURE);
        }
}

static void send_client_idx(ENetPeer *const client, unsigned long idx) {
        size_t data_size = sizeof(struct networkPacketPlayerId) + sizeof(struct networkedPlayerStatus)*world.num_players;
        struct networkPacketPlayerId *data = malloc(data_size);
        
        data->base.type = PACKET_TYPE_PLAYER_ID;
        data->id = (uint32_t)idx;
        data->num_players = (uint16_t)world.num_players;

        size_t i=0;
        for (struct player *player = world.first;
             player != NULL;
             player = player->next) {
                struct networkedPlayerStatus *nwPlayer = data->players + i;
                nwPlayer->id = (uint32_t)player->idx;
                nwPlayer->position = player->position;
                nwPlayer->direction = player->direction;
                nwPlayer->orientation = player->orientation;
                nwPlayer->airtime = player->airtime;
                nwPlayer->jumpingFalling = (uint8_t)player->jumping;
                nwPlayer->jumpingFalling |= (uint8_t)(player->falling << 1);
        }
        
        ENetPacket *packet = enet_packet_create(data, data_size, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(client, 0, packet);

        free(data);
}

static void onNewConnection(const ENetEvent *const event) {
        struct player *prev = NULL;
        struct player **ptr = &world.first;
        while (*ptr != NULL) {
                prev = *ptr;
                ptr = &(*ptr)->next;
        }
        *ptr = malloc(sizeof(**ptr));
        player_init(*ptr, prev, event->peer->address);
        event->peer->data = *ptr;
        world.num_players++;
        send_client_idx(event->peer, (*ptr)->idx);
        // TODO: broadcast new connection to all players
}

static void onDisconnection(const ENetEvent *const event) {
        struct player *player = event->peer->data;
        if (player == NULL) {
                return;
        }

        if (player->prev != NULL) {
                player->prev->next = player->next;
        }
        if (player->next != NULL) {
                player->next->prev = player->prev;
        }

        free(player);
        if (world.first == player) {
                world.first = NULL;
        }
        event->peer->data = NULL;
        world.num_players--;
        // TODO: broadcast disconnection to all players
}

static void onControlPacket(ENetPeer *client, struct player *const player, const void *const data) {
        (void)client;
        (void)player;
        (void)data;
}

static void onDirectionPacket(struct player *const player, const struct networkPacketDirection *const packet) {
        vec2s direction;
        direction.x = fsign(packet->direction & 0xF);
        direction.y = fsign((packet->direction & 0xF0) >> 4);
        player->direction = glms_vec2_normalize(direction);
}

static void onRotationPacket(struct player *const player, const struct networkPacketRotation *const packet) {
        (void)player;
        (void)packet;
}

static void onJumpPacket(struct player *const player) {
        if (!player->jumping && !player->falling) {
                player->jumping = true;
                player->airtime = 0;
        }
}

static bool float_eq_delta(const float a, const float b, const float delta) {
        return ABS(a-b) <= delta;
}

static bool validateNewPlayerPosition(const struct player *const player, const struct networkPacketMovement *const packet) {
        if (!float_eq_delta(player->position.x, packet->position.x, movement_speed*TICK_PERIOD)) {
                return false;
        }
        if (!float_eq_delta(player->position.y, packet->position.y, movement_speed*TICK_PERIOD)) {
                return false;
        }
        if (!float_eq_delta(player->position.z, packet->position.z, movement_speed*TICK_PERIOD)) {
                return false;
        }
        return true;
}

static void correct_client_position(ENetPeer *const peer, vec3s pos) {
        struct networkPacketPositionCorrection data;
        data.base.type = PACKET_TYPE_POSITION_CORRECTION;
        data.position = pos;
        ENetPacket *packet = enet_packet_create(&data, sizeof(data), 0);
        enet_peer_send(peer, 1, packet);
}

static void onMovementPacket(ENetPeer *const client, struct player *const player, const struct networkPacketMovement *const packet) {
        player->lastReceivedPositionIsValid = validateNewPlayerPosition(player, packet);
        if (player->lastReceivedPositionIsValid) {
                player->position = packet->position;
        } else {
                correct_client_position(client, player->position);
        }
        player->orientation = packet->orientation;
        
        switch (packet->base.type) {
        case PACKET_TYPE_DIRECTION:
                onDirectionPacket(player, (const void*)packet);
                break;
        case PACKET_TYPE_ROTATION:
                onRotationPacket(player, (const void*)packet);
                break;
        case PACKET_TYPE_JUMP:
                onJumpPacket(player);
                break;
        default:
                break;
        }
}

static void onReceive(const ENetEvent *const event) {
        struct player *player = event->peer->data;
        if (player == NULL) {
                return;
        }

        switch (event->channelID) {
        case 0:
                onControlPacket(event->peer, player, (void*)event->packet->data);
                break;
        case 1:
                onMovementPacket(event->peer, player, (void*)event->packet->data);
                break;
        default:
                break;
        }

        enet_packet_destroy(event->packet);
}

static void poll_events(unsigned timeout) {
        unsigned period;
        if (timeout > TICK_PERIOD_MS) {
                period = 0;
        }
        period = TICK_PERIOD_MS - timeout;
        
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
                        onReceive(&event);
                        break;
                case ENET_EVENT_TYPE_NONE:
                default:
                        break;
                }

                struct timespec t2 = monotonic();
                unsigned long t = monotonic_difference(t2, t1)/1000000;
                if (t >= period) {
                        break;
                }
                period -= (unsigned)t;
                t1 = t2;
        }
}

// TODO: This is copypaste from onAnimatePlayerJumpFall in playerController.c
static void step_player_jump(struct player *const player) {
        if (!player->jumping && !player->falling) {
                return;
        }
        
        player->airtime += TICK_PERIOD;
        float s = player->airtime / jump_time;
        if (s > 1) {
                if (player->jumping) {
                        s -= 1;
                        player->airtime = s * jump_time;
                        player->jumping = false;
                        player->falling = true;
                } else {
                        player->jumping = player->falling = false;
                        player->position.z = 0;
                        return;
                }
        }

        float v;
        if (player->jumping) {
                v = curve_sample(&jump_curve, s);
        } else if (player->falling) {
                v = curve_sample(&fall_curve, s);
        } else {
                v = 0;
        }
        v *= jump_height;
        player->position.z = v;
}

static void step_player_position(struct player *const player) {
        vec2s direction = glms_vec2_scale(
                glms_vec2_rotate(player->direction, player->orientation),
                TICK_PERIOD * movement_speed
                );
        player->position.x += direction.x;
        player->position.y += direction.y;
}

static void step_player(struct player *const player) {
        step_player_jump(player);
        step_player_position(player);
        //fprintf(stderr, "player %lu - pos:%f,%f,%f - rot:%f\n", player->idx, player->position.x, player->position.y, player->position.z, player->orientation);
}

static void step_world(void) {
        for (struct player *player = world.first;
             player != NULL;
             player = player->next) {
                step_player(player);
        }
}

static void broadcast_world_state(void) {
        for (struct player *player = world.first;
             player != NULL;
             player = player->next) {
                // TODO
        }
}

int main(void) {
        init_world();
        init_networking();

        printf("Server start.\n");
        for (;;) {
                struct timespec t1 = monotonic();
                step_world();
                broadcast_world_state();
                struct timespec t2 = monotonic();
                
                poll_events((unsigned)monotonic_difference(t2, t1)/1000000);
        }

}
