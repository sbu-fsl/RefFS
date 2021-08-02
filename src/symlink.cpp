/** @file symlink.cpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#include "common.h"

#include "util.hpp"
#include "inode.hpp"
#include "symlink.hpp"

int SymLink::WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off) {
    return fuse_reply_err(req, EISDIR);
}

int SymLink::ReadAndReply(fuse_req_t req, size_t size, off_t off) {
    return fuse_reply_err(req, EISDIR);
}

void SymLink::Initialize(fuse_ino_t ino, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid) {
    Inode::Initialize(ino, mode, nlink, gid, uid);

    m_fuseEntryParam.attr.st_size = m_link.size();
    m_fuseEntryParam.attr.st_blocks = m_fuseEntryParam.attr.st_size / m_fuseEntryParam.attr.st_blksize;

    // Add another block if the size is larger than a whole number of blocks
    if (m_fuseEntryParam.attr.st_blksize * m_fuseEntryParam.attr.st_blocks < m_fuseEntryParam.attr.st_size) {
        m_fuseEntryParam.attr.st_blocks++;
    }
}

size_t SymLink::GetPickledSize() {
    return Inode::GetPickledSize() + m_link.size();
}

size_t SymLink::Pickle(void *&buf) {
    if (buf == nullptr) {
        buf = malloc(SymLink::GetPickledSize());
    }
    if (buf == nullptr) {
        return 0;
    }
    size_t offset = Inode::Pickle(buf);
    char *ptr = (char *)buf + offset;
    memcpy(ptr, m_link.c_str(), m_link.size());
    return offset + m_link.size();
}

size_t SymLink::Load(const void *&buf) {
    size_t offset = Inode::Load(buf);
    const char *ptr = (const char *) buf + offset;
    size_t linksize = m_fuseEntryParam.attr.st_size;
    m_link = std::string(ptr, linksize);
    return offset + linksize;
}