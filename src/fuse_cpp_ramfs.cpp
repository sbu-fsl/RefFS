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
 True if the filesystem is reclaiming inodes at the present point in time.
 */
bool FuseRamFs::m_reclaimingInodes = false;


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
    m_reclaimingInodes = false;
    
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
    
    cout << "init" << endl;
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
    
    cout << "destroy" << endl;
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
    
    // TODO: What do we do if the inode was deleted?
    Inode *inode = Inodes[ino];
    
    cout << "lookup for " << ino << ". nlookup++" << endl;
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
    
    // TODO: What do we do if the inode was deleted?
    Inode *inode = Inodes[ino];
    
    cout << "getattr for " << ino << endl;
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
    
    // TODO: What do we do if the inode was deleted?
    Inode *inode = Inodes[ino];
    
    cout << "setattr for " << ino << endl;
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
    
    // You can't open a file with 'opendir'. Check for this.
    File *file = dynamic_cast<File *>(inode);
    if (file != NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    // TODO: Handle permissions on files:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    cout << "opendir for " << ino << endl;
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
    
    // You can't close a file with 'closedir'. Check for this.
    File *file = dynamic_cast<File *>(inode);
    if (file != NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    // TODO: Handle permissions on files:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    cout << "releasedir for " << ino << endl;
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
    
    cout << "readdir for " << ino;
    
    size_t numInodes = Inodes.size();
    // TODO: Node may also be deleted.
    if (ino >= numInodes) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    map<string, fuse_ino_t>::const_iterator *childIterator = (map<string, fuse_ino_t>::const_iterator *) off;
    
    Inode *inode = Inodes[ino];
    Directory *dir = dynamic_cast<Directory *>(inode);
    if (dir == NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    if (childIterator != NULL && *childIterator == dir->Children().end()) {
        cout << " Was going to delete childIterator at " << (void *) childIterator << " pointing to " << (void *) &(**childIterator) << ". Let it leak instead" << endl;
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
        cerr << "*** fatal error: cannot allocate memory" << endl;
        fuse_reply_err(req, ENOMEM);
    }

    // We'll assume that off is 0 when we start. This means that
    // childIterator hasn't been newed up yet.
    size_t bytesAdded = 0;
    size_t entriesAdded = 0;
    if (childIterator == NULL) {
        childIterator = new map<string, fuse_ino_t>::const_iterator(dir->Children().begin());
        cout << " with new iterator at " << (void *) childIterator << " pointing to " << (void *) &(**childIterator);
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
            cout << ". backed iterator at " << (void *) childIterator << " to point to " << (void *) &(**childIterator);
            bytesAdded = oldSize;
            break;
        } else {
            ++(*childIterator);
            cout << ". advanced iterator at " << (void *) childIterator << " to point to " << (void *) &(**childIterator);
            ++entriesAdded;
        }
    }

    cout << endl;
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

    cout << "open for " << ino << ". with flags " << fi->flags << endl;
    fuse_reply_open(req, fi);
}

void FuseRamFs::FuseRelease(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];
    
    // You can't release a dir with 'close'. Check for this.
    Directory *dir = dynamic_cast<Directory *>(inode_p);
    if (dir != NULL) {
        fuse_reply_err(req, EISDIR);
        return;
    }
    
    // TODO: Handle permissions on files:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    cout << "release for " << ino << endl;
    fuse_reply_err(req, 0);
}

void FuseRamFs::FuseFsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    cout << "fysnc for " << ino << endl;
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
    
    cout << "fsyncdir for " << ino << endl;
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
    
    // You can only make something inside a directory
    Directory *parentDir_p = dynamic_cast<Directory *>(parentInode);
    if (parentDir_p == NULL) {
        fuse_reply_err(req, EISDIR);
        return;
    }
    
    // TODO: Handle permissions on dirs. You can't just create anything you please!:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    const struct fuse_ctx* ctx_p = fuse_req_ctx(req);

    
    Inode *inode_p;
    
    nlink_t nlink = 0;
    if (S_ISDIR(mode)) {
        inode_p = new Directory();
        nlink = 2;
        
        // Update the number of hardlinks in the parent dir
        parentDir_p->AddHardLink();
    } else if (S_ISREG(mode)) {
        inode_p = new File();
        nlink = 1;
    } else {
        // TODO: Handle
        // S_ISBLK
        // S_ISCHR
        // S_ISDIR
        // S_ISFIFO
        // S_ISREG
        // S_ISLNK
        // S_ISSOCK
        // ...instead of returning this error.
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    // TODO: Handle: S_ISCHR S_ISBLK S_ISFIFO S_ISLNK S_ISSOCK S_TYPEISMQ S_TYPEISSEM S_TYPEISSHM
    assert(inode_p != NULL);

    fuse_ino_t ino = RegisterInode(inode_p, mode, nlink, ctx_p->gid, ctx_p->uid);
    
    // Insert the inode into the directory. TODO: What if it already exists?
    parentDir_p->UpdateChild(string(name), ino);
    
    // TODO: Is reply_entry only for directories? What about files?
    cout << "mknod for " << ino << ". nlookup++" << endl;
    inode_p->ReplyEntry(req);
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
    
    Directory *dir_p = new Directory();
    fuse_ino_t ino = RegisterInode(dir_p, mode | S_IFDIR, 2, ctx_p->gid, ctx_p->uid);

    // TODO: Handle error if adding things failed: Needs to rollback
    /* Initialize the new directory: Add '.' and '..' */
    dir_p->AddChild(string("."), ino);
    dir_p->AddChild(string(".."), parent);
    parentDir_p->AddHardLink();

    // Insert the inode into the directory. TODO: What if it already exists?
    parentDir_p->AddChild(string(name), ino);
    
    cout << "mkdir for " << ino << ". nlookup++" << endl;
    dir_p->ReplyEntry(req);
}

void FuseRamFs::FuseUnlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    cout << "unlink for " << name << " in " << parent << endl;
    
    size_t numInodes = Inodes.size();
    // TODO: Node may also be deleted.
    if (parent >= numInodes) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *parentInode = Inodes[parent];
    
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
    parentDir_p->UpdateChild(string(name), 0);
    
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
    
    Inode *inode_p = Inodes[ino];
    // TODO: Any way we can fail here? What if the inode doesn't exist? That probably indicates
    // a problem that happened earlier.
    
    Directory *dir_p = dynamic_cast<Directory *>(inode_p);
    if (dir_p == NULL) {
        // Someone tried to rmdir on something that wasn't a directory.
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    // Update the number of hardlinks in the parent dir
    parentDir_p->RemoveHardLink();
    
    // Remove the hard links to this dir so it can be cleaned up later
    // TODO: What if there's a real hardlink to this dir? Hardlinks to dirs allowed?
    while (!dir_p->HasNoLinks()) {
        dir_p->RemoveHardLink();
    }
    
    // Reply with no error. TODO: Where is ESUCCESS?
    fuse_reply_err(req, 0);
}

void FuseRamFs::FuseForget(fuse_req_t req, fuse_ino_t ino, unsigned long nlookup)
{
    // TODO: What if the inode doesn't exist!?!?!?!
    Inode *inode_p = Inodes[ino];
    
    cout << "forget for " << ino << ". nlookup -= " << nlookup << endl;
    inode_p->Forget(req, nlookup);
    
    if (inode_p->Forgotten())
    {
        if (inode_p->HasNoLinks())
        {
            // Add the inode to the deleted list. Anything in this list may be reclaimed.
            DeletedInodes.push(ino);
            if (DeletedInodes.size() > FuseRamFs::kInodeReclamationThreshold) {
                m_reclaimingInodes = true;
            }
            
            // At this point, directory entries will still point to this inode. This is
            // OK since we'll update those entries as soon as someone reads the directory.
            cout << "Added " << ino << " to deleted list" << endl;
        }
        else
        {
            // TODO: Verify that this only happens on unmount. It's OK on unmount but bad elsewhere.
            cout << "inode " << ino << " was forgotten but not deleted" << endl;
        }
    }
    
    // Note that there's no reply here. That was done in the steps above this check.
}

void FuseRamFs::FuseWrite(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi)
{
    // TODO: Fuse seems to have problems writing with a null (buf) buffer.
    if (buf == NULL) {
        fuse_reply_err(req, EPERM);
        return;
    }
    
    // TODO: Node may also be deleted.
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];
    
    // TODO: Handle info in fi.

    cout << "Write request for " << size << " bytes at " << off << " to " << ino << endl;
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
    
    cout << "flush for " << ino << endl;
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
    
    // TODO: Handle info in fi.
    
    cout << "read for " << size << " at " << off << " from " << ino << endl;
    
    inode_p->ReadAndReply(req, size, off);
}


void FuseRamFs::FuseRename(fuse_req_t req, fuse_ino_t parent, const char *name, fuse_ino_t newparent, const char *newname)
{
    // Make sure the parent still exists.
    if (parent >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *parentInode = Inodes[parent];
    
    // You can only rename something inside a directory
    Directory *parentDir = dynamic_cast<Directory *>(parentInode);
    if (parentDir == NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    // TODO: Handle permissions on dirs. You can't just rename anything you please!:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    // Return an error if the child doesn't exist.
    fuse_ino_t ino = parentDir->ChildInodeNumberWithName(string(name));
    if (ino == -1) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    // Make sure the new parent still exists.
    if (newparent >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *newParentInode = Inodes[newparent];
    
    // The new parent must be a directory. TODO: Do we need this check? Will FUSE
    // ever give us a parent that isn't a dir? Test this.
    Directory *newParentDir = dynamic_cast<Directory *>(newParentInode);
    if (newParentDir == NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    // Look for an existing child with the same name in the new parent
    // directory
    fuse_ino_t existingIno = newParentDir->ChildInodeNumberWithName(string(newname));
    // Type is unsigned so we have to explicitly check for largest value. TODO: Refactor please.
    if (existingIno != -1 && existingIno > 0) {
        // There's already a child with that name. Replace it.
        // TODO: What about directories with the same name?
        Inode *existingInode_p = Inodes[parent];
        cout << "Removing hard link to " << existingIno << endl;
        existingInode_p->RemoveHardLink();
    }
    
    // Update (or create) the new name and point it to the inode.
    newParentDir->UpdateChild(string(newname), ino);
    
    // Mark the old name as unused. TODO: Should we just delete the old name?
    parentDir->UpdateChild(string(name), 0);
    
    cout << "Rename " << name << " in " << parent << " to " << newname << " in " << newparent << endl;
    fuse_reply_err(req, 0);
}

void FuseRamFs::FuseLink(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname)
{
    // Make sure the new parent still exists.
    if (newparent >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[newparent];
    
    // The new parent must be a directory. TODO: Do we need this check? Will FUSE
    // ever give us a parent that isn't a dir? Test this.
    Directory *newParentDir_p = dynamic_cast<Directory *>(inode_p);
    if (newParentDir_p == NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    // Make target still exists.
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    inode_p = Inodes[ino];
    
    // Look for an existing child with the same name in the new parent
    // directory
    fuse_ino_t existingIno = newParentDir_p->ChildInodeNumberWithName(string(newname));
    // Type is unsigned so we have to explicitly check for largest value. TODO: Refactor please.
    if (existingIno != -1 && existingIno > 0) {
        // There's already a child with that name. Return an error.
        fuse_reply_err(req, EEXIST);
    }
    
    // Create the new name and point it to the inode.
    newParentDir_p->UpdateChild(string(newname), ino);
    
    // Update the number of hardlinks in the target
    inode_p->AddHardLink();
    
    cout << "link " << newname << " in " << newparent << " to " << ino << endl;
    inode_p->ReplyEntry(req);
}

void FuseRamFs::FuseSymlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name)
{
    if (parent >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *parent_p = Inodes[parent];
    
    // You can only make something inside a directory
    Directory *dir = dynamic_cast<Directory *>(parent_p);
    if (dir == NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    
    // TODO: Handle permissions on dirs. You can't just make symlinks anywhere:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    const struct fuse_ctx* ctx_p = fuse_req_ctx(req);
    
    
    Inode *inode_p = new SymLink(string(link));
    fuse_ino_t ino = RegisterInode(inode_p, S_IFLNK | 0755, 1, ctx_p->gid, ctx_p->uid);
    
    // Insert the inode into the directory. TODO: What if it already exists?
    dir->UpdateChild(string(name), ino);
    
    // TODO: Is reply_entry only for directories? What about files?
    cout << "symlink for " << ino << ". nlookup++" << endl;
    inode_p->ReplyEntry(req);
}

void FuseRamFs::FuseReadLink(fuse_req_t req, fuse_ino_t ino)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];
    
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
    
    
    // TODO: Is reply_entry only for directories? What about files?
    cout << "readlink for " << ino << endl;
    
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
    
    cout << "statfs for " << ino << endl;
    
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
    
#ifndef __APPLE__
    uint32_t position = 0;
#endif

    cout << "setxattr for " << ino << endl;
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
    
#ifndef __APPLE__
    uint32_t position = 0;
#endif
    
    cout << "getxattr for " << ino << endl;
    inode_p->GetXAttrAndReply(req, string(name), size, position);
}

void FuseRamFs::FuseListXAttr(fuse_req_t req, fuse_ino_t ino, size_t size)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];

    cout << "listxattr for " << ino << endl;
    inode_p->ListXAttrAndReply(req, size);
}

void FuseRamFs::FuseRemoveXAttr(fuse_req_t req, fuse_ino_t ino, const char *name)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];
    
    cout << "removexattr for " << ino << endl;
    inode_p->RemoveXAttrAndReply(req, string(name));
}

void FuseRamFs::FuseAccess(fuse_req_t req, fuse_ino_t ino, int mask)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];
    
    const struct fuse_ctx* ctx_p = fuse_req_ctx(req);
    
    cout << "access for " << ino << endl;
    
    inode_p->ReplyAccess(req, mask, ctx_p->gid, ctx_p->uid);
}

void FuseRamFs::FuseCreate(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi)
{
    if (parent >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *parent_p = Inodes[parent];
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
    parentDir_p->UpdateChild(string(name), ino);
    
    cout << "create for " << ino << " with name " << name << " in " << parent << endl;
    inode_p->ReplyCreate(req, fi);
}

void FuseRamFs::FuseGetLock(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock)
{
    if (ino >= Inodes.size()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    Inode *inode_p = Inodes[ino];
    
    cout << "getlk for " << ino << endl;
    // TODO: implement locking
    //inode_p->ReplyGetLock(req, lock);
}

fuse_ino_t FuseRamFs::RegisterInode(Inode *inode_p, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid)
{
    // Stop reclaiming inodes if there are no more to reclaim.
    if (DeletedInodes.size() == 0) {
        m_reclaimingInodes = false;
    }
    
    // Either re-use a deleted inode or push one back depending on whether we're reclaiming inodes now or
    // not.
    fuse_ino_t ino;
    if (m_reclaimingInodes) {
        ino = DeletedInodes.front();
        DeletedInodes.pop();
        Inode *del_p = Inodes[ino];
        FuseRamFs::UpdateUsedBlocks(-(del_p->UsedBlocks()));
        Inodes[ino] = inode_p;
        delete del_p;
    } else {
        Inodes.push_back(inode_p);
        ino = Inodes.size() - 1;
        FuseRamFs::UpdateUsedInodes(1);
    }

    inode_p->Initialize(ino, mode, nlink, gid, uid);
    return ino;
}
