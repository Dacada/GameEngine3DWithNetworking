#include <playerController.h>
#include <thirty/util.h>

static const float movement_speed = 10.0F;
static const float spin_speed = 2.0F;
static const float look_sensitivity = 0.1F;
static const float camera_distance_min = 2.0F;
static const float camera_distance_max = 10.0F;
static const float camera_distance_default = 4.0F;
static const float camera_zoom_animation_rate = 4.0F;
static const float camera_zoom_animation_stop = 1e-3f;
static const float camera_pos_yaw_behind = GLM_PIf;
static const float camera_pos_yaw_animation_stop = 1e-3f;
static const float camera_pos_yaw_animation_rate = 8.0F;
static const float camera_pos_pitch_initial = 1.25f;

static inline void update_cursor_position(struct playerController *controller) {
        ui_setQuadPosition(game_getCurrentUi(controller->game), controller->cursor_idx,
                           controller->cursorPosition.x, controller->cursorPosition.y,
                           controller->cursorPosition.x+controller->cursorDimensions.x,
                           controller->cursorPosition.y+controller->cursorDimensions.y, 1.0F);
}

static inline void update_cursor_visibility(struct playerController *controller, bool visible) {
        ui_setQuadVisibility(game_getCurrentUi(controller->game), controller->cursor_idx, visible);
}

static inline float normalize_yaw(const float angle) {
        if (angle < 0) {
                return angle + 2*GLM_PIf;
        } else if (angle > 2*GLM_PIf) {
                return angle - 2*GLM_PIf;
        } else {
                return angle;
        }
}

static inline mat4s getCameraModel(struct sphericalCoord coord) {
        // TODO: this can be optimized
        
        vec3s pos = {
                .x=coord.distance*sinf(coord.pitch)*cosf(coord.yaw),
                .y=coord.distance*sinf(coord.pitch)*sinf(coord.yaw),
                .z=coord.distance*cosf(coord.pitch)
        };
        
        return glms_rotate_z(glms_rotate_y(glms_rotate_z(glms_translate(GLMS_MAT4_IDENTITY, pos), coord.yaw), coord.pitch), GLM_PI_2f);
}

static inline void onMousePositionCursor(struct playerController *controller,
                              struct eventBrokerMousePosition *args) {
        vec2s windowSize = game_getWindowDimensions(controller->game);
        vec2s cursorPosition = {
                .x = glm_clamp((float)args->xpos, 0, windowSize.x),
                .y = glm_clamp((float)args->ypos, 0, windowSize.y),
        };
        game_setCursorPosition(controller->game, cursorPosition);
        controller->cursorPosition = cursorPosition;

        update_cursor_position(controller);
}

static inline void onMousePositionCamera(struct playerController *controller,
                                   struct eventBrokerMousePosition *args) {
        const vec2s current = {
                .x = (float)args->xpos,
                .y = (float)args->ypos,
        };

        struct scene *scene = game_getCurrentScene(controller->game);
        struct object *camera = scene_getObjectFromIdx(scene, controller->camera_idx);
        struct object *pc = scene_getObjectFromIdx(scene, controller->playerCharacter_idx);
        struct transform *camera_trans = object_getComponent(camera, COMPONENT_TRANSFORM);
        struct transform *pc_trans = object_getComponent(pc, COMPONENT_TRANSFORM);

        if (controller->first_camera_look) {
                controller->previous_camera_look_mouse_position = current;
                controller->first_camera_look = false;
                return;
        }

        const vec2s offset = glms_vec2_sub(controller->previous_camera_look_mouse_position, current);

        float yaw_delta = game_timeDelta(controller->game)*look_sensitivity*offset.x;
        switch (controller->camera_mode) {
        case CAMERA_MODE_CURSOR:
                // impossible
                break;
        case CAMERA_MODE_LOOK:
                controller->camera_position.yaw += yaw_delta;
                controller->camera_position.yaw = normalize_yaw(controller->camera_position.yaw);
                break;
        case CAMERA_MODE_CONTROL:
        case CAMERA_MODE_ONEHAND:
                transform_rotateZ(pc_trans, yaw_delta);
                break;
        default:
                assert_fail();
        }

        controller->camera_position.pitch += game_timeDelta(controller->game)*look_sensitivity*offset.y;
        controller->camera_position.pitch = glm_clamp(controller->camera_position.pitch, 0.05f, GLM_PI_2f - 0.05f);
        
        camera_trans->model = getCameraModel(controller->camera_position);
        controller->previous_camera_look_mouse_position = current;
}

static inline bool onAnimateCameraDistance(struct playerController *controller, struct eventBrokerUpdate *args) {
        float diff = controller->camera_distance_goal - controller->camera_position.distance;
        if (ABS(diff) <= camera_zoom_animation_stop) {
                return false;
        }
        
        controller->camera_position.distance += diff * camera_zoom_animation_rate * args->timeDelta;
        if (diff > 0) {
                controller->camera_position.distance = glm_clamp(controller->camera_position.distance, camera_distance_min, controller->camera_distance_goal);
        } else {
                controller->camera_position.distance = glm_clamp(controller->camera_position.distance, controller->camera_distance_goal, camera_distance_max);
        }

        return true;
}

static inline bool onAnimateCameraYaw(struct playerController *controller, struct eventBrokerUpdate *args) {
        if (!controller->camera_yaw_animation_ongoing) {
                return false;
        }

        float diff = camera_pos_yaw_behind - controller->camera_position.yaw;
        if (ABS(diff) <= camera_pos_yaw_animation_stop) {
                controller->camera_yaw_animation_ongoing = false;
                return false;
        }

        controller->camera_position.yaw += diff * camera_pos_yaw_animation_rate * args->timeDelta;
        controller->camera_position.yaw = normalize_yaw(controller->camera_position.yaw);

        return true;
}

static void onUpdate(void *registerArgs, void *fireArgs) {
        struct playerController *controller = registerArgs;
        struct eventBrokerUpdate *args = fireArgs;

        bool runUpdate = false;
        runUpdate |= onAnimateCameraDistance(controller, args);
        runUpdate |= onAnimateCameraYaw(controller, args);

        if (runUpdate) {
                struct scene *scene = game_getCurrentScene(controller->game);
                struct object *camera = scene_getObjectFromIdx(scene, controller->camera_idx);
                struct transform *camera_trans = object_getComponent(camera, COMPONENT_TRANSFORM);
                camera_trans->model = getCameraModel(controller->camera_position);
        }
}

static void onMousePosition(void *registerArgs, void *fireArgs) {
        struct playerController *controller = registerArgs;
        struct eventBrokerMousePosition *args = fireArgs;
                
        switch (controller->camera_mode) {
        case CAMERA_MODE_CURSOR:
                onMousePositionCursor(controller, args);
                break;
        case CAMERA_MODE_LOOK:
        case CAMERA_MODE_CONTROL:
        case CAMERA_MODE_ONEHAND:
                onMousePositionCamera(controller, args);
                break;
        default:
                assert_fail();
        }
}

static void onMouseButton(void *registerArgs, void *fireArgs) {
        struct playerController *controller = registerArgs;
        struct eventBrokerMouseButton *args = fireArgs;

        if (args->action == GLFW_PRESS) {
                if (args->button == GLFW_MOUSE_BUTTON_LEFT) {
                        switch (controller->camera_mode) {
                        case CAMERA_MODE_CURSOR:
                                controller->first_camera_look = true;
                                update_cursor_visibility(controller, false);
                                controller->camera_mode = CAMERA_MODE_LOOK;
                                break;
                        case CAMERA_MODE_LOOK:
                                // should be impossible
                                update_cursor_visibility(controller, false);
                                break;
                        case CAMERA_MODE_CONTROL:
                                // should already be invisible
                                update_cursor_visibility(controller, false);
                                controller->camera_mode = CAMERA_MODE_ONEHAND;
                                break;
                        case CAMERA_MODE_ONEHAND:
                                // should be impossible
                                update_cursor_visibility(controller, false);
                                break;
                        default:
                                assert_fail();
                        }
                } else if (args->button == GLFW_MOUSE_BUTTON_RIGHT) {
                        switch (controller->camera_mode) {
                        case CAMERA_MODE_CURSOR:
                                controller->first_camera_look = true;
                                update_cursor_visibility(controller, false);
                                controller->camera_mode = CAMERA_MODE_CONTROL;
                                break;
                        case CAMERA_MODE_LOOK:
                                // should already be invisible
                                update_cursor_visibility(controller, false);
                                controller->camera_mode = CAMERA_MODE_ONEHAND;
                                break;
                        case CAMERA_MODE_CONTROL:
                                // should be impossible
                                update_cursor_visibility(controller, false);
                                break;
                        case CAMERA_MODE_ONEHAND:
                                // should be impossible
                                update_cursor_visibility(controller, false);
                                break;
                        default:
                                assert_fail();
                        }
                }
        } else {
                if (args->button == GLFW_MOUSE_BUTTON_LEFT) {
                        switch (controller->camera_mode) {
                        case CAMERA_MODE_CURSOR:
                                // should be impossible
                                update_cursor_visibility(controller, true);
                                game_setCursorPosition(controller->game, controller->cursorPosition);
                                break;
                        case CAMERA_MODE_LOOK:
                                update_cursor_visibility(controller, true);
                                game_setCursorPosition(controller->game, controller->cursorPosition);
                                controller->camera_mode = CAMERA_MODE_CURSOR;
                                break;
                        case CAMERA_MODE_CONTROL:
                                // should be impossible
                                update_cursor_visibility(controller, true);
                                break;
                        case CAMERA_MODE_ONEHAND:
                                update_cursor_visibility(controller, true);
                                controller->camera_mode = CAMERA_MODE_CONTROL;
                                break;
                        default:
                                assert_fail();
                        }
                } else if (args->button == GLFW_MOUSE_BUTTON_RIGHT) {
                        switch (controller->camera_mode) {
                        case CAMERA_MODE_CURSOR:
                                // should be impossible
                                update_cursor_visibility(controller, true);
                                game_setCursorPosition(controller->game, controller->cursorPosition);
                                break;
                        case CAMERA_MODE_LOOK:
                                // should be impossible
                                update_cursor_visibility(controller, false);
                                break;
                        case CAMERA_MODE_CONTROL:
                                update_cursor_visibility(controller, true);
                                game_setCursorPosition(controller->game, controller->cursorPosition);
                                controller->camera_mode = CAMERA_MODE_CURSOR;
                                break;
                        case CAMERA_MODE_ONEHAND:
                                update_cursor_visibility(controller, false);
                                controller->camera_mode = CAMERA_MODE_LOOK;
                                break;
                        default:
                                assert_fail();
                        }
                }
        }
}

static void onMouseScroll(void *registerArgs, void *fireArgs) {
        struct playerController *controller = registerArgs;
        struct eventBrokerMouseScroll *args = fireArgs;
        controller->camera_distance_goal = glm_clamp(controller->camera_distance_goal - (float)args->amount, camera_distance_min, camera_distance_max);
}

static void onMousePoll(void *registerArgs, void *fireArgs) {
        (void)fireArgs;
        struct playerController *controller = registerArgs;
        struct scene *scene = game_getCurrentScene(controller->game);
        struct object *pc = scene_getObjectFromIdx(scene, controller->playerCharacter_idx);
        struct transform *trans = object_getComponent(pc, COMPONENT_TRANSFORM);

        controller->camera_yaw_animation_ongoing = false;
        if (game_mouseButtonPressed(controller->game, GLFW_MOUSE_BUTTON_LEFT) &&
            game_mouseButtonPressed(controller->game, GLFW_MOUSE_BUTTON_RIGHT)) {
                transform_translateX(trans, game_timeDelta(controller->game)*movement_speed);
                controller->camera_yaw_animation_ongoing = true;
        }
}

static void onKeyboardPoll(void *registerArgs, void *fireArgs) {
        (void)fireArgs;
        struct playerController *controller = registerArgs;
        struct scene *scene = game_getCurrentScene(controller->game);
        struct object *pc = scene_getObjectFromIdx(scene, controller->playerCharacter_idx);
        struct transform *trans = object_getComponent(pc, COMPONENT_TRANSFORM);

        bool key_pressed = false;
        if (game_keyPressed(controller->game, GLFW_KEY_LEFT) ||
            game_keyPressed(controller->game, GLFW_KEY_A)) {
                switch (controller->camera_mode) {
                case CAMERA_MODE_CURSOR:
                case CAMERA_MODE_LOOK:
                        transform_rotateZ(trans, game_timeDelta(controller->game)*spin_speed);
                        break;
                case CAMERA_MODE_CONTROL:
                case CAMERA_MODE_ONEHAND:
                        transform_translateY(trans, game_timeDelta(controller->game)*movement_speed);
                        break;
                default:
                        assert_fail();
                }
                key_pressed = true;
        }
        if (game_keyPressed(controller->game, GLFW_KEY_RIGHT) ||
            game_keyPressed(controller->game, GLFW_KEY_D)) {
                switch (controller->camera_mode) {
                case CAMERA_MODE_CURSOR:
                case CAMERA_MODE_LOOK:
                        transform_rotateZ(trans, -game_timeDelta(controller->game)*spin_speed);
                        break;
                case CAMERA_MODE_CONTROL:
                case CAMERA_MODE_ONEHAND:
                        transform_translateY(trans, -game_timeDelta(controller->game)*movement_speed);
                        break;
                default:
                        assert_fail();
                }
                key_pressed = true;
        }
        if (game_keyPressed(controller->game, GLFW_KEY_UP) ||
            game_keyPressed(controller->game, GLFW_KEY_W)) {
                transform_translateX(trans, game_timeDelta(controller->game)*movement_speed);
                key_pressed = true;
        }
        if (game_keyPressed(controller->game, GLFW_KEY_DOWN) ||
            game_keyPressed(controller->game, GLFW_KEY_S)) {
                transform_translateX(trans, -game_timeDelta(controller->game)*movement_speed);
                key_pressed = true;
        }
        
        controller->camera_yaw_animation_ongoing = false;
        if (key_pressed && controller->camera_mode == CAMERA_MODE_CURSOR) {
                controller->camera_yaw_animation_ongoing = true;
        }
}


void playerController_setup(struct playerController *controller, const struct object *const camera, vec2s cursorDimensions, size_t cursor_idx) {
        controller->game = camera->game;
        
        controller->camera_idx = camera->idx;
        controller->playerCharacter_idx = camera->parent;
        controller->cursor_idx = cursor_idx;
        
        controller->cursorDimensions = cursorDimensions;
        controller->cursorPosition = glms_vec2_scale(game_getWindowDimensions(camera->game), 0.5);

        controller->camera_position.distance = camera_distance_default;
        controller->camera_position.yaw = camera_pos_yaw_behind;
        controller->camera_position.pitch = camera_pos_pitch_initial;
        controller->camera_distance_goal = camera_distance_default;
        controller->camera_yaw_animation_ongoing = false;
        
        controller->first_camera_look = true;
        
        controller->camera_mode = CAMERA_MODE_CURSOR;

        
        eventBroker_register(onUpdate, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_UPDATE, controller);

        eventBroker_register(onMousePosition, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_MOUSE_POSITION, controller);
        eventBroker_register(onMouseButton, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_MOUSE_BUTTON, controller);
        eventBroker_register(onMouseScroll, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_MOUSE_SCROLL, controller);
        eventBroker_register(onMousePoll, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_MOUSE_POLL, controller);

        eventBroker_register(onKeyboardPoll, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_MOUSE_POLL, controller);

        update_cursor_position(controller);
        update_cursor_visibility(controller, true);

        struct transform *camera_trans = object_getComponent(camera, COMPONENT_TRANSFORM);
        camera_trans->model = getCameraModel(controller->camera_position);
}
