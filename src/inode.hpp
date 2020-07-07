/** @file inode.hpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#ifndef inode_hpp
#define inode_hpp

#include "common.h"

class Inode {
private:    
    bool m_markedForDeletion;
    std::atomic_ulong m_nlookup;

protected:
    struct fuse_entry_param m_fuseEntryParam;
    std::shared_mutex entryRwSem;
    std::map<std::string, std::pair<void *, size_t> > m_xattr;
    std::shared_mutex xattrRwSem;
    
public:
    static const size_t BufBlockSize = 512;
    
public:
    Inode() :
    m_markedForDeletion(false),
    m_nlookup(0)
    {}
    
    virtual ~Inode() = 0;
    
    virtual int WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off) = 0;
    virtual int ReadAndReply(fuse_req_t req, size_t size, off_t off) = 0;
    int ReplyEntry(fuse_req_t req);
    int ReplyCreate(fuse_req_t req, struct fuse_file_info *fi);
    // TODO: Should this be GetAttr?
    int ReplyAttr(fuse_req_t req);
    // TODO: This is doing more then just replying. Factor out setting attributes?
    int ReplySetAttr(fuse_req_t req, struct stat *attr, int to_set);
    void Forget(fuse_req_t req, unsigned long nlookup);
    virtual void Initialize(fuse_ino_t ino, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid);
    virtual int SetXAttrAndReply(fuse_req_t req, const std::string &name, const void *value, size_t size, int flags, uint32_t position);
    virtual int GetXAttrAndReply(fuse_req_t req, const std::string &name, size_t size, uint32_t position);
    virtual int ListXAttrAndReply(fuse_req_t req, size_t size);
    virtual int RemoveXAttrAndReply(fuse_req_t req, const std::string &name);
    virtual int ReplyAccess(fuse_req_t req, int mask, gid_t gid, uid_t uid);
    
    /* Atomic file attribute operations */
    void AddHardLink() {
        std::unique_lock<std::shared_mutex> lk(entryRwSem);
        m_fuseEntryParam.attr.st_nlink++;
    }
    void RemoveHardLink() {
        std::unique_lock<std::shared_mutex> lk(entryRwSem);
        m_fuseEntryParam.attr.st_nlink--;
    }
    bool HasNoLinks() {
        std::shared_lock<std::shared_mutex> lk(entryRwSem);
        return m_fuseEntryParam.attr.st_nlink == 0;
    }
    size_t UsedBlocks() {
        std::shared_lock<std::shared_mutex> lk(entryRwSem);
        return m_fuseEntryParam.attr.st_blocks;
    }
    size_t Size() {
        std::shared_lock<std::shared_mutex> lk(entryRwSem);
        return m_fuseEntryParam.attr.st_size;
    }
    void GetAttr(struct stat *out) {
        std::shared_lock<std::shared_mutex> lk(entryRwSem);
        *out = m_fuseEntryParam.attr;
    }
    mode_t GetMode() {
        std::shared_lock<std::shared_mutex> lk(entryRwSem);
        return m_fuseEntryParam.attr.st_mode;
    }
    /* We assume that attr.st_ino won't change */
    fuse_ino_t GetIno() { return m_fuseEntryParam.attr.st_ino; }
    
    bool Forgotten() { return m_nlookup == 0; }
};

#endif /* inode_hpp */
