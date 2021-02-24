/** @file symlink.cpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#include "common.h"

#include "util.hpp"
#include "inode.hpp"
#include "symlink.hpp"


SymLink::SymLink(SymLink &obj)
{
    // Special for Directory
    m_link =obj.m_link;
    // Common in Inode
    m_fuseEntryParam = obj.m_fuseEntryParam;
    m_markedForDeletion = obj.m_markedForDeletion;
    m_nlookup.store(obj.m_nlookup);
    m_xattr = obj.m_xattr;
    //entryRwSem(obj.entryRwSem);
    //xattrRwSem(obj.xattrRwSem);
}

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