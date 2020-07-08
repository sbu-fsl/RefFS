/** @file directory.hpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#ifndef directory_hpp
#define directory_hpp

#include "common.h"

class Directory : public Inode {
private:
    std::map<std::string, fuse_ino_t> m_children;
    std::shared_mutex childrenRwSem;

    void UpdateSize(ssize_t delta);
public:
    struct ReadDirCtx {
        off_t cookie;
        std::map<std::string, fuse_ino_t>::iterator it;
        std::map<std::string, fuse_ino_t> children;
        ReadDirCtx() {}
        ReadDirCtx(off_t ck, std::map<std::string, fuse_ino_t> &ch)
            : cookie(ck) {
                children = ch;
                it = children.begin();
            }
    };

    static std::unordered_map<off_t, Directory::ReadDirCtx *> readdirStates;
    ReadDirCtx* PrepareReaddir(off_t cookie);
public:
    ~Directory() {}

    void Initialize(fuse_ino_t ino, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid);
    fuse_ino_t ChildInodeNumberWithName(const std::string &name);
    int AddChild(const std::string &name, fuse_ino_t);
    int UpdateChild(const std::string &name, fuse_ino_t ino);
    int RemoveChild(const std::string &name);
    int WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off);
    int ReadAndReply(fuse_req_t req, size_t size, off_t off);

    /* Atomic children operations */
    bool IsEmpty() {
        std::shared_lock<std::shared_mutex> lk(childrenRwSem);
        return m_children.size() <= 2;
    }
    
    /* NOTE: Not guarded, so accuracy is not guaranteed.
     * Mainly intended for readdir() method. */
    const std::map<std::string, fuse_ino_t> &Children() { return m_children; }
};

#endif /* directory_hpp */
