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