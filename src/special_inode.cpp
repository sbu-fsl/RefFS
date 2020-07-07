/** @file special_inode.cpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#include "common.h"

#include "inode.hpp"
#include "special_inode.hpp"

SpecialInode::SpecialInode(enum SpecialInodeTypes type, dev_t dev) :
m_type(type) {
    m_fuseEntryParam.attr.st_dev = dev;
}

//SpecialInode::~SpecialInode() {}


enum SpecialInodeTypes SpecialInode::Type() {
    return m_type;
}

int SpecialInode::WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off) {
    return fuse_reply_err(req, ENOENT);
}

int SpecialInode::ReadAndReply(fuse_req_t req, size_t size, off_t off) {
    return fuse_reply_err(req, ENOENT);
}