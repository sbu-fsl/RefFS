/*
 * This file is part of RefFS.
 * 
 * Copyright (c) 2020-2024 Yifei Liu
 * Copyright (c) 2020-2024 Wei Su
 * Copyright (c) 2020-2024 Erez Zadok
 * Copyright (c) 2020-2024 Stony Brook University
 * Copyright (c) 2020-2024 The Research Foundation of SUNY
 * Original Copyright (C) Peter Watkins
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

#include "common.h"

#include "inode.hpp"
#include "fuse_cpp_ramfs.hpp"

using namespace std;

char **copy_args(int argc, const char * argv[]) {
    char **new_argv = (char **)malloc(sizeof(char *)*argc);
    for (int i = 0; i < argc; ++i) {
        int len = (int) strlen(argv[i]) + 1;
        new_argv[i] = (char *)malloc(len);
        strncpy(new_argv[i], argv[i], len);
    }
    return new_argv;
}

void delete_args(int argc, char **argv) {
    for (int i = 0; i < argc; ++i) {
        delete[] argv[i];
    }
    delete[] argv;
}

struct fuse_chan *ch;
/** The main entry point for the program. A filesystem may be mounted by running this executable directly.
 * @param argc The number of arguments.
 * @param argv A pointer to the first argument.
 * @return The exit code of the program. Zero on success. */
int main(int argc, const char * argv[]) {
    char **fuse_argv = copy_args(argc, argv);
    
    struct fuse_args args = {argc, fuse_argv, 1};
    struct fuse_ramfs_options options = {0};
    char *mountpoint;
    // default error, exit code of any kind of failure
    int err = -1;

    std::srand(std::time(nullptr));
    
    /* Parse command-line args by fuse-ramfs */
    opterr = 0;
    ramfs_parse_cmdline(args, options);
    // The core code for our filesystem.
    size_t nblocks = options.capacity / Inode::BufBlockSize;
    FuseRamFs core(nblocks, options.inodes);
    
    if (options.subtype) {
        mountpoint = options.mountpoint;
        if (mountpoint == nullptr) {
            cerr << "USAGE: fuse-cpp-ramfs MOUNTPOINT" << endl;
        } else if ((ch = fuse_mount(mountpoint, &args)) != nullptr) {
            struct fuse_session *se;
            // The FUSE options come from our core code.
            se = fuse_lowlevel_new(&args, &(FuseRamFs::FuseOps),
                                   sizeof(FuseRamFs::FuseOps), nullptr);
            if (se != nullptr) {
                fuse_daemonize(options.deamonize == 0);
                if (fuse_set_signal_handlers(se) != -1) {
                    fuse_session_add_chan(se, ch);
                    err = fuse_session_loop(se);
                    fuse_remove_signal_handlers(se);
                    fuse_session_remove_chan(ch);
                }
                fuse_session_destroy(se);
            }
            fuse_unmount(mountpoint, ch);
            delete[] mountpoint;
        }
    }
    fuse_opt_free_args(&args);
    
    //delete_args(argc, fuse_argv);
    
    return err ? 1 : 0;
}
