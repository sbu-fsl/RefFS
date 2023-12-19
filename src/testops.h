/*
 * This file is part of RefFS.
 * 
 * Copyright (c) 2020-2024 Yifei Liu
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

#ifndef _TEST_FS_OPS_H
#define _TEST_FS_OPS_H

#include <stdio.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

int create_file(const char *path, mode_t mode);
ssize_t write_file(const char *path, void *data, off_t offset, size_t length);
int create_dir(const char *path, mode_t mode);
int unlink_file(const char *path);
int remove_dir(const char *path);

#define VERIFS2_MOUNTPOINT "/mnt/test-verifs2"

#ifdef __cplusplus
}
#endif

#endif