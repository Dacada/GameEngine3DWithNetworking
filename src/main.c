#include <thirty/inputHelpers.h>
#include <thirty/game.h>
#include <thirty/util.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define TITLE_BUFFER_SIZE 256
#define FRAME_PERIOD_FPS_REFRESH 10
#define CAMERA_PITCH_LIMIT_OFFSET 0.1F
#define CAMERA_NEAR 0.1F
#define CAMERA_FAR 100.0F
#define CAMERA_FOV 45.0F

static const float movement_speed = 10.0F;
static const float look_sensitivity = 0.1F;

static size_t camera_idx = 0;
static struct fpsCameraController cam_ctrl;

static void processMouseInput(void *registerArgs, void *fireArgs) {
        struct game *game = registerArgs;
        struct eventBrokerMousePosition *args = fireArgs;
        const double xpos = args->xpos;
        const double ypos = args->ypos;

        struct scene *scene = game_getCurrentScene(game);
        struct object *camera = scene_getObjectFromIdx(scene, camera_idx);
        struct camera_fps *cameracomp = object_getComponent(camera, COMPONENT_CAMERA);
        
        const vec2s curr = {
                .x = (float)xpos,
                .y = (float)ypos,
        };

        static bool first = true;
        static vec2s prev;
        if (first) {
                prev = curr;
                first = false;
        }

        const vec2s offset = glms_vec2_sub(prev, curr);
        
        vec2s yaw_pitch = fpsCameraController_look(&cam_ctrl, offset, game_timeDelta(game), camera);
        cameracomp->yaw = yaw_pitch.x;
        cameracomp->pitch = yaw_pitch.y;

        prev = curr;
}

static void processKeyboardInput(void *registerArgs, void *fireArgs) {
        struct game *game = registerArgs;
        (void)fireArgs;
        
        vec2s movement = {.x=0, .y=0};
        
        if (game_keyPressed(game, GLFW_KEY_LEFT) ||
            game_keyPressed(game, GLFW_KEY_A)) {
                movement.x -= 1;
        }
        if (game_keyPressed(game, GLFW_KEY_RIGHT) ||
            game_keyPressed(game, GLFW_KEY_D)) {
                movement.x += 1;
        }
        
        if (game_keyPressed(game, GLFW_KEY_UP) ||
            game_keyPressed(game, GLFW_KEY_W)) {
                movement.y += 1;
        }
        if (game_keyPressed(game, GLFW_KEY_DOWN) ||
            game_keyPressed(game, GLFW_KEY_S)) {
                movement.y -= 1;
        }
        
        movement = glms_vec2_normalize(movement);

        struct scene *scene = game_getCurrentScene(game);
        struct object *camera = scene_getObjectFromIdx(scene, camera_idx);
        struct camera_fps *cameracomp = object_getComponent(camera, COMPONENT_CAMERA);
        cameracomp->position = fpsCameraController_move(&cam_ctrl, movement, game_timeDelta(game), camera);
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
                } else if (key == GLFW_KEY_F) {
                        cam_ctrl.freefly = !cam_ctrl.freefly;
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
                snprintf(title, TITLE_BUFFER_SIZE, "[%d] Physics (%s)", fps,
                         cam_ctrl.freefly ?
                         "Freefly camera" :
                         "FPS camera");
                game_updateWindowTitle(game, title);
                count = 0;
        } else {
                count++;
        }
}

static void freeGame(void *registerArgs, void *fireArgs) {
        struct game *game = registerArgs;
        (void)fireArgs;

        game_free(game);
        free(game);
}

int main(void) {
        static const vec4s black = GLMS_VEC4_BLACK_INIT;

        // Initialize game
        struct game *game = smalloc(sizeof(struct game));
        game_init(game, SCREEN_WIDTH, SCREEN_HEIGHT, 1, 1);

        // Read scene from file
        struct scene *scene = game_createScene(game);
        FILE *f = fopen("scenes/scene.bgl", "r");
        scene_initFromFile(scene, game, f);
        game_setCurrentScene(game, scene->idx);

        // Setup camera
        camera_idx = scene_idxByName(scene, "Camera");
        fpsCameraController_init(&cam_ctrl, scene, movement_speed, look_sensitivity);

        // Create UI
        struct ui *ui = game_createUi(game);
        ui_init(ui, SCREEN_WIDTH, SCREEN_HEIGHT);
        ui_addQuad(ui, 10,10, 200,60, 0.0F, "ui_test_texture");
        struct font *font = ui_getFont("CutiveMono-Regular", 24, "latin-1");
        font_load(font);
        ui_addText(ui, 35,45, 0.1F, (const unsigned char*)"Something.", font, black);
        game_setCurrentUi(game, ui->idx);

        scene_setSkybox(scene, "skybox");

        // Register events
        eventBroker_register(processKeyboardInput, EVENT_BROKER_PRIORITY_HIGH,
                             EVENT_BROKER_KEYBOARD_INPUT, game);
        eventBroker_register(processKeyboardEvent, EVENT_BROKER_PRIORITY_HIGH,
                             EVENT_BROKER_KEYBOARD_EVENT, game);
        eventBroker_register(processMouseInput, EVENT_BROKER_PRIORITY_HIGH,
                             EVENT_BROKER_MOUSE_POSITION, game);
        eventBroker_register(updateTitle, EVENT_BROKER_PRIORITY_HIGH,
                             EVENT_BROKER_UPDATE, game);
        eventBroker_register(freeGame, EVENT_BROKER_PRIORITY_HIGH,
                             EVENT_BROKER_TEAR_DOWN, game);

        // Main loop
        game_run(game);
        return 0;
}
