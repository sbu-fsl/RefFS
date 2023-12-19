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

#ifndef directory_hpp
#define directory_hpp

#include "common.h"

class Directory : public Inode {
private:
    std::vector<std::pair<std::string, fuse_ino_t>> m_children;
    std::shared_mutex childrenRwSem;

    void UpdateSize(ssize_t delta);
public:
    struct ReadDirCtx {
        off_t cookie;
        std::vector<std::pair<std::string, fuse_ino_t>>::iterator it;
        std::vector<std::pair<std::string, fuse_ino_t>> children;
        ReadDirCtx() {}
        ReadDirCtx(off_t ck, std::vector<std::pair<std::string, fuse_ino_t>> &ch)
            : cookie(ck) {
                children = ch;
                it = children.begin();
            }
    };

    static std::unordered_map<off_t, Directory::ReadDirCtx *> readdirStates;
    ReadDirCtx* PrepareReaddir(off_t cookie);
    void RecycleStates();
    friend class FuseRamFs;
public:
    ~Directory() {}
    Directory() {};
    Directory(const Directory &d) : Inode(d) {
      m_children = d.m_children;
    }

    void Initialize(fuse_ino_t ino, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid);
    fuse_ino_t _ChildInodeNumberWithName(const std::string &name);
    fuse_ino_t ChildInodeNumberWithName(const std::string &name);
    int _AddChild(const std::string &name, fuse_ino_t);
    int AddChild(const std::string &name, fuse_ino_t);
    int _UpdateChild(const std::string &name, fuse_ino_t ino);
    int UpdateChild(const std::string &name, fuse_ino_t ino);
    int _RemoveChild(const std::string &name);
    int RemoveChild(const std::string &name);
    int WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off);
    int ReadAndReply(fuse_req_t req, size_t size, off_t off);
    std::vector<std::pair<std::string, fuse_ino_t>>::iterator find(const std::string &name);

    /* Atomic children operations */
    bool IsEmpty();
   
    /* NOTE: Not guarded, so accuracy is not guaranteed.
     * Mainly intended for readdir() method. */
    const std::vector<std::pair<std::string, fuse_ino_t>> &Children() { return m_children; }

    size_t GetPickledSize();
    size_t Pickle(void* &buf);
    size_t Load(const void* &buf);

    std::shared_mutex& DirLock() { return childrenRwSem; }
    #ifdef DUMP_TESTING
    friend void dump_Directory(Directory* dir);
    #endif
};

#endif /* directory_hpp */
