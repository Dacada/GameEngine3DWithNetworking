#ifndef EVENTS_H
#define EVENTS_H

#include <cglm/struct.h>
#include <thirty/eventBroker.h>

enum event {
        EVENT_PLAYER_POSITION_CHANGED = EVENT_BROKER_EVENTS_TOTAL,
        EVENT_PLAYER_ROTATION_CHANGED,
        EVENT_PLAYER_JUMPED,
        EVENT_SERVER_CONNECTION_SUCCESS,
        EVENT_SERVER_CORRECTED_PLAYER_POSITION,
        EVENT_NETWORK_ENTITY_UPDATE,
        EVENT_NETWORK_ENTITY_NEW,
        EVENT_NETWORK_ENTITY_DEL,
        EVENT_TOTAL,
};

struct eventPlayerPositionChanged {
        vec3s position;
};
struct eventPlayerRotationChanged {
        float rotation;
};
struct eventPlayerJumped {
};
struct eventServerConnectionSuccess {
};
struct eventPlayerPositionCorrected {
        vec3s position;
        bool jumping;
        bool falling;
};
struct eventNetworkEntityUpdate {
        size_t idx;
        vec3s position;
        float rotation;
};
struct eventNetworkEntityNew {
        size_t idx;
        vec3s position;
        float rotation;
};
struct eventNetworkEntityDel {
        size_t idx;
};

#endif
