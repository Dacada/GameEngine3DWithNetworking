#include <curve.h>
#include <cglm/bezier.h>

void curve_init(struct curve *curve, float begin, float control1, float control2, float end) {
        curve->p0 = begin;
        curve->c0 = control1;
        curve->c1 = control2;
        curve->p1 = end;
}

float curve_sample(const struct curve *curve, float point) {
        return glm_bezier(point, curve->p0, curve->c0, curve->c1, curve->p1);
}
