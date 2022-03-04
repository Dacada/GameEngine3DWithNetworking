#ifndef NETWORK_CONTROLLER_H
#define NETWORK_CONTROLLER_H

#include <timeutil.h>
#include <entityController.h>
#include <events.h>
#include <thirty/game.h>

#define PACKET_SEND_RATELIMIT_MS 50
#define PACKET_SEND_RATELIMIT 0.05f

enum networkChannel {
        NETWORK_CHANNEL_CONTROL,
        NETWORK_CHANNEL_MOVEMENT,
        NETWORK_CHANNEL_SERVER_UPDATES,
        NETWORK_CHANNELS_TOTAL,
};

struct networkController {
        struct game *game;
        bool connected;
        unsigned id;
        size_t testMapSceneIdx;

        bool sentPosPacket;
        struct timespec lastTimeSentPosPacket;

        bool sentRotPacket;
        struct timespec lastTimeSentRotPacket;
};

enum packetType {
        PACKET_TYPE_POSITION_UPDATE,
        PACKET_TYPE_ROTATION_UPDATE,
        PACKET_TYPE_JUMP_UPDATE,
        PACKET_TYPE_POSITION_CORRECTION,
        PACKET_TYPE_WELCOME,
        PACKET_TYPE_ENTITY_CHANGES_UPDATE,
        PACKET_TYPE_NEW_ENTITY,
        PACKET_TYPE_DEL_ENTITY,
};

struct __attribute__((packed)) networkPacket {
        uint8_t type;
};

struct __attribute__((packed)) networkPacketEntityChange {
        uint16_t idx;
        vec3s position;
        float rotation;
};

struct __attribute__((packed)) networkPacketWelcome {
        struct networkPacket base;
        uint16_t id;
        uint16_t count;
        struct networkPacketEntityChange currentEntities[];
};

struct __attribute__((packed)) networkPacketPositionCorrection {
        struct networkPacket base;
        vec3s position;
        uint8_t jumpFall;
};

struct __attribute__((packed)) networkPacketPosition {
        struct networkPacket base;
        vec3s position;
};

struct __attribute__((packed)) networkPacketRotation {
        struct networkPacket base;
        float rotation;
};

struct __attribute__((packed)) networkPacketJump {
        struct networkPacket base;
};

struct __attribute__((packed)) networkPacketNewEntity {
        struct networkPacket base;
        uint16_t idx;
        vec3s position;
        float rotation;
};

struct __attribute__((packed)) networkPacketDelEntity {
        struct networkPacket base;
        uint16_t idx;
};

struct __attribute__((packed)) networkPacketEntityChangesUpdate {
        struct networkPacket base;
        uint16_t count;
        struct networkPacketEntityChange entities[];
};

void networkController_setup(struct networkController *controller, struct game *game, const char *host, unsigned short port, size_t testMapSceneIdx)
        __attribute__((access (write_only, 1)))
        __attribute__((access (read_write, 2)))
        __attribute__((access (read_only, 3)))
        __attribute__((nonnull));

#endif /* NETWORK_CONTROLLER_H */
