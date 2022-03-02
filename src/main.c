// TODO: UI scaling: GLFW allows to retrieve a DPI scale thing that should be used to scale the UI so it looks good always
// TODO: raw mouse position, enable when controlling the camera, disable when controlling the cursor
// TODO: glfwWaitEvents vs glfwPollEvents
// TODO: glfwSwapInterval

#define _POSIX_C_SOURCE 200112L

#include <playerController.h>
#include <networkController.h>
#include <events.h>
#include <thirty/game.h>
#include <thirty/util.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define TITLE_BUFFER_SIZE 256
#define FRAME_PERIOD_FPS_REFRESH 10

static void processKeyboardEvent(void *registerArgs, void *fireArgs) {
        static bool playingQ = false;
        static bool playingE = false;
        
        struct game *game = registerArgs;
        struct eventBrokerKeyboardEvent *args = fireArgs;
        
        const int key = args->key;
        const int action = args->action;
        //const int modifiers = args->modifiers;
                        
        if (action == GLFW_PRESS) {
                if (key == GLFW_KEY_ESCAPE) {
                        game_disconnect(game, 0);
                        game_shouldStop(game);
                } else if (key == GLFW_KEY_E || key == GLFW_KEY_Q) {
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

static void updateUI(void *registerArgs, void *fireArgs) {
        struct game *game = registerArgs;
        struct eventBrokerUpdateUI *args = fireArgs;
        struct nk_context *ctx = args->ctx;

        static unsigned fps;
        {
                static float deltas = 0;
                static unsigned count = 0;
                deltas += ctx->delta_time_seconds;

                count++;
                if (count >= FRAME_PERIOD_FPS_REFRESH) {
                        float avgDelta = deltas/FRAME_PERIOD_FPS_REFRESH;
                        fps = (unsigned)(1/avgDelta);
                        
                        deltas = 0;
                        count = 0;
                }
        }
        char fpsText[256];
        snprintf(fpsText, 256, "fps: %u", fps);

        char pingText[256];
        if (game->server != NULL) {
                snprintf(pingText, 256, "ping: %u", game->server->roundTripTime);
        } else {
                snprintf(pingText, 256, "ping: n/a");
        }

        if (nk_begin(ctx, "fps", nk_rect(0, 0, 100, 30), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND | NK_WINDOW_NO_INPUT)) {
                nk_layout_row_static(ctx, 10, 100, 1);
                nk_label(ctx, fpsText, NK_TEXT_LEFT);
                nk_label(ctx, pingText, NK_TEXT_LEFT);
        }
        nk_end(ctx);
        
        /* struct nk_colorf bg; */
        /* bg.r = 0.10f, bg.g = 0.18f, bg.b = 0.24f, bg.a = 1.0f; */
        /* if (nk_begin(ctx, "asdf title!", nk_rect(50,50,230,250), NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)) { */
        /*         enum {EASY,HARD}; */
        /*         static int op = EASY; */
        /*         static int property = 20; */
        /*         nk_layout_row_static(ctx, 30, 80, 1); */
        /*         if (nk_button_label(ctx, "asdf button!")) { */
        /*                 fprintf(stderr, "button pressed!!\n"); */
        /*         } */
        /*         nk_layout_row_dynamic(ctx, 30, 2); */
        /*         if (nk_option_label(ctx, "easy", op == EASY)) op = EASY; */
        /*         if (nk_option_label(ctx, "hard", op == HARD)) op = HARD; */
        /*         nk_layout_row_dynamic(ctx, 25, 1); */
        /*         nk_property_int(ctx, "Compression:", 0, &property, 100, 10, 1); */
        /*         nk_layout_row_dynamic(ctx, 20, 1); */
        /*         nk_label(ctx, "!b!a!ckground:", NK_TEXT_LEFT); */
        /*         nk_layout_row_dynamic(ctx, 25, 1); */
        /*         if (nk_combo_begin_color(ctx, nk_rgb_cf(bg), nk_vec2(nk_widget_width(ctx),400))) { */
        /*                 nk_layout_row_dynamic(ctx, 120, 1); */
        /*                 bg = nk_color_picker(ctx, bg, NK_RGBA); */
        /*                 nk_layout_row_dynamic(ctx, 25, 1); */
        /*                 bg.r = nk_propertyf(ctx, "#R:", 0, bg.r, 1.0f, 0.01f,0.005f); */
        /*                 bg.g = nk_propertyf(ctx, "#G:", 0, bg.g, 1.0f, 0.01f,0.005f); */
        /*                 bg.b = nk_propertyf(ctx, "#B:", 0, bg.b, 1.0f, 0.01f,0.005f); */
        /*                 bg.a = nk_propertyf(ctx, "#A:", 0, bg.a, 1.0f, 0.01f,0.005f); */
        /*                 nk_combo_end(ctx); */
        /*         } */
        /* } */
        /* nk_end(ctx); */
}

int main(int argc, char *argv[]) {
        if (argc != 3) {
                fprintf(stderr, "usage:\n\t%s host port\n", argv[0]);
                return 1;
        }
        const char *const host = argv[1];
        const unsigned short port = (unsigned short)atoi(argv[2]);

        // Initialize game
        struct game *game = smalloc(sizeof(struct game));
        game_init(game, SCREEN_WIDTH, SCREEN_HEIGHT,
                  EVENT_TOTAL-EVENT_BROKER_EVENTS_TOTAL, 1);
        game_updateWindowTitle(game, "Title goes here :)");

        // Read scene from file
        struct scene *scene = game_createScene(game);
        FILE *f = fopen("scenes/scene.bgl", "r");
        scene_initFromFile(scene, game, f);
        game_setCurrentScene(game, scene->idx);

        // Setup player controller
        struct playerController *playerController = smalloc(sizeof(struct playerController));
        size_t camera_idx = scene_idxByName(scene, "Camera");
        struct object *camera = scene_getObjectFromIdx(scene, camera_idx);
        playerController_setup(playerController, camera);

        // Setup network controller
        struct networkController *networkController = smalloc(sizeof(struct networkController));
        networkController_setup(networkController, game, host, port);

        // Setup entity controller
        struct object *player = scene_getObjectFromIdx(scene, camera->parent);
        struct component *playerGeometry = object_getComponent(player, COMPONENT_GEOMETRY);
        struct component *playerMaterial = object_getComponent(player, COMPONENT_MATERIAL);
        struct entityController *entityController = smalloc(sizeof(struct entityController));
        entityController_setup(entityController, game, playerGeometry, playerMaterial);

        // Add a skybox
        scene_setSkybox(scene, "skybox");

        // Register events
        eventBroker_register(processKeyboardEvent, EVENT_BROKER_PRIORITY_HIGH,
                             EVENT_BROKER_KEYBOARD_EVENT, game);
        eventBroker_register(updateUI, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_UPDATE_UI, game);

        // Main loop
        game_run(game);

        game_free(game);

        free(playerController);
        free(networkController);
        free(entityController);
        free(game);
        
        return EXIT_SUCCESS;
}
