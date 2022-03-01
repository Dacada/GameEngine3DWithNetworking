#define _GNU_SOURCE

#include <timeutil.h>
#include <stdio.h>

struct timespec monotonic(void) {
        struct timespec tp = {0};
        if (clock_gettime(CLOCK_MONOTONIC, &tp) == -1) {
                perror("clock_gettime");
        }
        
        return tp;
}

unsigned long monotonic_difference(const struct timespec a, const struct timespec b) {
        long sec = a.tv_sec - b.tv_sec;
        if (sec < 0) {
                return 0;
        }
        
        long nsec = a.tv_nsec - b.tv_nsec;
        if (nsec < 0) {
                sec -= 1;
                nsec = (long)1e9 + nsec;
        }
        
        nsec += sec * (long)1e9;
        if (nsec < 0) {
                return 0;
        }
        return (unsigned long)nsec;
}
