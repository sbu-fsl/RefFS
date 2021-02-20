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
    if (writesz < length) {
        fprintf(stderr, "Note: less data written than expected (%ld < %zu)\n",
                writesz, length);
    }
    close(fd);
    return writesz;
}

int create_dir(const char *path, mode_t mode)
{
    int status;
    status = mkdir(path, mode);
    return status;
}
