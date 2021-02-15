#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cerrno>
#include "fuse_cpp_ramfs.hpp"

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <mountpoint> <key>\n", argv[0]);
        exit(1);
    }

    const char *mp = argv[1];
    char *end;
    uint64_t key = strtoul(argv[2], &end, 10);
    printf("Checkpointing file system at %s, key is %lu\n", mp, key);

    int dirfd = open(mp, O_RDONLY | __O_DIRECTORY);
    if (dirfd < 0) {
        fprintf(stderr, "Cannot open %s\n", mp);
        exit(1);
    }

    int ret = ioctl(dirfd, VERIFS2_CHECKPOINT, (void *)key);
    if (ret != 0) {
        printf("Result: ret = %d, errno = %d\n", ret, errno);
    }
    return (ret == 0) ? 0 : 1;
}
