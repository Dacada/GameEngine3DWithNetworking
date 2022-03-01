#ifndef ENTITY_CONTROLLER_H
#define ENTITY_CONTROLLER_H

#include <thirty/game.h>
#include <entityUtils.h>
#include <stdbool.h>

struct networkEntity {
        bool init;
        size_t localIdx;
};

struct entityController {
        struct game *game;
        struct networkEntity entities[MAX_ENTITIES];
};

void entityController_setup(struct entityController *const controller, struct game *const game)
        __attribute__((access (read_only, 1)))
        __attribute__((access (read_write, 2)))
        __attribute__((nonnull));

#endif /* ENTITY_CONTROLLER_H */
