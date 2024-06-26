/*
 * This file is part of RefFS.
 * 
 * Copyright (c) 2020-2024 Yifei Liu
 * Copyright (c) 2020-2024 Pei Liu
 * Copyright (c) 2020-2024 Wei Su
 * Copyright (c) 2020-2024 Erez Zadok
 * Copyright (c) 2020-2024 Stony Brook University
 * Copyright (c) 2020-2024 The Research Foundation of SUNY
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

#include <stdint.h>
#include <errno.h>
#include <mcfs/errnoname.h>
#include "common.h"
#include "cr.h"

// 2023-04-14: VeriFS2 pickle and load only support Ubuntu20 and does not support Ubuntu22 yet

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <mountpoint> <output-file>\n", argv[0]);
        exit(1);
    }

    // open the mounting point directory
    int dirfd = open(argv[1], O_RDONLY | __O_DIRECTORY);
    if (dirfd < 0) {
        fprintf(stderr, "Cannot open %s: %s\n", argv[1], errnoname(errno));
        exit(1);
    }

    // write the config file to pass the output file path
    int cfgfd = open(VERIFS_PICKLE_CFG, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (cfgfd < 0) {
        fprintf(stderr, "Cannot open/create %s: (%d:%s)\n", VERIFS_PICKLE_CFG,
                errno, errnoname(errno));
        exit(2);
    }
    size_t pathlen = strnlen(argv[2], PATH_MAX);
    ssize_t res = write(cfgfd, argv[2], pathlen);
    if (res < 0) {
        fprintf(stderr, "Cannot write parameter to %s: (%d:%s)\n",
                VERIFS_PICKLE_CFG, errno, errnoname(errno));
        exit(3);
    }
    close(cfgfd);

    // call the ioctl
    int ret = ioctl(dirfd, VERIFS_PICKLE, nullptr);
    if (ret != 0) {
        printf("Result: ret = %d, errno = %d (%s)\n",
               ret, errno, errnoname(errno));
    }
    close(dirfd);
    return (ret == 0) ? 0 : 1;
}
