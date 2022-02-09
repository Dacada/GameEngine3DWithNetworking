// TODO: UI scaling: GLFW allows to retrieve a DPI scale thing that should be used to scale the UI so it looks good always
// TODO: raw mouse position, enable when controlling the camera, disable when controlling the cursor
// TODO: glfwWaitEvents vs glfwPollEvents
// TODO: glfwSwapInterval

#define _POSIX_C_SOURCE 200112L

#include <playerController.h>
#include <thirty/game.h>
#include <thirty/network.h>
#include <thirty/util.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define TITLE_BUFFER_SIZE 256
#define FRAME_PERIOD_FPS_REFRESH 10
#define SERVER_HOST "localhost"
#define SERVER_PORT 8192

static sig_atomic_t last_signal = 0;
static void sigusr_handler(int signal) {
        last_signal = signal;
}

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

static void updateTitle(void *registerArgs, void *fireArgs) {
        struct game *game = registerArgs;
        (void)fireArgs;
        
        static unsigned count = 0;
        if (count == FRAME_PERIOD_FPS_REFRESH) {
                const int fps = (const int)(1.0F/game_timeDelta(game));
                static char title[TITLE_BUFFER_SIZE];
                snprintf(title, TITLE_BUFFER_SIZE, "[%d] Name goes here :)", fps);
                game_updateWindowTitle(game, title);
                count = 0;
        } else {
                count++;
        }
}

static void *network_fun(void *arg) {
        sigset_t set;
        int err;
        
        // block all signals
        sigfillset(&set);
        err = pthread_sigmask(SIG_BLOCK, &set, NULL);
        if (err != 0) {
                errno = err;
                perror("pthread_sigmask");
                die("could not set signal mask");
        }
        
        // but ensure SIGUSR1 and 2 are unblocked
        sigemptyset(&set);
        sigaddset(&set, SIGUSR1);
        sigaddset(&set, SIGUSR2);
        err = pthread_sigmask(SIG_UNBLOCK, &set, NULL);
        if (err != 0) {
                errno = err;
                perror("pthread_sigmask");
                die("could not set signal mask");
        }
        
        struct network *network = arg;
        for (;;) {
                if (!network_run(network)) {
                        static char error[1024];
                        network_retrieveError(network, error, 1024);
                        fprintf(stderr, "%s", error);
                        return NULL;
                } else if (last_signal == SIGUSR1) {
                        return NULL;
                } else if (last_signal == SIGUSR2) {
                        continue;
                }
        }
}

int main(void) {
        {
                sigset_t set;
                sigemptyset(&set);
                
                struct sigaction act = {
                        .sa_handler = sigusr_handler,
                        .sa_mask = set,
                        .sa_flags = 0,
                };
                
                if (sigaction(SIGUSR1, &act, NULL) == -1) {
                        perror("sigaction");
                        return EXIT_FAILURE;
                }
                if (sigaction(SIGUSR2, &act, NULL) == -1) {
                        perror("sigaction");
                        return EXIT_FAILURE;
                }

                // but ensure SIGUSR1 and 2 are blocked
                sigaddset(&set, SIGUSR1);
                sigaddset(&set, SIGUSR2);
                int err = pthread_sigmask(SIG_BLOCK, &set, NULL);
                if (err != 0) {
                        errno = err;
                        perror("pthread_sigmask");
                        die("could not set signal mask");
                }
        }
        
        static const vec4s black = GLMS_VEC4_BLACK_INIT;

        // Initialize networking
        struct network *network = smalloc(sizeof(struct network));
        network_init(network, false, false, true, true); // struct, use tcp, use udp
        if (!network_connect(network, SERVER_HOST, SERVER_PORT)) {
                static char error[1024];
                network_retrieveError(network, error, 1024);
                fprintf(stderr, "error: %s", error);
                return EXIT_FAILURE;
        }

        // Initialize game
        struct game *game = smalloc(sizeof(struct game));
        game_init(game, SCREEN_WIDTH, SCREEN_HEIGHT, 1, 1);

        // Read scene from file
        struct scene *scene = game_createScene(game);
        FILE *f = fopen("scenes/scene.bgl", "r");
        scene_initFromFile(scene, game, f);
        game_setCurrentScene(game, scene->idx);

        // Create UI
        struct ui *ui = game_createUi(game);
        ui_init(ui, SCREEN_WIDTH, SCREEN_HEIGHT);
        ui_addQuad(ui, 10,10, 200,60, 0.0F, "ui_test_texture");
        struct font *font = ui_getFont("CutiveMono-Regular", 24, "latin-1");
        font_load(font);
        ui_addText(ui, 35,45, 0.1F, (const unsigned char*)"Something.", font, black);
        game_setCurrentUi(game, ui->idx);

        // Create cursor (player controller will set dimensions correctly)
        size_t cursor_idx = ui_addQuad(ui, 0, 0, 1, 1, 0.0F, "cursor");

        // Setup player controller
        struct playerController *playerController = smalloc(sizeof(struct playerController));
        size_t camera_idx = scene_idxByName(scene, "Camera");
        struct object *camera = scene_getObjectFromIdx(scene, camera_idx);
        playerController_setup(playerController, camera, (vec2s){.x=12, .y=17}, cursor_idx);

        // Setup network controller
        /* struct networkController *networkController = smalloc(sizeof(struct networkController)); */
        /* networkController_setup(networkController, game); */

        // Add a skybox
        scene_setSkybox(scene, "skybox");

        // Register events
        eventBroker_register(processKeyboardEvent, EVENT_BROKER_PRIORITY_HIGH,
                             EVENT_BROKER_KEYBOARD_EVENT, game);
        eventBroker_register(updateTitle, EVENT_BROKER_PRIORITY_HIGH,
                             EVENT_BROKER_UPDATE, game);

        // Start networking thread
        int ret;
        pthread_t network_thread;
        if ((ret = pthread_create(&network_thread, NULL, network_fun, network)) != 0) {
                errno = ret;
                perror("pthread_create");
        }

        // Main loop
        game_run(game);

        // Kill networking thread
        if ((ret = pthread_kill(network_thread, SIGUSR1)) != 0) {
                errno = ret;
                perror("pthread_kill");
        }
        pthread_join(network_thread, NULL);
        network_destroy(network);
        game_free(game);

        free(playerController);
        /* free(networkController); */
        free(network);
        free(game);
        
        return EXIT_SUCCESS;
}
