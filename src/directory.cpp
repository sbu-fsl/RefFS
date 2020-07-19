/** @file directory.cpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#include "common.h"

#include "inode.hpp"
#include "directory.hpp"
#include "fuse_cpp_ramfs.hpp"

using namespace std;
std::unordered_map<off_t, Directory::ReadDirCtx *> Directory::readdirStates;

void Directory::UpdateSize(ssize_t delta) {
    std::unique_lock<std::shared_mutex> lk(entryRwSem);

    /* Avoid negative result */
    if (delta < 0 && -delta > m_fuseEntryParam.attr.st_size) {
        printf("Directory::UpdateSize(): %ld of delta causes negative size.\n", delta);
        assert(0);
    }

    size_t oldBlocks = m_fuseEntryParam.attr.st_blocks;
    size_t newSize = m_fuseEntryParam.attr.st_size + delta;
    size_t newBlocks = get_nblocks(newSize, Inode::BufBlockSize);
    m_fuseEntryParam.attr.st_size = newSize;
    if (newBlocks != oldBlocks) {
        m_fuseEntryParam.attr.st_blocks = newBlocks;
        FuseRamFs::UpdateUsedBlocks(newBlocks - oldBlocks);
    }
}

void Directory::Initialize(fuse_ino_t ino, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid) {
    Inode::Initialize(ino, mode, nlink, gid, uid);
    size_t base_size = sizeof(m_children);
    UpdateSize(base_size);
}

fuse_ino_t Directory::_ChildInodeNumberWithName(const string &name) {
    if (m_children.find(name) == m_children.end()) {
        return INO_NOTFOUND;
    }
    
    return m_children[name];
}

/**
 Returns a child inode number given the name of the child.

 @param name The child file / dir name.
 @return The child inode number if the child is found. -1 otherwise.
 */
fuse_ino_t Directory::ChildInodeNumberWithName(const string &name) {
    std::shared_lock<std::shared_mutex> lk(childrenRwSem);

    return _ChildInodeNumberWithName(name);
}

int Directory::_AddChild(const string &name, fuse_ino_t ino) {
    if (m_children.find(name) != m_children.end())
        return -EEXIST;

    size_t elem_size = sizeof(_Rb_tree_node_base) + sizeof(fuse_ino_t) + sizeof(std::string) + name.size();
    if (!FuseRamFs::CheckHasSpaceFor(this, elem_size)) {
        return -ENOSPC;
    }

    const auto [it, success] = m_children.insert({name, ino});
    if (!success)
        return -ENOMEM;

    UpdateSize(elem_size);
    return 0;
}

/**
 Add a child entry to this directory.

 @param name The name of the child to add.
 @param ino The corresponding inode number.
 @return 0 if successful, or errno if error occurred
 */
int Directory::AddChild(const string &name, fuse_ino_t ino) {
    std::unique_lock<std::shared_mutex> lk(childrenRwSem);

    return _AddChild(name, ino);
}

int Directory::_UpdateChild(const string &name, fuse_ino_t ino) {
    if (m_children.find(name) == m_children.end())
        return -ENOENT;

    m_children[name] = ino;
    
    // TODO: What about directory sizes? Shouldn't we increase the reported size of our dir?
    // NOTE: This is an **update** function, why should we care about increasing size here?
    return 0;
}

/**
 Changes the inode number of the child with the given name.

 @param name The name of the child to update.
 @param ino The new inode number.
 @return 0 if successful, or errno if error occurred
 */
int Directory::UpdateChild(const string &name, fuse_ino_t ino) {
    std::unique_lock<std::shared_mutex> lk(childrenRwSem);

    return _UpdateChild(name, ino);
}

int Directory::_RemoveChild(const string &name) {
    auto child = m_children.find(name);
    if (child == m_children.end())
        return -ENOENT;

    m_children.erase(child);

    size_t elem_size = sizeof(_Rb_tree_node_base) + sizeof(fuse_ino_t) + sizeof(std::string) + name.size();
    UpdateSize(-elem_size);
    return 0;
}

/**
 Remove a child entry in this directory
 
 WARNING: This method does NOT check if the child is a non-empty directory!
          Make sure the caller check this before calling!

 @param name The name of the child to delete.
 @return 0 if successful, or errno if error occurred
 */
int Directory::RemoveChild(const string &name) {
    std::unique_lock<std::shared_mutex> lk(childrenRwSem);

    return _RemoveChild(name);
}

int Directory::WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off) {
    return fuse_reply_err(req, EISDIR);
}

int Directory::ReadAndReply(fuse_req_t req, size_t size, off_t off) {
    return fuse_reply_err(req, EISDIR);
}

Directory::ReadDirCtx* Directory::PrepareReaddir(off_t cookie) {
    if (cookie != 0) {
        /* NOTE: Will throw std::out_of_range if no entry is found */
        Directory::ReadDirCtx* ctx = readdirStates.at(cookie);

        /* If we have reached the end of the iterator, we should destroy this context 
         * to release memory */
        if (ctx->it == ctx->children.end()) {
            Directory::readdirStates.erase(cookie);
            delete ctx;
            throw(std::out_of_range("Not found"));
        }
        return ctx;
    }
    /* Make a copy of children */
    std::shared_lock<std::shared_mutex> lk(childrenRwSem);
    std::map<std::string, fuse_ino_t> copiedChildren(m_children);
    lk.unlock();

    /* Add it to the table */
    cookie = rand();
    /* Make sure there is no duplicate */
    while (readdirStates.find(cookie) != readdirStates.end()) {
        cookie = rand();
    }
    ReadDirCtx *newctx = new ReadDirCtx(cookie, copiedChildren);
    readdirStates.insert({cookie, newctx});
    return newctx;
}

bool Directory::IsEmpty() {
    std::shared_lock<std::shared_mutex> lk(childrenRwSem);
    for (auto it = m_children.begin(); it != m_children.end(); ++it) {
        if (it->first == "." || it->first == "..") {
            continue;
        }
        Inode *entry = FuseRamFs::GetInode(it->second);
        /* Not empty if it has at least one undeleted inode */
        if (entry && !entry->HasNoLinks()) {
            return false;
        }
    }
    return true;
}
