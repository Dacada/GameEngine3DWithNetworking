// TODO: UI scaling: GLFW allows to retrieve a DPI scale thing that should be used to scale the UI so it looks good always
// TODO: raw mouse position, enable when controlling the camera, disable when controlling the cursor
// TODO: glfwWaitEvents vs glfwPollEvents
// TODO: glfwSwapInterval

#define _POSIX_C_SOURCE 200112L


#include <playerController.h>
#include <networkController.h>
#include <sceneController.h>
#include <uiController.h>
#include <events.h>
#include <thirty/game.h>
#include <thirty/util.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define TITLE_BUFFER_SIZE 256

static void processKeyboardEvent(void *registerArgs, void *fireArgs) {
        static bool playingQ = false;
        static bool playingE = false;
        
        struct game *game = registerArgs;
        struct eventBrokerKeyboardEvent *args = fireArgs;
        
        if (!game->inScene) {
                return;
        }
        
        const int key = args->key;
        const int action = args->action;
        //const int modifiers = args->modifiers;
                        
        if (action == GLFW_PRESS) {
                if (key == GLFW_KEY_E || key == GLFW_KEY_Q) {
                        struct scene *scene = game_getCurrentScene(game);
                        size_t idx = scene_idxByName(scene, "SnekSkin");
                        struct object *object = scene_getObjectFromIdx(scene, idx);
                        struct animationCollection *animationCollection = object_getComponent(object, COMPONENT_ANIMATIONCOLLECTION);
                        if (key == GLFW_KEY_Q) {
                                if (playingQ) {
                                        animationCollection_poseAnimation(animationCollection, 0, 0);
                                        playingQ = false;
                                } else {
                                        animationCollection_playAnimation(animationCollection, 0);
                                        playingQ = true;
                                }
                        } else {
                                if (playingE) {
                                        animationCollection_poseAnimation(animationCollection, 1, 0);
                                        playingE = false;
                                } else {
                                        animationCollection_playAnimation(animationCollection, 1);
                                        playingE = true;
                                }
                        }
                }
        }
}

static void setSceneSkybox(struct scene *scene, void *args) {
        char *name = args;
        scene_setSkybox(scene, name);
}

int main(void) {
        // Initialize game
        struct game *game = smalloc(sizeof(struct game));
        game_init(game, SCREEN_WIDTH, SCREEN_HEIGHT,
                  EVENT_TOTAL-EVENT_BROKER_EVENTS_TOTAL, 1);
        game_updateWindowTitle(game, "Title goes here :)");

        // Read scene from file
        size_t sceneIdx;
        {
                struct scene *scene = game_createScene(game);
                scene_initFromFile(scene, game, "scenes/scene.bgl");
                sceneIdx = scene->idx;
                scene_addLoadingStep(scene, setSceneSkybox, sstrdup("skybox"));
        }
        game_unsetCurrentScene(game);

        // Setup player controller
        struct playerController *playerController = smalloc(sizeof(struct playerController));
        playerController_setup(playerController, game, "Camera", "PlayerCharacter");

        // Setup network controller
        struct networkController *networkController = smalloc(sizeof(struct networkController));
        networkController_setup(networkController, game);

        // Setup entity controller
        struct entityController *entityController = smalloc(sizeof(struct entityController));
        entityController_setup(entityController, game, "PlayerCharacter");

        // Setup scene controller
        struct sceneController *sceneController = smalloc(sizeof(struct sceneController));
        sceneController_setup(sceneController, game, sceneIdx);

        // Setup ui controller
        struct uiController *uiController = smalloc(sizeof(struct uiController));
        uiController_setup(uiController, game, networkController);

        // Register events
        eventBroker_register(processKeyboardEvent, EVENT_BROKER_PRIORITY_HIGH,
                             EVENT_BROKER_KEYBOARD_EVENT, game);

        // Main loop
        game_run(game);

        game_free(game);

        free(playerController);
        free(networkController);
        free(entityController);
        free(sceneController);
        free(uiController);
        free(game);
        
        return EXIT_SUCCESS;
}
