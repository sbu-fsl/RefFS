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
#include "special_inode.hpp"

SpecialInode::SpecialInode(enum SpecialInodeTypes type, dev_t dev) :
m_type(type) {
    m_fuseEntryParam.attr.st_dev = dev;
    if (type == SPECIAL_INODE_TYPE_NO_BLOCK) {
        memset(&m_fuseEntryParam.attr, 0, sizeof(m_fuseEntryParam.attr));
    }
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

size_t SpecialInode::GetPickledSize() {
    return Inode::GetPickledSize() + sizeof(m_type);
}

size_t SpecialInode::Pickle(void* &buf) {
    if (buf == nullptr) {
        buf = malloc(SpecialInode::GetPickledSize());
    }
    if (buf == nullptr) {
        return 0;
    }
    size_t offset = Inode::Pickle(buf);
    char *ptr = (char *)buf + offset;
    memcpy(ptr, &m_type, sizeof(m_type));
    return offset + sizeof(m_type);
}

size_t SpecialInode::Load(const void* &buf) {
    size_t offset = Inode::Load(buf);
    char *ptr = (char *)buf + offset;
    memcpy(&m_type, ptr, sizeof(m_type));
    return offset + sizeof(m_type);
}
