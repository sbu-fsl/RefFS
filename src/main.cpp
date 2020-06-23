/** @file main.cpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#if !defined(FUSE_USE_VERSION) || FUSE_USE_VERSION < 30
#define FUSE_USE_VERSION 30
#endif

#ifdef __APPLE__
#include <osxfuse/fuse/fuse_lowlevel.h>
#else
#include <fuse/fuse_lowlevel.h>
#endif
#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <cstring>

#include "inode.hpp"
#include "fuse_cpp_ramfs.hpp"

using namespace std;

char **copy_args(int argc, const char * argv[]) {
    char **new_argv = new char*[argc];
    for (int i = 0; i < argc; ++i) {
        int len = (int) strlen(argv[i]) + 1;
        new_argv[i] = new char[len];
        strncpy(new_argv[i], argv[i], len);
    }
    return new_argv;
}

void delete_args(int argc, char **argv) {
    for (int i = 0; i < argc; ++i) {
        delete argv[i];
    }
    delete argv;
}

/** The main entry point for the program. A filesystem may be mounted by running this executable directly.
 * @param argc The number of arguments.
 * @param argv A pointer to the first argument.
 * @return The exit code of the program. Zero on success. */
int main(int argc, const char * argv[]) {
    char **fuse_argv = copy_args(argc, argv);
    
    struct fuse_args args = FUSE_ARGS_INIT(argc, fuse_argv);
    struct fuse_chan *ch;
    char *mountpoint;
    int err = -1;
    
    // The core code for our filesystem.
    FuseRamFs core;
    
    if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1) {
        if (mountpoint == NULL) {
            cerr << "USAGE: fuse-cpp-ramfs MOUNTPOINT" << endl;
        } else if ((ch = fuse_mount(mountpoint, &args)) != NULL) {
            struct fuse_session *se;
            
            // The FUSE options come from our core code.
            se = fuse_lowlevel_new(&args, &(core.FuseOps),
                                   sizeof(core.FuseOps), NULL);
            if (se != NULL) {
                if (fuse_set_signal_handlers(se) != -1) {
                    fuse_session_add_chan(se, ch);
                    err = fuse_session_loop(se);
                    fuse_remove_signal_handlers(se);
                    fuse_session_remove_chan(ch);
                }
                fuse_session_destroy(se);
            }
            fuse_unmount(mountpoint, ch);
        }
    }
    fuse_opt_free_args(&args);
    
    delete_args(argc, fuse_argv);
    
    return err ? 1 : 0;
}
