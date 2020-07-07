/** @file fuse_cpp_ramfs.hpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#ifndef fuse_ram_fs_hpp
#define fuse_ram_fs_hpp

#include "common.h"
#include "directory.hpp"

class FuseRamFs {
private:
    static const size_t kReadDirEntriesPerResponse = 255;
    static const size_t kReadDirBufSize = 384;
    static const size_t kInodeReclamationThreshold = 256;
    static const fsblkcnt_t kTotalBlocks = ~0;
    static const fsfilcnt_t kTotalInodes = ~0;
    static const unsigned long kFilesystemId = 0xc13f944870434d8f;
    static const size_t kMaxFilenameLength = 1024;
    
    static std::vector<Inode *> Inodes;
    static std::shared_mutex inodesRwSem;
    static std::queue<fuse_ino_t> DeletedInodes;
    static std::mutex deletedInodesMutex;
    static struct statvfs m_stbuf;
    static std::mutex stbufMutex;
    
public:
    static struct fuse_lowlevel_ops FuseOps;
    
private:
    static long do_create_node(Directory *parent, const char *name, mode_t mode, dev_t dev, const struct fuse_ctx *ctx);
    static fuse_ino_t RegisterInode(Inode *inode_p, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid);
    static fuse_ino_t NextInode();

    /* Atomic inode table operations */
    static void DeleteInode(fuse_ino_t ino) {
        std::unique_lock<std::shared_mutex> L1(inodesRwSem, std::defer_lock);
        std::unique_lock<std::mutex> L2(deletedInodesMutex, std::defer_lock);
        std::lock(L1, L2);
        Inodes[ino] = nullptr;
        DeletedInodes.push(ino);
    }

    static fuse_ino_t AddInode(Inode *inode) {
        std::unique_lock<std::shared_mutex> writelk(inodesRwSem);
        Inodes.push_back(inode);
        return Inodes.size() - 1;
    }

    static Inode *GetInode(fuse_ino_t ino) {
        std::shared_lock<std::shared_mutex> readlk(inodesRwSem);
        return Inodes[ino];
    }

    static void UpdateInode(fuse_ino_t ino, Inode *newInode) {
        std::unique_lock<std::shared_mutex> writelk(inodesRwSem);
        Inodes[ino] = newInode;
    }

    static fuse_ino_t PopOneDeletedInode() {
        std::lock_guard<std::mutex> lk(deletedInodesMutex);
        fuse_ino_t ino = DeletedInodes.front();
        DeletedInodes.pop();
    }
    
public:
    FuseRamFs();
    ~FuseRamFs();
    
    static void FuseInit(void *userdata, struct fuse_conn_info *conn);
    static void FuseDestroy(void *userdata);
    static void FuseLookup(fuse_req_t req, fuse_ino_t parent, const char *name);
    static void FuseGetAttr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    static void FuseSetAttr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi);
    static void FuseOpenDir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    static void FuseReleaseDir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    static void FuseFsyncDir(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi);
    static void FuseReadDir(fuse_req_t req, fuse_ino_t ino, size_t size,
                            off_t off, struct fuse_file_info *fi);
    static void FuseOpen(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    static void FuseRelease(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    static void FuseFsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi);
    static void FuseMknod(fuse_req_t req, fuse_ino_t parent, const char *name,
                          mode_t mode, dev_t rdev);
    static void FuseMkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode);
    static void FuseUnlink(fuse_req_t req, fuse_ino_t parent, const char *name);
    static void FuseRmdir(fuse_req_t req, fuse_ino_t parent, const char *name);
    static void FuseForget(fuse_req_t req, fuse_ino_t ino, unsigned long nlookup);
    static void FuseWrite(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi);
    static void FuseFlush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    static void FuseRead(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
    static void FuseRename(fuse_req_t req, fuse_ino_t parent, const char *name, fuse_ino_t newparent, const char *newname);
    static void FuseLink(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname);
    static void FuseSymlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name);
    static void FuseReadLink(fuse_req_t req, fuse_ino_t ino);
    static void FuseStatfs(fuse_req_t req, fuse_ino_t ino);

#ifdef __APPLE__
    static void FuseSetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags, uint32_t position);
#else
    static void FuseSetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags);
#endif
    
#ifdef __APPLE__
    static void FuseGetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size, uint32_t position);
#else
    static void FuseGetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size);
#endif
    
    static void FuseListXAttr(fuse_req_t req, fuse_ino_t ino, size_t size);
    static void FuseRemoveXAttr(fuse_req_t req, fuse_ino_t ino, const char *name);
    static void FuseAccess(fuse_req_t req, fuse_ino_t ino, int mask);
    static void FuseCreate(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi);
    static void FuseGetLock(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock);
    
    static void UpdateUsedBlocks(ssize_t blocksAdded) {
        std::lock_guard<std::mutex> lk(stbufMutex);
        m_stbuf.f_bfree -= blocksAdded;
        m_stbuf.f_bavail -= blocksAdded;
    }
    static void UpdateUsedInodes(ssize_t inodesAdded) {
        std::lock_guard<std::mutex> lk(stbufMutex);
        m_stbuf.f_ffree -= inodesAdded;
        m_stbuf.f_favail -= inodesAdded;
    }
    static void FsStat(struct statvfs *out) {
        std::lock_guard<std::mutex> lk(stbufMutex);
        *out = m_stbuf;
    }
};

#endif /* fuse_ram_fs_hpp */
