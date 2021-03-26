/** @file file.hpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
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
        size_t bufsize = f.m_fuseEntryParam.attr.st_size; 
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

    friend class FuseRamFs;
    #ifdef DUMP_TESTING
    friend void dump_File(File* file);
    #endif
//    size_t Size();
};

#endif /* file_hpp */
