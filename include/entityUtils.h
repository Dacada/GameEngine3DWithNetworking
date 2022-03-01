#ifndef ENTITY_UTILS_H
#define ENTITY_UTILS_H

#include <stdbool.h>

#define MAX_ENTITIES 1024
#define PLAYER_SPEED 10.0f
#define JUMP_HEIGHT 5.0f
#define JUMP_TIME 0.5f

// Utility function to clamp an angle between -2*pi and 2*pi radians.
float normalize_yaw(float angle);

/*
 * Return current height in a jump/fall animation.
 */
float jump_fall_animation(bool *jumping, bool *falling, float airtime)
        __attribute__((access (read_write, 1)))
        __attribute__((access (read_write, 2)))
        __attribute__((nonnull));

#endif /* ENTITY_UTILS_H */
