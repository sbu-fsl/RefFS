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
    printf("Checkpointing file system at %s, key is %lu\n", mp, key);

    int dirfd = open(mp, O_RDONLY | __O_DIRECTORY);
    if (dirfd < 0) {
        fprintf(stderr, "Cannot open %s\n", mp);
        exit(1);
    }

    int ret;
    /*
    // do some file system operations
    // create a file
    
    std::string testfile = "/ckpt_test.txt";
    std::string testdir = "/ckpt_test_dir";
    ret = create_file((MOUNTPOINT+testfile).c_str(), 0644);
    if(ret < 0){
        return ret;
    }
    // write to a file
    char *data = (char *)"Checkpoint";
    ret = write_file((MOUNTPOINT+testfile).c_str(), data, 0, strlen(data));
    if(ret < 0){
        return ret;
    }
    
    // create a directory
    ret = create_dir((MOUNTPOINT+testdir).c_str(), 0755);
    if(ret < 0){
        return ret;
    }

    // Testing for File on inner dir of mount point
    std::string test_inner_file = "/ckpt_test_dir/restore_ckpt_test_inner.txt";
    // Create this inner file
    ret = create_file((MOUNTPOINT+test_inner_file).c_str(), 0644);
    if(ret < 0){
        return ret;
    }
    // Write this file into the dir
    char *inner_data = (char *)"Inner data testing for Checkpoint&Restore APIs";
    ret = write_file((MOUNTPOINT+test_inner_file).c_str(), inner_data, 0, strlen(inner_data));
    if(ret < 0){
        return ret;
    }
    */

    ret = ioctl(dirfd, VERIFS_CHECKPOINT, (void *)key);
    if (ret != 0) {
        printf("Result: ret = %d, errno = %d\n", ret, errno);
    }

    //std::cout << "CHECKPOINT: Running dump_state_pool() to dump current states\n";
    return (ret == 0) ? 0 : 1;
}
