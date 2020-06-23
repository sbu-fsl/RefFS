/** @file directory.hpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#ifndef directory_hpp
#define directory_hpp

class Directory : public Inode {
private:
    std::map<std::string, fuse_ino_t> m_children;

    void UpdateSize(ssize_t delta);
public:
    ~Directory() {}

    void Initialize(fuse_ino_t ino, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid);
    fuse_ino_t ChildInodeNumberWithName(const std::string &name);
    int AddChild(const std::string &name, fuse_ino_t);
    int UpdateChild(const std::string &name, fuse_ino_t ino);
    int RemoveChild(const std::string &name);
    int WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off);
    int ReadAndReply(fuse_req_t req, size_t size, off_t off);
    
    const std::map<std::string, fuse_ino_t> &Children() { return m_children; }
};

#endif /* directory_hpp */
