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
