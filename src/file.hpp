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

#ifndef file_hpp
#define file_hpp

class File : public Inode {
private:
    void *m_buf;
    
public:
    File() :
    m_buf(NULL) {}

    File(const File &f) : Inode(f){
        size_t bufsize = f.m_fuseEntryParam.attr.st_blocks * f.BufBlockSize; 
        m_buf = malloc(bufsize);
        if (!m_buf){
            std::cerr << "malloc failed for File copy constructor\n";
            exit(EXIT_FAILURE);
        }
        memcpy(m_buf, f.m_buf, bufsize);
    };
    
    ~File();
    
    int WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off);
    int ReadAndReply(fuse_req_t req, size_t size, off_t off);
    int FileTruncate(size_t newSize);

    size_t GetPickledSize();
    size_t Pickle(void* &buf);
    size_t Load(const void* &buf);

    friend class FuseRamFs;
    #ifdef DUMP_TESTING
    friend void dump_File(File* file);
    #endif
//    size_t Size();
};

#endif /* file_hpp */
