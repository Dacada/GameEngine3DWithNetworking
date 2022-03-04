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
        
        if (game_inMainMenu(game)) {
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

int main(void) {
        const vec4s white = GLMS_VEC4_ONE_INIT;

        // Initialize game
        struct game *game = smalloc(sizeof(struct game));
        game_init(game, SCREEN_WIDTH, SCREEN_HEIGHT,
                  EVENT_TOTAL-EVENT_BROKER_EVENTS_TOTAL, 1);
        game_updateWindowTitle(game, "Title goes here :)");

        // Create initial empty scene
        size_t emptySceneIdx;
        {
                struct scene *emptyScene = game_createScene(game);
                scene_init(emptyScene, game, white, 1);
                emptySceneIdx = emptyScene->idx;
                struct object *camera = scene_createObject(emptyScene, "Camera", 0);
                struct component *cameraComp = componentCollection_create(game, COMPONENT_CAMERA);
                object_setComponent(camera, cameraComp);
                camera_init((struct camera*)cameraComp, "CameraComponent",
                            800.0f/600.0f, 0.1f, 100.0f, glm_rad(45), true,
                            COMPONENT_CAMERA);
        }

        // Read scene from file
        size_t sceneIdx;
        {
                struct scene *scene = game_createScene(game);
                FILE *f = fopen("scenes/scene.bgl", "r");
                scene_initFromFile(scene, game, f);
                scene_setSkybox(scene, "skybox");
                sceneIdx = scene->idx;
        }
        
        game_setCurrentScene(game, emptySceneIdx);
        game_setMainMenuScene(game, emptySceneIdx);

        // Setup player controller
        struct playerController *playerController = smalloc(sizeof(struct playerController));
        {
                struct scene *scene = game_getSceneFromIdx(game, sceneIdx);
                size_t cameraIdx = scene_idxByName(scene, "Camera");
                size_t playerIdx = scene_idxByName(scene, "PlayerCharacter");
                playerController_setup(playerController, game, cameraIdx, playerIdx);
        }

        // Setup network controller
        struct networkController *networkController = smalloc(sizeof(struct networkController));
        {
                struct scene *scene = game_getSceneFromIdx(game, sceneIdx);
                networkController_setup(networkController, game, scene->idx);
        }

        // Setup entity controller
        struct entityController *entityController = smalloc(sizeof(struct entityController));
        {
                struct scene *scene = game_getSceneFromIdx(game, sceneIdx);
                size_t playerIdx = scene_idxByName(scene, "PlayerCharacter");
                struct object *player = scene_getObjectFromIdx(scene, playerIdx);
                struct component *playerGeometry = object_getComponent(player, COMPONENT_GEOMETRY);
                struct component *playerMaterial = object_getComponent(player, COMPONENT_MATERIAL);
                entityController_setup(entityController, game, playerGeometry, playerMaterial);
        }

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
