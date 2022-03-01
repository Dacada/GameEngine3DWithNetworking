#ifndef TIMEUTIL_H
#define TIMEUTIL_H

#include <time.h>

// Get current time according to a monotonic clock. This does not relate to the
// real clock time. Nanosecond precission.
struct timespec monotonic(void);

// Get the difference in nanoseconds between two monotonic clock times.
unsigned long monotonic_difference(struct timespec a, struct timespec b);

#endif /* TIMEUTIL_H */
