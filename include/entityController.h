#ifndef ENTITY_CONTROLLER_H
#define ENTITY_CONTROLLER_H

#include <stdbool.h>
#include <thirty/game.h>
#include <timeutil.h>
#include <entityUtils.h>

struct networkEntity {
        bool init;
        size_t localIdx;

        vec3s prevPos;
        float prevRot;

        vec3s nextPos;
        float nextRot;

        struct timespec lastUpdate;
};

struct entityController {
        struct game *game;
        struct networkEntity entities[MAX_ENTITIES];
        size_t numEntities;
        
        struct component *geometry;
        struct component *material;
};

void entityController_setup(struct entityController *const controller, struct game *const game, struct component *const playerGeometry, struct component *const playerMaterial)
        __attribute__((access (read_only, 1)))
        __attribute__((access (read_write, 2)))
        __attribute__((nonnull));

#endif /* ENTITY_CONTROLLER_H */
