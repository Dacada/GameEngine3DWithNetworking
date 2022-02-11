#ifndef EVENTS_H
#define EVENTS_H

#include <cglm/struct.h>
#include <thirty/eventBroker.h>

enum event {
        EVENT_PLAYER_JUMPED = EVENT_BROKER_EVENTS_TOTAL,
        EVENT_PLAYER_POSITION_NEEDS_UPDATE,
};

struct eventPlayerJumped {
        vec3s position;
};
struct eventPlayerPositionUpdate {
        vec3s position;
};

#endif
