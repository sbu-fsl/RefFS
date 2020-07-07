/** @file fuse_cpp_ramfs.cpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#if !defined(FUSE_USE_VERSION) || FUSE_USE_VERSION < 30
#define FUSE_USE_VERSION 30
#endif

#include <vector>
#include <queue>
#include <map>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <cerrno>
#include <cassert>
#include <cstring>
#ifdef __APPLE__
#include <osxfuse/fuse/fuse_lowlevel.h>
#else
#include <fuse/fuse_lowlevel.h>
#endif
#include <unistd.h>

#include "inode.hpp"
#include "file.hpp"
#include "directory.hpp"
#include "special_inode.hpp"
#include "symlink.hpp"
#include "fuse_cpp_ramfs.hpp"
#include "util.hpp"

using namespace std;

/**
 All the Inode objects in the system.
 */
vector<Inode *> FuseRamFs::Inodes = vector<Inode *>();


/**
 The Inodes which have been deleted.
 */
queue<fuse_ino_t> FuseRamFs::DeletedInodes = queue<fuse_ino_t>();

/**
 The constants defining the capabilities and sizes of the filesystem.
 */
struct statvfs FuseRamFs::m_stbuf = {};

/**
 All the supported filesystem operations mapped to object-methods.
 */
struct fuse_lowlevel_ops FuseRamFs::FuseOps = {};


FuseRamFs::FuseRamFs()
{
    FuseOps.init        = FuseRamFs::FuseInit;
    FuseOps.destroy     = FuseRamFs::FuseDestroy;
    FuseOps.lookup      = FuseRamFs::FuseLookup;
    FuseOps.forget      = FuseRamFs::FuseForget;
    FuseOps.getattr     = FuseRamFs::FuseGetAttr;
    FuseOps.setattr     = FuseRamFs::FuseSetAttr;
    FuseOps.readlink    = FuseRamFs::FuseReadLink;
    FuseOps.mknod       = FuseRamFs::FuseMknod;
    FuseOps.mkdir       = FuseRamFs::FuseMkdir;
    FuseOps.unlink      = FuseRamFs::FuseUnlink;
    FuseOps.rmdir       = FuseRamFs::FuseRmdir;
    FuseOps.symlink     = FuseRamFs::FuseSymlink;
    FuseOps.rename      = FuseRamFs::FuseRename;
    FuseOps.link        = FuseRamFs::FuseLink;
    FuseOps.open        = FuseRamFs::FuseOpen;
    FuseOps.read        = FuseRamFs::FuseRead;
    FuseOps.write       = FuseRamFs::FuseWrite;
    FuseOps.flush       = FuseRamFs::FuseFlush;
    FuseOps.release     = FuseRamFs::FuseRelease;
    FuseOps.fsync       = FuseRamFs::FuseFsync;
    FuseOps.opendir     = FuseRamFs::FuseOpenDir;
    FuseOps.readdir     = FuseRamFs::FuseReadDir;
    FuseOps.releasedir  = FuseRamFs::FuseReleaseDir;
    FuseOps.fsyncdir    = FuseRamFs::FuseFsyncDir;
    FuseOps.statfs      = FuseRamFs::FuseStatfs;
    FuseOps.setxattr    = FuseRamFs::FuseSetXAttr;
    FuseOps.getxattr    = FuseRamFs::FuseGetXAttr;
    FuseOps.listxattr   = FuseRamFs::FuseListXAttr;
    FuseOps.removexattr = FuseRamFs::FuseRemoveXAttr;
    FuseOps.access      = FuseRamFs::FuseAccess;
    FuseOps.create      = FuseRamFs::FuseCreate;
    FuseOps.getlk       = FuseRamFs::FuseGetLock;
    
    m_stbuf.f_bsize   = Inode::BufBlockSize;   /* File system block size */
    m_stbuf.f_frsize  = Inode::BufBlockSize;   /* Fundamental file system block size */
    m_stbuf.f_blocks  = kTotalBlocks;          /* Blocks on FS in units of f_frsize */
    m_stbuf.f_bfree   = kTotalBlocks;          /* Free blocks */
    m_stbuf.f_bavail  = kTotalBlocks;          /* Blocks available to non-root */
    m_stbuf.f_files   = kTotalInodes;          /* Total inodes */
    m_stbuf.f_ffree   = kTotalInodes;          /* Free inodes */
    m_stbuf.f_favail  = kTotalInodes;          /* Free inodes for non-root */
    m_stbuf.f_fsid    = kFilesystemId;         /* Filesystem ID */
    m_stbuf.f_flag    = 0;                     /* Bit mask of values */
    m_stbuf.f_namemax = kMaxFilenameLength;    /* Max file name length */
}

FuseRamFs::~FuseRamFs()
{

}


/**
 Initializes the filesystem. Creates the root directory. The UID and GID are those
 of the creating process.

 @param userdata Any user data carried through FUSE calls.
 @param conn Information on the capabilities of the connection to FUSE.
 */
void FuseRamFs::FuseInit(void *userdata, struct fuse_conn_info *conn)
{
    m_stbuf.f_bfree  = m_stbuf.f_blocks;	/* Free blocks */
    m_stbuf.f_bavail = m_stbuf.f_blocks;	/* Blocks available to non-root */
    m_stbuf.f_ffree  = m_stbuf.f_files;	/* Free inodes */
    m_stbuf.f_favail = m_stbuf.f_files;	/* Free inodes for non-root */
    m_stbuf.f_flag   = 0;		/* Bit mask of values */
    
    // We start out with a special inode and a single directory (the root directory).
    Inode *inode_p;
    
    // For our root nodes, we'll set gid and uid to the ones the process is using.
    uid_t gid = getgid();
    
    // TODO: Should I be getting the effective UID instead?
    uid_t uid = getuid();
    inode_p = new SpecialInode(SPECIAL_INODE_TYPE_NO_BLOCK);
    RegisterInode(inode_p, 0, 0, gid, uid);
    
    Directory *root = new Directory();
    
    // I think that that the root directory should have a hardlink count of 3.
    // This is what I believe I've surmised from reading around.
    fuse_ino_t rootno = RegisterInode(root, S_IFDIR | 0777, 3, gid, uid);
    root->AddChild(string("."), rootno);
    root->AddChild(string(".."), rootno);
}


/**
 Destroys the filesystem.

 @param userdata Any user data carried through FUSE calls.
 */
void FuseRamFs::FuseDestroy(void *userdata)
{
    for(auto const& inode: Inodes) {
        delete inode;
    }
}


/**
 Looks up an inode given a parent the name of the inode.

 @param req The FUSE request.
 @param parent The parent inode.
 @param name The name of the child to look up.
 */
void FuseRamFs::FuseLookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    if (parent >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *parentInode = Inodes[parent];
    Directory *dir = dynamic_cast<Directory *>(parentInode);
    if (dir == NULL) {
        // The parent wasn't a directory. It can't have any children.
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    fuse_ino_t ino = dir->ChildInodeNumberWithName(string(name));
    if (ino == INO_NOTFOUND) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode = Inodes[ino];
    /* Return ENOENT if this inode has been deleted */
    if (inode == nullptr || inode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    inode->ReplyEntry(req);
}


/**
 Gets an inode's attributes.

 @param req The FUSE request.
 @param ino The inode to git the attributes from.
 @param fi The file info (information about an open file).
 */
void FuseRamFs::FuseGetAttr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    // Fail if the inode hasn't been created yet
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
    }
    
    Inode *inode = Inodes[ino];
    /* return enoent if this inode has been deleted */
    if (inode == nullptr || inode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    inode->ReplyAttr(req);
}


/**
 Sets the attributes on an inode.

 @param req The FUSE request.
 @param ino The inode.
 @param attr The incoming attributes.
 @param to_set A mask of all incoming attributes that should be applied to the inode.
 @param fi The file info (information about an open file).
 */
void FuseRamFs::FuseSetAttr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi)
{
    // Fail if the inode hasn't been created yet
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
    }
    
    Inode *inode = Inodes[ino];
    /* return enoent if this inode has been deleted */
    if (inode == nullptr || inode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    inode->ReplySetAttr(req, attr, to_set);
}

/**
 Opens a directory.

 @param req The FUSE request.
 @param ino The directory inode.
 @param fi The file info (information about an open file).
 */
void FuseRamFs::FuseOpenDir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode = Inodes[ino];
    /* return enoent if this inode has been deleted */
    if (inode == nullptr || inode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }   
    // You can't open a file with 'opendir'. Check for this.
    File *file = dynamic_cast<File *>(inode);
    if (file != NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    // TODO: Handle permissions on files:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    fuse_reply_open(req, fi);
}


/**
 Closes a directory.

 @param req The FUSE request.
 @param ino The directory inode.
 @param fi The file info (information about an open file).
 */
void FuseRamFs::FuseReleaseDir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode = Inodes[ino];
    /* return enoent if this inode has been deleted */
    if (inode == nullptr || inode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }   
    // You can't close a file with 'closedir'. Check for this.
    File *file = dynamic_cast<File *>(inode);
    if (file != NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    // TODO: Handle permissions on files:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    fuse_reply_err(req, 0);
}


/**
 Reads a directory.

 @param req The FUSE request.
 @param ino The directory inode.
 @param size The maximum response size.
 @param off The offset into the list of children.
 @param fi The file info (information about an open file).
 */
void FuseRamFs::FuseReadDir(fuse_req_t req, fuse_ino_t ino, size_t size,
                             off_t off, struct fuse_file_info *fi)
{
    (void) fi;
    
    size_t numInodes = Inodes.size();
    // TODO[resolved]: Node may also be deleted.
    if (ino >= numInodes) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    map<string, fuse_ino_t>::const_iterator *childIterator = (map<string, fuse_ino_t>::const_iterator *) off;
    
    Inode *inode = Inodes[ino];
    /* return ENOENT if this inode has been deleted */
    if (inode == nullptr || inode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    Directory *dir = dynamic_cast<Directory *>(inode);
    if (dir == NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    if (childIterator != NULL && *childIterator == dir->Children().end()) {
        //delete childIterator;
        // This is the case where we've been called after we've sent all the children. End
        // with an empty buffer.
        fuse_reply_buf(req, NULL, 0);
        return;
    }
    
    // Loop through and put children into a buffer until we have either:
    // a) exceeded the passed in size parameter, or
    // b) filled the maximum number of children per response, or
    // c) come to the end of the directory listing
    //
    // In the case of (a), we won't know if this is the case until we've
    // added a child and exceeded the size. In that case, we need to back up.
    // In the process, we may end up exceeding our buffer size for this
    // resonse. In that case, increase the buffer size and add the child again.
    //
    // We must exercise care not to re-send children because one may have been
    // added in the middle of our map of children while we were sending them.
    // This is why we access the children with an iterator (instead of using
    // some sort of index).
    
    struct stat stbuf;
    memset(&stbuf, 0, sizeof(stbuf));
    
    // Pick the lesser of the max response size or our max size.
    size_t bufSize = FuseRamFs::kReadDirBufSize < size ? FuseRamFs::kReadDirBufSize : size;
    char *buf = (char *) malloc(bufSize);
    if (buf == NULL) {
        fuse_reply_err(req, ENOMEM);
    }

    // We'll assume that off is 0 when we start. This means that
    // childIterator hasn't been newed up yet.
    size_t bytesAdded = 0;
    size_t entriesAdded = 0;
    if (childIterator == NULL) {
        childIterator = new map<string, fuse_ino_t>::const_iterator(dir->Children().begin());
    }
    
    while (entriesAdded < FuseRamFs::kReadDirEntriesPerResponse &&
           *childIterator != dir->Children().end()) {
        fuse_ino_t child_ino = (*childIterator)->second;
        Inode *childInode = Inodes[child_ino];
        if (childInode == nullptr)
            continue;

        stbuf = childInode->GetAttr();
        stbuf.st_ino = (*childIterator)->second;
        
        // TODO: We don't look at sticky bits, etc. Revisit this in the future.
//        Inode &childInode = Inodes[stbuf.st_ino];
//        Directory *childDir = dynamic_cast<Directory *>(&childInode);
//        if (childDir == NULL) {
//            // This must be a file.
//            stbuf.st_mode
//        }
        
        size_t oldSize = bytesAdded;
        bytesAdded += fuse_add_direntry(req,
                                        buf + bytesAdded,
                                        bufSize - bytesAdded,
                                        (*childIterator)->first.c_str(),
                                        &stbuf,
                                        (off_t) childIterator);
        if (bytesAdded > bufSize) {
            // Oops. There wasn't enough space for that last item. Back up and exit.
            --(*childIterator);
            bytesAdded = oldSize;
            break;
        } else {
            ++(*childIterator);
            ++entriesAdded;
        }
    }

    fuse_reply_buf(req, buf, bytesAdded);
    free(buf);
}

void FuseRamFs::FuseOpen(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    size_t numInodes = Inodes.size();
    // TODO: Node may also be deleted.
    if (ino >= numInodes) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode = Inodes[ino];
    /* return ENOENT if this inode has been deleted */
    if (inode == nullptr || inode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    // You can't open a dir with 'open'. Check for this.
    Directory *dir = dynamic_cast<Directory *>(inode);
    if (dir != NULL) {
        fuse_reply_err(req, EISDIR);
        return;
    }
    
    // TODO: Handle permissions on files:
//    else if ((fi->flags & 3) != O_RDONLY)
//        fuse_reply_err(req, EACCES);
    
    // TODO: We seem to be able to delete a file and copy it back without a new inode being created. The only evidence is the open call. How do we handle this?

    fuse_reply_open(req, fi);
}

void FuseRamFs::FuseRelease(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];
    /* return ENOENT if this inode has been deleted */
    if (inode_p == nullptr || inode_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    // You can't release a dir with 'close'. Check for this.
    Directory *dir = dynamic_cast<Directory *>(inode_p);
    if (dir != NULL) {
        fuse_reply_err(req, EISDIR);
        return;
    }
    
    // TODO: Handle permissions on files:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    fuse_reply_err(req, 0);
}

void FuseRamFs::FuseFsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    fuse_reply_err(req, 0);
}

void FuseRamFs::FuseFsyncDir(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];
    
    // You can only sync a dir with 'fsyncdir'. Check for this.
    Directory *dir_p = dynamic_cast<Directory *>(inode_p);
    if (dir_p == NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    fuse_reply_err(req, 0);
}

void FuseRamFs::FuseMknod(fuse_req_t req, fuse_ino_t parent, const char *name,
                         mode_t mode, dev_t rdev)
{
    if (parent >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *parentInode = Inodes[parent];
    /* return ENOENT if this inode has been deleted */
    if (parentInode == nullptr || parentInode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    // You can only make something inside a directory
    Directory *parentDir_p = dynamic_cast<Directory *>(parentInode);
    if (parentDir_p == NULL) {
        fuse_reply_err(req, EISDIR);
        return;
    }

    /* Don't use an existing name */
    if (parentDir_p->ChildInodeNumberWithName(string(name)) != INO_NOTFOUND) {
        fuse_reply_err(req, EEXIST);
        return;
    }
    
    // TODO: Handle permissions on dirs. You can't just create anything you please!:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    const struct fuse_ctx* ctx_p = fuse_req_ctx(req);

    long ino = do_create_node(parentDir_p, name, mode, rdev, ctx_p);
    
    if (ino > 0) {
        Inodes[ino]->ReplyEntry(req);
    } else {
        fuse_reply_err(req, -ino);
    }
}

/* do_create_node: Helper function to create filesystem object
 * 
 * @param[in] parent:   Parent directory object
 * @param[in] name:     The name of the new object
 * @param[in] mode:     Mode
 * @param[in] dev:      Device number (required only if creating a device)
 * @param[in] ctx:      FUSE context
 * 
 * @return: If successful, return the inode number of created node.
 *          Otherwise, return a negative error code.
 */
long FuseRamFs::do_create_node(Directory *parent, const char *name, mode_t mode, dev_t dev, const struct fuse_ctx *ctx)
{
    Inode *new_node;
    nlink_t links = 1;

    /* Create object based on mode */
    if (S_ISREG(mode)) {
        new_node = new File();
    } else if (S_ISDIR(mode)) {
        new_node = new Directory();
        links = 2;
    } else if (S_ISCHR(mode)) {
        new_node = new SpecialInode(SPECIAL_INODE_CHAR_DEV, dev);
    } else if (S_ISBLK(mode)) {
        new_node = new SpecialInode(SPECIAL_INODE_BLOCK_DEV, dev);
    } else if (S_ISFIFO(mode)) {
        new_node = new SpecialInode(SPECIAL_INODE_FIFO);
    } else if (S_ISSOCK(mode)) {
        new_node = new SpecialInode(SPECIAL_INODE_SOCK);
    } else {
        return -EINVAL;
    }

    /* Register the new inode into file system */
    fuse_ino_t ino = RegisterInode(new_node, mode, links, ctx->gid, ctx->uid);

    /* Special treatment for directories */
    if (S_ISDIR(mode)) {
        /* Initialize the new directory: Add '.' and '..' */
        Directory *dir_p = dynamic_cast<Directory *>(new_node);
        dir_p->AddChild(string("."), ino);
        dir_p->AddChild(string(".."), parent->GetAttr().st_ino);
        parent->AddHardLink();
    }

    // Insert the new entry into the directory.
    parent->AddChild(string(name), ino);
    return ino;
}

void FuseRamFs::FuseMkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode)
{
    if (parent >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *parentInode = Inodes[parent];
    
    // You can only make something inside a directory
    Directory *parentDir_p = dynamic_cast<Directory *>(parentInode);
    if (parentDir_p == nullptr) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }

    /* Avoid making an existing directory */
    if (parentDir_p->ChildInodeNumberWithName(name) != INO_NOTFOUND) {
        fuse_reply_err(req, EEXIST);
        return;
    }
    
    // TODO: Handle permissions on dirs. You can't just create anything you please!:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    const struct fuse_ctx* ctx_p = fuse_req_ctx(req);

    long ino = do_create_node(parentDir_p, name, mode | S_IFDIR, 0, ctx_p);
   
    if (ino > 0) {
        Inodes[ino]->ReplyEntry(req);
    } else {
        fuse_reply_err(req, -ino);
    }
}

void FuseRamFs::FuseUnlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    
    size_t numInodes = Inodes.size();
    if (parent >= numInodes) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *parentInode = Inodes[parent];
    /* return ENOENT if this inode has been deleted */
    if (parentInode == nullptr || parentInode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    // You can only delete something inside a directory
    Directory *parentDir_p = dynamic_cast<Directory *>(parentInode);
    if (parentDir_p == NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    // TODO: Handle permissions on dirs. You can't just delete anything you please!:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    // Return an error if the child doesn't exist.
    fuse_ino_t ino = parentDir_p->ChildInodeNumberWithName(string(name));
    if (ino == -1) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    // Point the name to the deleted block
    parentDir_p->RemoveChild(string(name));
    
    Inode *inode_p = Inodes[ino];
    // TODO: Any way we can fail here? What if the inode doesn't exist? That probably indicates
    // a problem that happened earlier.
    
    // Update the number of hardlinks in the target
    inode_p->RemoveHardLink();
    
    // Reply with no error. TODO: Where is ESUCCESS?
    fuse_reply_err(req, 0);
}

void FuseRamFs::FuseRmdir(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    if (parent >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *parentInode = Inodes[parent];
    /* return ENOENT if this inode has been deleted */
    if (parentInode == nullptr || parentInode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    // You can only delete something inside a directory
    Directory *parentDir_p = dynamic_cast<Directory *>(parentInode);
    if (parentDir_p == NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    // TODO: Handle permissions on dirs. You can't just delete anything you please!:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    // Return an error if the child doesn't exist.
    fuse_ino_t ino = parentDir_p->ChildInodeNumberWithName(string(name));
    if (ino == -1) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    /* Prevent removing '.': raise error if ino == parent */
    if (ino == parent) {
        fuse_reply_err(req, EINVAL);
        return;
    }

    Inode *inode_p = Inodes[ino];
    // TODO: Any way we can fail here? What if the inode doesn't exist? That probably indicates
    // a problem that happened earlier.
    if (inode_p == nullptr || inode_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    Directory *dir_p = dynamic_cast<Directory *>(inode_p);
    if (dir_p == NULL) {
        // Someone tried to rmdir on something that wasn't a directory.
        fuse_reply_err(req, ENOTDIR);
        return;
    }

    /* Cannot remove if the directory is not empty */
    /* 2 is a base size: each dir contains at least '.' and '..' */
    /* This also prevents removing '..' */
    if (dir_p->Children().size() > 2) {
        fuse_reply_err(req, ENOTEMPTY);
        return;
    }
    
    parentDir_p->RemoveChild(name);
    // Update the number of hardlinks in the parent dir
    parentDir_p->RemoveHardLink();
    
    // Remove the hard links to this dir so it can be cleaned up later
    // TODO: What if there's a real hardlink to this dir? Hardlinks to dirs allowed?
    // NOTE: No, hardlinks to dirs are not allowed. 
    while (!dir_p->HasNoLinks()) {
        dir_p->RemoveHardLink();
    }
    
    // Reply with no error. 
    fuse_reply_err(req, 0);
}

void FuseRamFs::FuseForget(fuse_req_t req, fuse_ino_t ino, unsigned long nlookup)
{
    Inode *inode_p = Inodes[ino];
    
    if (inode_p == nullptr) {
        return;
    }

    inode_p->Forget(req, nlookup);
    
    if (inode_p->Forgotten())
    {
        if (inode_p->HasNoLinks())
        {
            // Let's just delete this inode and free memory.
            size_t blocks_freed = inode_p->UsedBlocks();
            delete inode_p;
            Inodes[ino] = nullptr;
            FuseRamFs::UpdateUsedInodes(-blocks_freed);

            // Insert the inode number to DeletedInodes queue for slot reclaim
            DeletedInodes.push(ino);
        }
        else
        {
            // TODO: Verify that this only happens on unmount. It's OK on unmount but bad elsewhere.
        }
    }
    
    // Note that there's no reply here. That was done in the steps above this check.
}

void FuseRamFs::FuseWrite(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi)
{
    // TODO: Fuse seems to have problems writing with a null (buf) buffer.
    if (buf == NULL) {
        fuse_reply_err(req, EINVAL);
        return;
    }
    
    // TODO: Node may also be deleted.
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino]; 
    if (inode_p == nullptr || inode_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    // TODO: Handle info in fi.

    inode_p->WriteAndReply(req, buf, size, off);
}

void FuseRamFs::FuseFlush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    // Inode *inode_p = Inodes[ino];
    
    // TODO: Handle info in fi.
    
    fuse_reply_err(req, 0);
}


void FuseRamFs::FuseRead(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
    // TODO: Node may also be deleted.
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];
    
    if (inode_p == nullptr || inode_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    // TODO: Handle info in fi.
    
    inode_p->ReadAndReply(req, size, off);
}


void FuseRamFs::FuseRename(fuse_req_t req, fuse_ino_t parent, const char *name, fuse_ino_t newparent, const char *newname)
{
    // Make sure the parents still exists.
    if (parent >= Inodes.size() || newparent >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *parentInode = Inodes[parent];
    Inode *newParentInode = Inodes[newparent];
    
    // Make sure it's not an already deleted inode
    if (parentInode == nullptr || parentInode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    if (newParentInode == nullptr || newParentInode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    // You can only rename something inside a directory
    Directory *parentDir = dynamic_cast<Directory *>(parentInode);
    if (parentDir == NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }

    Directory *newParentDir = dynamic_cast<Directory *>(newParentInode);
    if (newParentDir == NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    // TODO: Handle permissions on dirs. You can't just rename anything you please!:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    // Return an error if the source doesn't exist.
    fuse_ino_t srcIno = parentDir->ChildInodeNumberWithName(string(name));
    if (srcIno == INO_NOTFOUND) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    Inode *srcInode = Inodes[srcIno];
    
    // Look for an existing child with the same name in the new parent
    // directory
    fuse_ino_t existingIno = newParentDir->ChildInodeNumberWithName(string(newname));
    Inode *existingInode = nullptr;
    /* If the newname (or destination) already exists, rename() should replace
     * the destination with the source.
     * HOWEVER, rename() will NOT replace if the destination is a non-empty
     * directory, or the destination is a directory while the source is not,
     * or the source is a directory but the dest is not.
     */
    if (existingIno != INO_NOTFOUND) {
        existingInode = Inodes[parent];
        /* src is directory but dest is not: return ENOTDIR */
        if (S_ISDIR(srcInode->GetAttr().st_mode) && !S_ISDIR(existingInode->GetAttr().st_mode)) {
            fuse_reply_err(req, ENOTDIR);
            return;
        }
        /* Vise versa: return EISDIR */
        if (!S_ISDIR(srcInode->GetAttr().st_mode) && S_ISDIR(existingInode->GetAttr().st_mode)) {
            fuse_reply_err(req, EISDIR);
            return;
        }
        /* If dest is a non-empty directory, return ENOTEMPTY */
        if (S_ISDIR(existingInode->GetAttr().st_mode)) {
            Directory *existingDir = dynamic_cast<Directory *>(existingInode);
            /* If the mode indicates a directory but it's not,
               something bad might have happened */
            assert(existingDir);
            if (existingDir->Children().size() > 2) {
                fuse_reply_err(req, ENOTEMPTY);
                return;
            }
        }
        /* Otherwise, let's replace the existing dest */
        newParentDir->UpdateChild(string(newname), srcIno);
        existingInode->RemoveHardLink();
        parentDir->RemoveChild(string(name));
        fuse_reply_err(req, 0);
    } else {
        int err = newParentDir->AddChild(string(newname), srcIno);
        if (err == 0)
            parentDir->RemoveChild(string(name));
        fuse_reply_err(req, err);
    }
}

void FuseRamFs::FuseLink(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname)
{
    // Make sure the source inode and the parent exists.
    if (ino >= Inodes.size() || newparent >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *parent = Inodes[newparent];
    Inode *src = Inodes[ino];
    
    if (src == nullptr || src->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    if (parent == nullptr || parent->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    // The new parent must be a directory. TODO: Do we need this check? Will FUSE
    // ever give us a parent that isn't a dir? Test this.
    Directory *parentDir = dynamic_cast<Directory *>(parent);
    if (parentDir == NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }

    /* If newname exists, we do NOT overwrite it */
    fuse_ino_t existingIno = parentDir->ChildInodeNumberWithName(string(newname));
    // Type is unsigned so we have to explicitly check for largest value. TODO: Refactor please.
    if (existingIno != INO_NOTFOUND) {
        // There's already a child with that name. Return an error.
        fuse_reply_err(req, EEXIST);
    }
    
    // Create the new name and point it to the inode.
    parentDir->AddChild(string(newname), ino);
    
    // Update the number of hardlinks in the target
    src->AddHardLink();
    
    parent->ReplyEntry(req);
}

void FuseRamFs::FuseSymlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name)
{
    if (parent >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *parent_p = Inodes[parent];
    
    if (parent_p == nullptr || parent_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    // You can only make something inside a directory
    Directory *dir = dynamic_cast<Directory *>(parent_p);
    if (dir == NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }

    /* We don't overwrite if name exists in parent directory */
    if (dir->ChildInodeNumberWithName(string(name)) != INO_NOTFOUND) {
        fuse_reply_err(req, EEXIST);
        return;
    }
    
    // TODO: Handle permissions on dirs. You can't just make symlinks anywhere:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    const struct fuse_ctx* ctx_p = fuse_req_ctx(req);
    
    Inode *inode_p = new SymLink(string(link));
    fuse_ino_t ino = RegisterInode(inode_p, S_IFLNK | 0777, 1, ctx_p->gid, ctx_p->uid);
    
    // Insert the inode into the directory. TODO: What if it already exists?
    dir->AddChild(string(name), ino);
    
    inode_p->ReplyEntry(req);
}

void FuseRamFs::FuseReadLink(fuse_req_t req, fuse_ino_t ino)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];
    
    if (inode_p == nullptr || inode_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    // You can only readlink on a symlink
    SymLink *link_p = dynamic_cast<SymLink *>(inode_p);
    if (link_p == NULL) {
        fuse_reply_err(req, EINVAL);
        return;
    }
    
    // TODO: Handle permissions.
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    //const struct fuse_ctx* ctx_p = fuse_req_ctx(req);
    
    fuse_reply_readlink(req, link_p->Link().c_str());
}

void FuseRamFs::FuseStatfs(fuse_req_t req, fuse_ino_t ino)
{
    // TODO: Why were we given an inode? What do we do with it?
//    if (ino >= Inodes.size()) {
//        fuse_reply_err(req, ENOENT);
//        return;
//    }
//    
//    Inode *inode_p = Inodes[ino];
    
    fuse_reply_statfs(req, &m_stbuf);
}

#ifdef __APPLE__
void FuseRamFs::FuseSetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags, uint32_t position)
#else
void FuseRamFs::FuseSetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags)
#endif
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];
    
    if (inode_p == nullptr || inode_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

#ifndef __APPLE__
    uint32_t position = 0;
#endif

    inode_p->SetXAttrAndReply(req, string(name), value, size, flags, position);
}

#ifdef __APPLE__
void FuseRamFs::FuseGetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size, uint32_t position)
#else
void FuseRamFs::FuseGetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size)
#endif
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];
    
    if (inode_p == nullptr || inode_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

#ifndef __APPLE__
    uint32_t position = 0;
#endif
    
    inode_p->GetXAttrAndReply(req, string(name), size, position);
}

void FuseRamFs::FuseListXAttr(fuse_req_t req, fuse_ino_t ino, size_t size)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];

    if (inode_p == nullptr || inode_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    inode_p->ListXAttrAndReply(req, size);
}

void FuseRamFs::FuseRemoveXAttr(fuse_req_t req, fuse_ino_t ino, const char *name)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];
    
    if (inode_p == nullptr || inode_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    inode_p->RemoveXAttrAndReply(req, string(name));
}

void FuseRamFs::FuseAccess(fuse_req_t req, fuse_ino_t ino, int mask)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];
    
    if (inode_p == nullptr || inode_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    const struct fuse_ctx* ctx_p = fuse_req_ctx(req);
    
    inode_p->ReplyAccess(req, mask, ctx_p->gid, ctx_p->uid);
}

void FuseRamFs::FuseCreate(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi)
{
    if (parent >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *parent_p = Inodes[parent];
    if (parent_p == nullptr || parent_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    Directory *parentDir_p = dynamic_cast<Directory *>(parent_p);
    if (parentDir_p == NULL) {
        // The parent wasn't a directory. It can't have any children.
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    const struct fuse_ctx* ctx_p = fuse_req_ctx(req);
    
    Inode *inode_p = new File();

    // TODO: It looks like, according to the documentation, that this will never be called to
    // make a dir--only a file. Test to make sure this is true.
    fuse_ino_t ino = RegisterInode(inode_p, mode, 1, ctx_p->gid, ctx_p->uid);
    
    // Insert the inode into the directory. TODO: What if it already exists?
    parentDir_p->AddChild(string(name), ino);
    
    inode_p->ReplyCreate(req, fi);
}

void FuseRamFs::FuseGetLock(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];
    
    // TODO: implement locking
    //inode_p->ReplyGetLock(req, lock);
}

fuse_ino_t FuseRamFs::RegisterInode(Inode *inode_p, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid)
{
    // Either re-use a deleted inode or push one back depending on whether we're reclaiming inodes now or
    // not.
    fuse_ino_t ino;
    if (DeletedInodes.empty()) {
        Inodes.push_back(inode_p);
        ino = Inodes.size() - 1;
        FuseRamFs::UpdateUsedInodes(1);
    } else {
        ino = DeletedInodes.front();
        DeletedInodes.pop();
        Inodes[ino] = inode_p;
    }

    inode_p->Initialize(ino, mode, nlink, gid, uid);
    FuseRamFs::UpdateUsedBlocks(inode_p->UsedBlocks());
    return ino;
}
