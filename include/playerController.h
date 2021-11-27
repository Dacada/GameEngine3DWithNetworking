#ifndef PLAYER_CONTROLLER_H
#define PLAYER_CONTROLLER_H

#include <thirty/game.h>
#include <cglm/struct.h>
#include <stddef.h>

struct sphericalCoord {
        float distance;
        float yaw;
        float pitch;
};

struct playerController {
        struct game *game;
        
        size_t camera_idx;
        size_t playerCharacter_idx;
        size_t cursor_idx;

        vec2s cursor_dimensions;
        vec2s cursor_position;

        vec2s pc_movement_direction;
        float pc_rotation;
        bool camera_needs_update;

        struct sphericalCoord camera_position;
        float camera_distance_goal;
        bool camera_yaw_animation_ongoing;
        
        bool first_camera_look;
        vec2s previous_camera_look_mouse_position;
        
        enum {
                CAMERA_MODE_CURSOR,  // Mouse movement does not control camera,
                                     // instead moves a cursor.
                
                CAMERA_MODE_LOOK,    // Mouse movement controls the camera but
                                     // not the character, allowing to look
                                     // around without spinning. Directional
                                     // movement spins the character.
                
                CAMERA_MODE_CONTROL, // Mouse movement controls the camera and
                                     // the character, such that moving around
                                     // spins the character. Directional
                                     // movement strafes the character.
                
                CAMERA_MODE_ONEHAND, // Mouse movement controls the camera and
                                     // the character exactly as
                                     // CAMERA_MODE_CONTROL and additionally
                                     // the character moves forward.
        } camera_mode;
};

void playerController_setup(struct playerController *controller, const struct object *const camera, vec2s cursorDimensions, size_t cursor_idx)
        __attribute__((access (write_only, 1)))
        __attribute__((access (read_only, 2)))
        __attribute__((nonnull));

#endif /* PLAYER_CONTROLLER_H */
