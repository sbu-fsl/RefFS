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

    void ClearXAttrs() {
        for (auto it = m_xattr.begin(); it != m_xattr.end(); ++it) {
            free(it->second.first);
        }
        m_xattr.clear();
    }
    
public:
    static const size_t BufBlockSize = 512;
    
public:
    Inode() :
    m_markedForDeletion(false),
    m_nlookup(0)
    {}

    Inode(const Inode &src) {
      m_markedForDeletion = src.m_markedForDeletion;
      m_nlookup.store(src.m_nlookup.load());
      m_fuseEntryParam = src.m_fuseEntryParam;
      m_xattr = src.m_xattr;
    }

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
    void IncrementLinkCount() {
        std::unique_lock<std::shared_mutex> lk(entryRwSem);
        m_fuseEntryParam.attr.st_nlink++;
    }
    void DecrementLinkCount() {
        std::unique_lock<std::shared_mutex> lk(entryRwSem);
        m_fuseEntryParam.attr.st_nlink--;
    }
    int NumLinks() {
        std::shared_lock<std::shared_mutex> lk(entryRwSem);
        return m_fuseEntryParam.attr.st_nlink;
    }
    bool IsActive() {
        std::shared_lock<std::shared_mutex> lk(entryRwSem);
        return (m_fuseEntryParam.attr.st_nlink + m_nlookup) > 0;
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

    virtual size_t GetPickledSize();

    /* Pickle: Serialize the Inode object.
     *
     * NOTE: This function does NOT consider implementation or architecture
     * specific data type length or byte order, so it's not recommended to
     * pickle and unpickle on different machines. 
     *
     * @param[in] The pointer to the buffer. If the buffer is nullptr, Pickle
     * will automatically allocate sufficient buffer to store the data.
     * If buf is not null, it must provide at least GetPickledSize() bytes of
     * buffer, otherwise the behavior is undefined.
     *
     * @return The size of serialized object
     */
    virtual size_t Pickle(void* &buf);
    virtual size_t Load(const void* &buf);

    friend class FuseRamFs;
    friend class File;
    friend class Directory;
    friend class SpecialInode;
    friend class SymLink;
};

#endif /* inode_hpp */
