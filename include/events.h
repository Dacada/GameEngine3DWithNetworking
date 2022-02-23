#ifndef EVENTS_H
#define EVENTS_H

#include <cglm/struct.h>
#include <thirty/eventBroker.h>

enum event {
        EVENT_PLAYER_DIRECTION_CHANGED = EVENT_BROKER_EVENTS_TOTAL,
        EVENT_PLAYER_ROTATION_CHANGED,
        EVENT_PLAYER_JUMPED,
        EVENT_PLAYER_POSITION_CORRECTED,
        EVENT_TOTAL,
};

struct eventPlayerDirectionChange {
        vec3s position;
        float orientation;
        vec2s direction;
};
struct eventPlayerRotationChange {
        vec3s position;
        float orientation;
};
struct eventPlayerJumped {
        vec3s position;
        float orientation;
};
struct eventPlayerPositionCorrected {
        vec3s position;
};

#endif
