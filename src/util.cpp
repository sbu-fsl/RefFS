/** @file util.cpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#include "util.hpp"
#include <cstring>

// TODO: This looks like it was required before. Perhaps Sierra now includes it.
//#ifdef __MACH__
//#include <sys/time.h>
//clock_gettime is not implemented on OSX.
// TODO: Look into this implementation. Is this the proper thing to use
// for Linux and OS X?
// int clock_gettime(int /*clk_id*/, struct timespec* t) {
//     struct timeval now;
//     int rv = gettimeofday(&now, NULL);
//     if (rv) return rv;
//     t->tv_sec  = now.tv_sec;
//     t->tv_nsec = now.tv_usec * 1000;
//     return 0;
// }
//#endif

/* SizeStr2Number: Convert size string to number */
size_t SizeStr2Number(const char *str)
{
        size_t len = strnlen(str, OPTION_MAX);
        size_t unit, size = 0, magnitude = 1;
        const char *sp = str + (len - 1);
        /* ignore spaces and get the unit character */
        while (*sp == ' ') {
                --sp;
        }

        char uc = *sp;
        if (uc >= '0' && uc <= '9') {
                unit = 1;
                ++sp;
        } else if (uc == 'k' || uc == 'K') {
                unit = 1024UL;
        } else if (uc == 'm' || uc == 'M') {
                unit = 1048576UL;
        } else if (uc == 'g' || uc == 'G') {
                unit = 1048576UL * 1024UL;
        } else if (uc == 't' || uc == 'T') {
                unit = 1048576UL * 1048576UL;
        } else if (uc == 'p' || uc == 'P') {
                unit = 1048576UL * 1048576UL * 1024;
        } else if (uc == 'e' || uc == 'E') {
                unit = 1048576UL * 1048576UL * 1048576UL;
        } else {
                /* Unsupported unit */
                return 0;
        }
        --sp;
        /* Gather the number value */
        for (; sp >= str; --sp, magnitude *= 10) {
                char cur = *sp;
                if (cur < '0' || cur > '9') {
                        /* Encountered non-number, quit */
                        return 0;
                }
                size += magnitude * (cur - '0');
        }
        return size * unit;
}