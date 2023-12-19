/*
 * This file is part of RefFS.
 * 
 * Copyright (c) 2020-2024 Yifei Liu
 * Copyright (c) 2020-2024 Wei Su
 * Copyright (c) 2020-2024 Erez Zadok
 * Copyright (c) 2020-2024 Stony Brook University
 * Copyright (c) 2020-2024 The Research Foundation of SUNY
 * Original Copyright (C) Peter Watkins
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * RefFS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
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

/* Make sure strtok_r() exists */
#if !defined(_POSIX_C_SOURCE) && !defined(_BSD_SOURCE) && !defined(_SVID_SOURCE)
#error strtok_r is not available on your system. Please check your glibc version
#endif

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
void ramfs_parse_cmdline(struct fuse_args &args, struct fuse_ramfs_options &options);

#endif /* util_hpp */
