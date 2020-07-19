/** @file util.cpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#include "util.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>

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

/* ramfs_parse_options: Parse option string provided by -o argument
 *
 * @param[in] optstr:   Option string 
 * @param[in] opt:      The fuse_ramfs_options structure for config values
 * 
 * The option string should be in the following form:
 * "key1=value1,key2=value2,key3=value3,switch1,switch2,..."
 * Currently this function parses the following options:
 *   - size     Capacity of the RAM file system. Supports unit suffix
 *              including k,m,g,t,p,e.
 *   - inodes   Inode slots of the file system. Also supports unit suffix.
 *   - subtype  Subtype name to be displayed in mount list.
 * 
 * @return: The new string buffer containing the original option string
 *   with the parsed options excluded.
 */
char *ramfs_parse_options(char *optstr, struct fuse_ramfs_options &opt) {
    char *ctx1;
    char *kvtoken = strtok_r(optstr, ",", &ctx1);
    char *newopt = new char[strnlen(optstr, OPTION_MAX) + 1];
    char *ptr = newopt;
    printf("opt string: %s\n", optstr);
    while (kvtoken) {
        char *ctx2;
        char *key = strtok_r(kvtoken, "=", &ctx2);
        char *value = strtok_r(nullptr, "=", &ctx2);
        printf("kvtoken: %s, key=%s, value=%s\n", kvtoken, key, (value) ? value : "<null>");
        if (key && strncmp(key, "size", OPTION_MAX) == 0) {
            if (value) {
                opt.capacity = SizeStr2Number(value);
                printf("Custom capacity: %zu bytes\n", opt.capacity);
            }
        } else if (key && strncmp(key, "inodes", OPTION_MAX) == 0) {
            if (value) {
                opt.inodes = SizeStr2Number(value);
                printf("Custom inode slots: %zu\n", opt.inodes);
            }
        } else if (key && strncmp(key, "subtype", OPTION_MAX) == 0) {
            if (value) {
                opt.subtype = value;
                printf("Custom subtype: %s\n", value);
            }
        } else if (key) {
            /* For options not recognized by this function, let's copy
             * them into the newopt buffer for future use (e.g. fuse_mount)
             */
            size_t keylen = strnlen(key, OPTION_MAX);
            strncpy(ptr, key, keylen);
            ptr += keylen;
            if (value) {
                *(ptr++) = '=';
                size_t valen = strnlen(value, OPTION_MAX);
                strncpy(ptr, value, valen);
            }
            *(ptr++) = ',';
        }
        kvtoken = strtok_r(nullptr, ",", &ctx1);
    }
    /* Verify options */
    if (opt.capacity > 0 && opt.capacity < 1024) {
        printf("fuse-cpp-ramfs requires at least 1KB capacity.\n");
        exit(1);
    }
    if (opt.inodes > 0 && opt.inodes < 2) {
        printf("fuse-cpp-ramfs requires at least 2 inode slots\n");
        exit(1);
    }

    return newopt;
}