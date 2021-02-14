/** @file file.hpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#ifndef file_hpp
#define file_hpp

class File : public Inode {
public:
    void *m_buf;
    
public:
    File() :
    m_buf(NULL) {}
    
    ~File();
    
    int WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off);
    int ReadAndReply(fuse_req_t req, size_t size, off_t off);
    int FileTruncate(size_t newSize);
    
//    size_t Size();
};

#endif /* file_hpp */
