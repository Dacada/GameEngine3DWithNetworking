#ifndef SCENE_CONTROLLER_H
#define SCENE_CONTROLLER_H

#include <thirty/game.h>

struct sceneController {
        struct game *game;
        size_t testSceneIdx;
};

void sceneController_setup(struct sceneController *const controller, struct game *game, size_t testSceneIdx)
        __attribute__((access (write_only, 1)))
        __attribute__((access (read_write, 2)))
        __attribute__((nonnull));

#endif /* SCENE_CONTROLLER_H */
