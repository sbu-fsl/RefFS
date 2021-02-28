#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cerrno>
#include "fuse_cpp_ramfs.hpp"
#include "testops.h"

std::string MOUNTPOINT = VERIFS2_MOUNTPOINT;

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <mountpoint> <key>\n", argv[0]);
        exit(1);
    }

    const char *mp = argv[1];
    char *end;
    uint64_t key = strtoul(argv[2], &end, 10);
    printf("Restoring file system at %s, key is %lu\n", mp, key);

    int dirfd = open(mp, O_RDONLY | __O_DIRECTORY);
    if (dirfd < 0) {
        fprintf(stderr, "Cannot open %s\n", mp);
        exit(1);
    }

    // Remove existing files and dirs
    int ret;
    std::string testfile = "/ckpt_test.txt";
    std::string testdir = "/ckpt_test_dir";

    std::string testfile_renamed = "/ckpt_test_rename.txt";
    std::string testdir_renamed = "/ckpt_test_rename_dir";  


    /*
    // Comment it, because it triggers VeriFS2 bug -- after deletion 
    // Inodes size is the still before deletion while data has been removed
    // So it will have segmentation fault
    ret = unlink_file((MOUNTPOINT+testfile).c_str());
    if (ret != 0){
        goto err;
    }
    ret = remove_dir((MOUNTPOINT+testdir).c_str());
    if (ret != 0){
        goto err;
    }
    */

    ret = rename((MOUNTPOINT+testfile).c_str(), (MOUNTPOINT+testfile_renamed).c_str());
    if (ret != 0){
        goto err;
    }
    ret = rename((MOUNTPOINT+testdir).c_str(), (MOUNTPOINT+testdir_renamed).c_str());
    if (ret != 0){
        goto err;
    }

    ret = ioctl(dirfd, VERIFS2_RESTORE, (void *)key);
    if (ret != 0) {
        printf("Result: ret = %d, errno = %d\n", ret, errno);
    }
    return (ret == 0) ? 0 : 1;
err:
    printf("Error: ret = %d, errno = %d\n", ret, errno);
    exit(EXIT_FAILURE);
}
