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