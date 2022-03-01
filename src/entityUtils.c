#include <entityUtils.h>
#include <curve.h>
#include <cglm/cglm.h>

static const struct curve jump_curve = {
        .p0 = 0.0f,
        .c0 = 0.5f,
        .c1 = 1.0f,
        .p1 = 1.0f,
};

static const struct curve fall_curve = {
        .p0 = 1.0f,
        .c0 = 1.0f,
        .c1 = 0.5f,
        .p1 = 0.0f,
};

float normalize_yaw(const float angle) {
        if (angle < 0) {
                return angle + 2*GLM_PIf;
        } else if (angle > 2*GLM_PIf) {
                return angle - 2*GLM_PIf;
        } else {
                return angle;
        }
}

float jump_fall_animation(bool *const jumping, bool *const falling, const float airtime) {
        float s = airtime / JUMP_TIME;
        if (s > 1 && *jumping) {
                *jumping = false;
                *falling = true;
        } else if (s > 2) {
                *jumping = *falling = false;
                return 0;
        }

        float v;
        if (*jumping) {
                v = curve_sample(&jump_curve, s);
        } else if (*falling) {
                v = curve_sample(&fall_curve, s-1);
        } else {
                v = 0;
        }
        v *= JUMP_HEIGHT;

        return v;
}
