#ifndef NETWORK_CONTROLLER_H
#define NETWORK_CONTROLLER_H

#include <events.h>
#include <thirty/game.h>

#define SERVER_HOST "localhost"
#define SERVER_PORT 8192

struct networkController {
        struct game *game;
        bool connected;
        unsigned id;
        struct growingArray otherPlayers;
};

struct otherPlayer {
        unsigned id;
        vec3s position;
        vec2s direction;
        float orientation;
        float airtime;
        bool jumping;
        bool falling;
};

struct __attribute__((packed)) networkedPlayerStatus {
        uint32_t id;
        vec3s position;
        vec2s direction;
        float orientation;
        float airtime;
        uint8_t jumpingFalling;
};

enum packetType {
        PACKET_TYPE_DIRECTION,
        PACKET_TYPE_ROTATION,
        PACKET_TYPE_JUMP,
        PACKET_TYPE_POSITION_CORRECTION,
        PACKET_TYPE_PLAYER_ID,
};


struct __attribute__((packed)) networkPacket {
        uint8_t type;
};

struct __attribute__((packed)) networkPacketPlayerId {
        struct networkPacket base;
        uint32_t id;
        uint16_t num_players;
        struct networkedPlayerStatus players[];
};

struct __attribute__((packed)) networkPacketPositionCorrection {
        struct networkPacket base;
        vec3s position;
};

struct __attribute__((packed)) networkPacketMovement {
        struct networkPacket base;
        vec3s position;
        float orientation;
};

struct __attribute__((packed)) networkPacketDirection {
        struct networkPacketMovement base;
        uint8_t direction;
};

struct __attribute__((packed)) networkPacketRotation {
        struct networkPacketMovement base;
};

struct __attribute__((packed)) networkPacketJump {
        struct networkPacketMovement base;
};

void networkController_setup(struct networkController *controller, struct game *game)
        __attribute__((access (write_only, 1)))
        __attribute__((access (read_only, 2)))
        __attribute__((nonnull));

void networkController_unsetup(struct networkController *controller)
        __attribute__((access (read_write, 1)))
        __attribute__((nonnull));

#endif /* NETWORK_CONTROLLER_H */
