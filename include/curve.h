#ifndef CURVE_H
#define CURVE_H

/*
 * This module defines a simple one dimensional normalized cubic bezier
 * curve. The curve implementation itself is left up to cglm. The curve is
 * defined by a begin point, an end point and two control
 * points. http://www.demofox.org/bezcubic1d.html offers a good visualizer of
 * these curves.
 */

struct curve {
        float p0, c0, c1, p1;
};

// Initialize curve data structure
void curve_init(struct curve *curve, float begin, float control1, float control2, float end)
        __attribute__((access (write_only, 1)))
        __attribute__((nonnull));

// Return value at the given point of the interval [0,1]
float curve_sample(const struct curve *curve, float point)
        __attribute__((access (read_only, 1)))
        __attribute__((nonnull));

#endif /* CURVE_H */
