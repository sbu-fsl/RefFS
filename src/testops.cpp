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

#include "testops.h"

int create_file(const char *path, mode_t mode)
{
    int fd = creat(path, mode);
    if (fd >= 0) {
        close(fd);
    }

    return (fd >= 0)? 0 : -1;
}

ssize_t write_file(const char *path, void *data, off_t offset, size_t length)
{
    int fd = open(path, O_RDWR);
    int err;
    if (fd < 0) {
        return -1;
    }
    off_t res = lseek(fd, offset, SEEK_SET);
    if (res == (off_t) -1) {
        err = errno;
        close(fd);
        errno = err;
        return -1;
    }
    ssize_t writesz = write(fd, data, length);
    if (writesz < 0) {
        err = errno;
        close(fd);
        errno = err;
        return -1;
    }
    else if (writesz < length) {
        fprintf(stderr, "Note: less data written than expected (%ld < %zu)\n",
                writesz, length);
    }
    else{
        printf("write(%s) -> ret=%ld, errno=%s\n", 
        path, writesz, strerror(errno));
    }
    close(fd);
    return writesz;
}

int unlink_file(const char *path)
{
    int ret = unlink(path);
    int err = errno;
	printf("unlink(%s) -> ret=%d, errno=%s\n",
	       path, ret, strerror(err));
    return ret;
}


int create_dir(const char *path, mode_t mode)
{
    int status;
    status = mkdir(path, mode);
    return status;
}

int remove_dir(const char *path)
{
    int ret = rmdir(path);
    int err = errno;
	printf("rmdir(%s) -> ret=%d, errno=%s\n", 
            path, ret, strerror(err));
	return ret;
}
