#include <curve.h>
static const float movement_speed = 10.0F;
static const float spin_speed = 2.0F;
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
