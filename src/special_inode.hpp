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

#ifndef special_inode_hpp
#define special_inode_hpp

enum SpecialInodeTypes {
    SPECIAL_INODE_TYPE_NO_BLOCK,
    SPECIAL_INODE_CHAR_DEV,
    SPECIAL_INODE_BLOCK_DEV,
    SPECIAL_INODE_FIFO,
    SPECIAL_INODE_SOCK
};

class SpecialInode : public Inode {
private:
    enum SpecialInodeTypes m_type;
public:
    SpecialInode() : m_type(SPECIAL_INODE_TYPE_NO_BLOCK) {}
    SpecialInode(enum SpecialInodeTypes type, dev_t dev = 0);
    ~SpecialInode() {};

    SpecialInode(const SpecialInode &sp) : Inode(sp) {
      m_type = sp.m_type;
    }    
    
    int WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off);
    
    int ReadAndReply(fuse_req_t req, size_t size, off_t off);

    enum SpecialInodeTypes Type();

    size_t GetPickledSize();
    size_t Pickle(void* &buf);
    size_t Load(const void* &buf);

    #ifdef DUMP_TESTING
    friend void dump_SpecialInode(SpecialInode* sinode);
    #endif
};

#endif /* special_inode_hpp */
