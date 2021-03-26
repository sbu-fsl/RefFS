/** @file fuse_cpp_ramfs.hpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#ifndef fuse_ram_fs_hpp
#define fuse_ram_fs_hpp

#include "common.h"
#include "inode.hpp"
#include "cr_util.hpp"
#include "cr.h"

class Directory;

class FuseRamFs {
private:
    static const size_t kReadDirEntriesPerResponse = 255;
    static const size_t kReadDirBufSize = 384;
    static const size_t kInodeReclamationThreshold = 256;
    static const fsblkcnt_t kTotalBlocks = 8388608;
    static const fsfilcnt_t kTotalInodes = 1048576;
    static const unsigned long kFilesystemId = 0xc13f944870434d8f;
    static const size_t kMaxFilenameLength = 1024;
    
    static std::vector<Inode *> Inodes;
    static std::shared_mutex inodesRwSem;
    static std::shared_mutex crMutex;
    static std::queue<fuse_ino_t> DeletedInodes;
    static std::mutex deletedInodesMutex;
    static struct statvfs m_stbuf;
    static std::shared_mutex stbufMutex;

    static std::mutex renameMutex;
    
public:
    static struct fuse_lowlevel_ops FuseOps;
    
private:
    static long do_create_node(Directory *parent, const char *name, mode_t mode, dev_t dev, const struct fuse_ctx *ctx, const char *symlink = nullptr);
    static fuse_ino_t RegisterInode(Inode *inode_p, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid);
    static fuse_ino_t NextInode();
    static int checkpoint(uint64_t key);
    static void invalidate_kernel_states();
    static int restore(uint64_t key);

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

    static void UpdateInode(fuse_ino_t ino, Inode *newInode) {
        std::unique_lock<std::shared_mutex> writelk(inodesRwSem);
        Inodes[ino] = newInode;
    }

    static fuse_ino_t PopOneDeletedInode() {
        std::lock_guard<std::mutex> lk(deletedInodesMutex);
        fuse_ino_t ino = DeletedInodes.front();
        DeletedInodes.pop();
        return ino;
    }
    
public:
    FuseRamFs(fsblkcnt_t blocks = 0, fsfilcnt_t inodes = 0);
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
    static void FuseIoctl(fuse_req_t req, fuse_ino_t ino, int cmd, void *arg, struct fuse_file_info *fi, unsigned flags, const void *in_buf, size_t in_bufsz, size_t out_bufsz);

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
        std::unique_lock<std::shared_mutex> lk(stbufMutex);
        m_stbuf.f_bfree -= blocksAdded;
        m_stbuf.f_bavail -= blocksAdded;
    }
    static void UpdateUsedInodes(ssize_t inodesAdded) {
        std::unique_lock<std::shared_mutex> lk(stbufMutex);
        m_stbuf.f_ffree -= inodesAdded;
        m_stbuf.f_favail -= inodesAdded;
    }
    static void FsStat(struct statvfs *out) {
        std::shared_lock<std::shared_mutex> lk(stbufMutex);
        *out = m_stbuf;
    }

    static Inode *GetInode(fuse_ino_t ino) {
        std::shared_lock<std::shared_mutex> readlk(inodesRwSem);
        try {
            return Inodes.at(ino);
        } catch (std::out_of_range e) {
            return nullptr;
        }
    }

    /* Check if the file system can handle the increased size */
    static bool CheckHasSpaceFor(Inode *inode, ssize_t incSize) {
        if (incSize <= 0) {
            return true;
        }
        size_t oldBlocks, newSize;
        if (inode) {
            oldBlocks = inode->UsedBlocks();
            newSize = inode->Size() + incSize;
        } else {
            oldBlocks = 0;
            newSize = incSize;
        }
        size_t newBlocks = get_nblocks(newSize, Inode::BufBlockSize);
        if (newBlocks <= oldBlocks) {
            return true;
        } else {
            std::shared_lock<std::shared_mutex> lk(stbufMutex);
            return m_stbuf.f_bfree >= (newBlocks - oldBlocks);
        }
    }

    static fsfilcnt_t GetFreeInodes() {
        std::shared_lock<std::shared_mutex> lk(stbufMutex);
        return m_stbuf.f_ffree;
    }
};

#endif /* fuse_ram_fs_hpp */
