/** @file util.hpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#ifndef util_hpp
#define util_hpp

#include <sys/types.h>
#include <linux/binfmts.h>

#define INO_NOTFOUND    (fuse_ino_t) 0
#ifndef PAGE_SIZE
#define PAGE_SIZE       4096
#endif
#define OPTION_MAX      MAX_ARG_STRLEN

struct fuse_ramfs_options {
    size_t capacity;
    size_t inodes;
    bool deamonize;
    char *subtype;
    char *mountpoint;
    char *_optstr;
};

// TODO: This looks like it was required before. Perhaps Sierra now includes it.
//#ifdef __MACH__
//#define CLOCK_REALTIME 0
//clock_gettime is not implemented on OSX.
//int clock_gettime(int /*clk_id*/, struct timespec* t);
//#endif

static inline size_t round_up(size_t value, size_t unit) {
        return (value + unit - 1) / unit * unit;
}

static inline size_t get_nblocks(size_t size, size_t blocksize) {
        return (size + blocksize - 1) / blocksize;
}

size_t SizeStr2Number(const char *str);
char *ramfs_parse_options(char *optstr, struct fuse_ramfs_options &opt);

#endif /* util_hpp */
