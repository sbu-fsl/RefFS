/** @file directory.cpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#include <cstdio>
#include <string>
#include <map>
#include <cerrno>
#ifdef __APPLE__
#include <osxfuse/fuse/fuse_lowlevel.h>
#else
#include <fuse/fuse_lowlevel.h>
#endif

#include "inode.hpp"
#include "directory.hpp"
#include "util.hpp"

using namespace std;

void Directory::UpdateSize(ssize_t delta) {
    /* Avoid negative result */
    if (delta < 0 && -delta > m_fuseEntryParam.attr.st_size)
        return;

    m_fuseEntryParam.attr.st_size += delta;
    m_fuseEntryParam.attr.st_blocks = get_nblocks(m_fuseEntryParam.attr.st_size, Inode::BufBlockSize);
}

void Directory::Initialize(fuse_ino_t ino, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid) {
    Inode::Initialize(ino, mode, nlink, gid, uid);
    size_t base_size = sizeof(m_children);
    UpdateSize(base_size);
}

/**
 Returns a child inode number given the name of the child.

 @param name The child file / dir name.
 @return The child inode number if the child is found. -1 otherwise.
 */
fuse_ino_t Directory::ChildInodeNumberWithName(const string &name) {
    if (m_children.find(name) == m_children.end()) {
        return -1;
    }
    
    return m_children[name];
}

/**
 Add a child entry to this directory.

 @param name The name of the child to add.
 @param ino The corresponding inode number.
 @return 0 if successful, or errno if error occurred
 */
int Directory::AddChild(const string &name, fuse_ino_t ino) {
    if (m_children.find(name) != m_children.end())
        return -EEXIST;

    const auto [it, success] = m_children.insert({name, ino});
    if (!success)
        return -ENOMEM;

    size_t elem_size = sizeof(_Rb_tree_node_base) + sizeof(fuse_ino_t) + sizeof(std::string) + name.size();
    UpdateSize(elem_size);
    return 0;
}

/**
 Changes the inode number of the child with the given name.

 @param name The name of the child to update.
 @param ino The new inode number.
 @return 0 if successful, or errno if error occurred
 */
int Directory::UpdateChild(const string &name, fuse_ino_t ino) {
    if (m_children.find(name) == m_children.end())
        return -ENOENT;

    m_children[name] = ino;
    
    // TODO: What about directory sizes? Shouldn't we increase the reported size of our dir?
    // NOTE: This is an **update** function, why should we care about increasing size here?
    
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
    auto child = m_children.find(name);
    if (child == m_children.end())
        return -ENOENT;

    m_children.erase(child);
    size_t elem_size = sizeof(_Rb_tree_node_base) + sizeof(fuse_ino_t) + sizeof(std::string) + name.size();
    UpdateSize(-elem_size);
    return 0;
}

int Directory::WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off) {
    return fuse_reply_err(req, EISDIR);
}

int Directory::ReadAndReply(fuse_req_t req, size_t size, off_t off) {
    return fuse_reply_err(req, EISDIR);
}
