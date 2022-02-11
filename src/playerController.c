#include <playerController.h>
#include <events.h>
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
static const float movement_vector_zero = 1e-9f;
static const struct curve jump_curve = {
        .p0=0.0f,
        .c0=0.5f,
        .c1=1.0f,
        .p1=1.0f,
};
static const struct curve fall_curve = {
        .p0=1.0f,
        .c0=1.0f,
        .c1=0.5f,
        .p1=0.0f,
};
static const float jump_height = 5.0f;
static const float jump_time = 0.5f;
static const unsigned position_update_period = 60;

static inline void update_cursor_position(struct playerController *controller) {
        ui_setQuadPosition(game_getCurrentUi(controller->game), controller->cursor_idx,
                           controller->cursor_position.x, controller->cursor_position.y,
                           controller->cursor_position.x+controller->cursor_dimensions.x,
                           controller->cursor_position.y+controller->cursor_dimensions.y, 1.0F);
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
        vec2s cursor_position = {
                .x = glm_clamp((float)args->xpos, 0, windowSize.x),
                .y = glm_clamp((float)args->ypos, 0, windowSize.y),
        };
        game_setCursorPosition(controller->game, cursor_position);
        controller->cursor_position = cursor_position;

        update_cursor_position(controller);
}

static inline void onMousePositionCamera(struct playerController *controller,
                                   struct eventBrokerMousePosition *args) {
        const vec2s current = {
                .x = (float)args->xpos,
                .y = (float)args->ypos,
        };

        if (controller->first_camera_look) {
                controller->previous_camera_look_mouse_position = current;
                controller->first_camera_look = false;
                return;
        }

        const vec2s offset = glms_vec2_sub(controller->previous_camera_look_mouse_position, current);

        float yaw_delta = game_timeDelta(controller->game)*look_sensitivity*offset.x;
        switch (controller->camera_mode) {
        case CAMERA_MODE_LOOK:
                controller->camera_position.yaw += yaw_delta;
                controller->camera_position.yaw = normalize_yaw(controller->camera_position.yaw);
                controller->camera_yaw_animation_ongoing = false; // stop animating camera to the back if we move it before animation is complete
                break;
        case CAMERA_MODE_CONTROL:
                controller->pc_rotation += controller->camera_position.yaw - camera_pos_yaw_behind;
                controller->camera_position.yaw = camera_pos_yaw_behind;
                __attribute__((fallthrough));
        case CAMERA_MODE_ONEHAND:
                controller->pc_rotation += yaw_delta;
                break;
        case CAMERA_MODE_CURSOR:
        default:
                assert_fail();
        }

        controller->camera_position.pitch += game_timeDelta(controller->game)*look_sensitivity*offset.y;
        controller->camera_position.pitch = glm_clamp(controller->camera_position.pitch, 0.05f, GLM_PI_2f - 0.05f);

        controller->camera_needs_update = true;
        controller->previous_camera_look_mouse_position = current;
}

static inline void onAnimateCameraDistance(struct playerController *controller, struct eventBrokerUpdate *args) {
        float diff = controller->camera_distance_goal - controller->camera_position.distance;
        if (ABS(diff) <= camera_zoom_animation_stop) {
                return;
        }
        
        controller->camera_position.distance += diff * camera_zoom_animation_rate * args->timeDelta;
        if (diff > 0) {
                controller->camera_position.distance = glm_clamp(controller->camera_position.distance, camera_distance_min, controller->camera_distance_goal);
        } else {
                controller->camera_position.distance = glm_clamp(controller->camera_position.distance, controller->camera_distance_goal, camera_distance_max);
        }
        
        controller->camera_needs_update = true;
}

static inline void onAnimateCameraYaw(struct playerController *controller, struct eventBrokerUpdate *args) {
        if (!controller->camera_yaw_animation_ongoing) {
                return;
        }

        float diff = camera_pos_yaw_behind - controller->camera_position.yaw;
        if (ABS(diff) <= camera_pos_yaw_animation_stop) {
                controller->camera_yaw_animation_ongoing = false;
                return;
        }

        controller->camera_position.yaw += diff * camera_pos_yaw_animation_rate * args->timeDelta;
        controller->camera_position.yaw = normalize_yaw(controller->camera_position.yaw);

        controller->camera_needs_update = true;
}

static inline void onAnimatePlayerJumpFall(struct playerController *controller, struct eventBrokerUpdate *args) {
        if (!controller->pc_jumping && !controller->pc_falling) {
                return;
        }
        controller->player_height_needs_update = true;

        controller->pc_airtime += args->timeDelta;
        float s = controller->pc_airtime / jump_time;
        if (s > 1) {
                if (controller->pc_jumping) {
                        s -= 1;
                        controller->pc_airtime = s * jump_time;
                        controller->pc_jumping = false;
                        controller->pc_falling = true;
                } else {
                        controller->pc_jumping = controller->pc_falling = false;
                        controller->pc_height = 0;
                        // TODO: if terrain height is ever supported this needs to change to a smooth transition (probably)
                        return;
                }
        }

        float v;
        if (controller->pc_jumping) {
                v = curve_sample(&jump_curve, s);
        } else if (controller->pc_falling) {
                v = curve_sample(&fall_curve, s);
        } else {
                // shouldn't happen
                v = 0;
        }
        v *= jump_height;

        controller->pc_height = v;
}

static void onUpdate(void *registerArgs, void *fireArgs) {
        struct playerController *controller = registerArgs;
        struct eventBrokerUpdate *args = fireArgs;

        onAnimateCameraDistance(controller, args);
        onAnimateCameraYaw(controller, args);
        onAnimatePlayerJumpFall(controller, args);

        struct scene *scene = NULL;
        
        if (controller->camera_needs_update) {
                scene = game_getCurrentScene(controller->game);
                struct object *camera = scene_getObjectFromIdx(scene, controller->camera_idx);
                struct transform *camera_trans = object_getComponent(camera, COMPONENT_TRANSFORM);
                camera_trans->model = getCameraModel(controller->camera_position);
        }
        controller->camera_needs_update = false;

        if (controller->player_height_needs_update) {
                scene = game_getCurrentScene(controller->game);
                struct object *player = scene_getObjectFromIdx(scene, controller->playerCharacter_idx);
                struct transform *player_trans = object_getComponent(player, COMPONENT_TRANSFORM);
                transform_setZ(player_trans, controller->pc_height);
        }
        controller->player_height_needs_update = false;

        struct transform *pc_trans = NULL;
        
        if (ABS(controller->pc_rotation) > movement_vector_zero) {
                if (scene == NULL) {
                        scene = game_getCurrentScene(controller->game);
                }
                pc_trans = object_getComponent(scene_getObjectFromIdx(scene, controller->playerCharacter_idx), COMPONENT_TRANSFORM);
                transform_rotateZ(pc_trans, normalize_yaw(controller->pc_rotation));
        }
        controller->pc_rotation = 0;

        controller->frames_since_last_position_update += 1;
        if (ABS(controller->pc_movement_direction.x) > movement_vector_zero ||
            ABS(controller->pc_movement_direction.y) > movement_vector_zero) {
                vec2s movement_direction = glms_vec2_scale(
                        glms_vec2_normalize(controller->pc_movement_direction),
                        game_timeDelta(controller->game)*movement_speed);
                if (scene == NULL) {
                        scene = game_getCurrentScene(controller->game);
                }
                if (pc_trans == NULL) {
                        pc_trans = object_getComponent(scene_getObjectFromIdx(scene, controller->playerCharacter_idx), COMPONENT_TRANSFORM);
                }
                
                if (ABS(movement_direction.x) > movement_vector_zero) {
                        transform_translateX(pc_trans, movement_direction.x);
                }
                if (ABS(movement_direction.y) > movement_vector_zero) {
                        transform_translateY(pc_trans, movement_direction.y);
                }
                if (controller->frames_since_last_position_update >= position_update_period) {
                        static struct eventPlayerPositionUpdate args;
                        args.position = glms_vec3(pc_trans->model.col[3]);
                        eventBroker_fire((enum eventBrokerEvent)EVENT_PLAYER_POSITION_NEEDS_UPDATE, &args);
                        controller->frames_since_last_position_update = 0;
                }
        }
        controller->pc_movement_direction = GLMS_VEC2_ZERO;
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
                                
                                // character must face where camera was
                                // pointing and camera must return behind it
                                controller->pc_rotation += controller->camera_position.yaw - camera_pos_yaw_behind;
                                controller->camera_position.yaw = camera_pos_yaw_behind;
                                controller->camera_needs_update = true;
                                
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
                                game_setCursorPosition(controller->game, controller->cursor_position);
                                break;
                        case CAMERA_MODE_LOOK:
                                update_cursor_visibility(controller, true);
                                game_setCursorPosition(controller->game, controller->cursor_position);
                                controller->camera_mode = CAMERA_MODE_CURSOR;
                                break;
                        case CAMERA_MODE_CONTROL:
                                // should be impossible
                                update_cursor_visibility(controller, true);
                                break;
                        case CAMERA_MODE_ONEHAND:
                                update_cursor_visibility(controller, false);
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
                                game_setCursorPosition(controller->game, controller->cursor_position);
                                break;
                        case CAMERA_MODE_LOOK:
                                // should be impossible
                                update_cursor_visibility(controller, false);
                                break;
                        case CAMERA_MODE_CONTROL:
                                update_cursor_visibility(controller, true);
                                game_setCursorPosition(controller->game, controller->cursor_position);
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

        if (game_mouseButtonPressed(controller->game, GLFW_MOUSE_BUTTON_LEFT) &&
            game_mouseButtonPressed(controller->game, GLFW_MOUSE_BUTTON_RIGHT)) {
                controller->pc_movement_direction.x += 1;
                controller->camera_yaw_animation_ongoing = true;
        }
}

static void onKeyboardPoll(void *registerArgs, void *fireArgs) {
        (void)fireArgs;
        struct playerController *controller = registerArgs;

        bool pc_left = game_keyPressed(controller->game, GLFW_KEY_A);
        bool pc_right = game_keyPressed(controller->game, GLFW_KEY_D);
        
        bool cam_left = game_keyPressed(controller->game, GLFW_KEY_LEFT);
        bool cam_right = game_keyPressed(controller->game, GLFW_KEY_RIGHT);
        
        bool up = game_keyPressed(controller->game, GLFW_KEY_UP) ||
                game_keyPressed(controller->game, GLFW_KEY_W);
        bool down = game_keyPressed(controller->game, GLFW_KEY_DOWN) ||
                game_keyPressed(controller->game, GLFW_KEY_S);
        
        bool movement_initiated = up || down || pc_left || pc_right;
        bool camera_movement_initiated = cam_left || cam_right;

        float timeDelta = game_timeDelta(controller->game);
        float spin = timeDelta*spin_speed;
        float one = 1;

        if ((cam_left || cam_right) && !(cam_left && cam_right)) {
                if (cam_left) {
                        controller->camera_position.yaw = normalize_yaw(controller->camera_position.yaw + spin);
                } else {
                        controller->camera_position.yaw = normalize_yaw(controller->camera_position.yaw - spin);
                }
                controller->camera_needs_update = true;
        }
        
        if ((pc_left || pc_right) && !(pc_left && pc_right)) {
                if (pc_right) {
                        spin = -spin;
                        one = -one;
                }
                
                switch (controller->camera_mode) {
                case CAMERA_MODE_LOOK:
                        // adjust camera yaw so that camera doesn't move in
                        // relation to pc, then update its transform too
                        controller->camera_position.yaw = normalize_yaw(controller->camera_position.yaw - spin);
                        controller->camera_needs_update = true;
                        __attribute__((fallthrough));
                case CAMERA_MODE_CURSOR:
                        controller->pc_rotation += spin;
                        break;
                case CAMERA_MODE_CONTROL:
                case CAMERA_MODE_ONEHAND:
                        controller->pc_movement_direction.y += one;
                        break;
                default:
                        assert_fail();
                }
        }

        one = 1;
        if ((up || down) && !(up && down)) {
                if (down) {
                        one = -one;
                }
                controller->pc_movement_direction.x += one;
        }
        
        if (movement_initiated && controller->camera_mode == CAMERA_MODE_CURSOR) {
                controller->camera_yaw_animation_ongoing = true;
        }
        if (camera_movement_initiated) {
                // don't animate camera to the back if we're moving it
                controller->camera_yaw_animation_ongoing = false;
        }
}

static void onKeyboardEvent(void *registerArgs, void *fireArgs) {
        struct playerController *controller = registerArgs;
        struct eventBrokerKeyboardEvent *args = fireArgs;
        const int key = args->key;
        const int action = args->action;

        if (action == GLFW_PRESS) {
                if (key == GLFW_KEY_SPACE) {
                        if (!controller->pc_jumping && !controller->pc_falling) {
                                controller->pc_jumping = true;
                                controller->pc_airtime = 0;

                                static struct eventPlayerJumped args;
                                struct scene *scene = game_getCurrentScene(controller->game);
                                struct object *player = scene_getObjectFromIdx(scene, controller->playerCharacter_idx);
                                struct transform *player_trans = object_getComponent(player, COMPONENT_TRANSFORM);
                                args.position = glms_vec3(player_trans->model.col[3]);
                                eventBroker_fire((enum eventBrokerEvent)EVENT_PLAYER_JUMPED, &args);
                        }
                }
        }
}


void playerController_setup(struct playerController *controller, const struct object *const camera, vec2s cursor_dimensions, size_t cursor_idx) {
        controller->game = camera->game;
        
        controller->camera_idx = camera->idx;
        controller->playerCharacter_idx = camera->parent;
        controller->cursor_idx = cursor_idx;

        controller->pc_movement_direction = GLMS_VEC2_ZERO;
        controller->pc_rotation = 0;
        controller->camera_needs_update = false;
        
        controller->cursor_dimensions = cursor_dimensions;
        controller->cursor_position = glms_vec2_scale(game_getWindowDimensions(camera->game), 0.5);

        controller->camera_position.distance = camera_distance_default;
        controller->camera_position.yaw = camera_pos_yaw_behind;
        controller->camera_position.pitch = camera_pos_pitch_initial;
        controller->camera_distance_goal = camera_distance_default;
        controller->camera_yaw_animation_ongoing = false;
        
        controller->first_camera_look = true;

        controller->pc_jumping = false;
        controller->pc_falling = false;
        controller->pc_airtime = 0;
        controller->pc_height = 0;
        controller->player_height_needs_update = false;

        controller->frames_since_last_position_update = 0;
        
        controller->camera_mode = CAMERA_MODE_CURSOR;

        eventBroker_register(onUpdate, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_UPDATE, controller);
        eventBroker_register(onMousePosition, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_MOUSE_POSITION, controller);
        eventBroker_register(onMouseButton, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_MOUSE_BUTTON, controller);
        eventBroker_register(onMouseScroll, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_MOUSE_SCROLL, controller);
        eventBroker_register(onKeyboardEvent, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_KEYBOARD_EVENT, controller);
        eventBroker_register(onMousePoll, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_MOUSE_POLL, controller);
        eventBroker_register(onKeyboardPoll, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_MOUSE_POLL, controller);

        update_cursor_position(controller);
        update_cursor_visibility(controller, true);

        struct transform *camera_trans = object_getComponent(camera, COMPONENT_TRANSFORM);
        camera_trans->model = getCameraModel(controller->camera_position);
}
