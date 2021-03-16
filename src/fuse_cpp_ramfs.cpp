/** @file fuse_cpp_ramfs.cpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#if !defined(FUSE_USE_VERSION) || FUSE_USE_VERSION < 30
#define FUSE_USE_VERSION 30
#endif

#include "common.h"

#include "inode.hpp"
#include "file.hpp"
#include "directory.hpp"
#include "special_inode.hpp"
#include "symlink.hpp"
#include "fuse_cpp_ramfs.hpp"

using namespace std;

/**
 All the Inode objects in the system.
 */
vector<Inode *> FuseRamFs::Inodes = vector<Inode *>();
std::shared_mutex FuseRamFs::inodesRwSem;
std::shared_mutex FuseRamFs::crMutex;

/**
 The Inodes which have been deleted.
 */
queue<fuse_ino_t> FuseRamFs::DeletedInodes = queue<fuse_ino_t>();
std::mutex FuseRamFs::deletedInodesMutex;

/**
 The constants defining the capabilities and sizes of the filesystem.
 */
struct statvfs FuseRamFs::m_stbuf = {};
std::shared_mutex FuseRamFs::stbufMutex;

std::mutex FuseRamFs::renameMutex;
/**
 All the supported filesystem operations mapped to object-methods.
 */
struct fuse_lowlevel_ops FuseRamFs::FuseOps = {};


FuseRamFs::FuseRamFs(fsblkcnt_t blocks, fsfilcnt_t inodes)
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
    FuseOps.ioctl       = FuseRamFs::FuseIoctl;
    
    if (blocks <= 0) {
        blocks = kTotalBlocks;
    }
    if (inodes <= 0) {
        inodes = kTotalInodes;
    }
    /* No need for locking because no other threads should
     * be accessing these attributes during construction */
    m_stbuf.f_bsize   = Inode::BufBlockSize;   /* File system block size */
    m_stbuf.f_frsize  = Inode::BufBlockSize;   /* Fundamental file system block size */
    m_stbuf.f_blocks  = blocks;                /* Blocks on FS in units of f_frsize */
    m_stbuf.f_bfree   = blocks;                /* Free blocks */
    m_stbuf.f_bavail  = blocks;                /* Blocks available to non-root */
    m_stbuf.f_files   = inodes;                /* Total inodes */
    m_stbuf.f_ffree   = inodes;                /* Free inodes */
    m_stbuf.f_favail  = inodes;                /* Free inodes for non-root */
    m_stbuf.f_fsid    = kFilesystemId;         /* Filesystem ID */
    m_stbuf.f_flag    = 0;                     /* Bit mask of values */
    m_stbuf.f_namemax = kMaxFilenameLength;    /* Max file name length */
}

FuseRamFs::~FuseRamFs()
{

}

int FuseRamFs::checkpoint(uint64_t key)
{
    // Lock
    std::unique_lock<std::shared_mutex> lk(crMutex);
    int ret = 0;
    std::vector <Inode *> copied_files = std::vector<Inode *>();

    mode_t inode_mode;

    File *file_inode_old;
    Directory *dir_inode_old;
    SpecialInode *special_inode_old;
    SymLink *symlink_inode_old;

    for (unsigned i = 0; i < Inodes.size(); i++)
    {
        if (Inodes[i] == nullptr){
            copied_files.push_back(nullptr);
            continue;
        }

        inode_mode = Inodes[i]->GetMode();
        if (S_ISREG(inode_mode)){
            file_inode_old = dynamic_cast<File *>(Inodes[i]);
            if (file_inode_old == NULL){
                ret = -EBADF;
                goto err;
            }

            File *file_inode_copy = new File(*file_inode_old);
            copied_files.push_back((Inode*) file_inode_copy);

        } else if (S_ISDIR(inode_mode)) {
            dir_inode_old = dynamic_cast<Directory *>(Inodes[i]);
            if (dir_inode_old == NULL){
                ret = -EBADF;
                goto err;
            }

            Directory *dir_copy = new Directory(*dir_inode_old);
            copied_files.push_back((Inode*) dir_copy);

        } else if (S_ISCHR(inode_mode) || S_ISBLK(inode_mode) || S_ISFIFO(inode_mode) || S_ISSOCK(inode_mode)) {
            special_inode_old = dynamic_cast<SpecialInode *>(Inodes[i]);
            if (special_inode_old == NULL){
                ret = -EBADF;
                goto err;
            }

            SpecialInode *special_inode_copy = new SpecialInode(*special_inode_old);
            copied_files.push_back((Inode*) special_inode_copy);

        } else if (S_ISLNK(inode_mode)) {
            symlink_inode_old = dynamic_cast<SymLink *>(Inodes[i]);
            if (symlink_inode_old == NULL){
                ret = -EBADF;
                goto err;
            }

            SymLink *symlink_inode_copy = new SymLink(*symlink_inode_old);
            copied_files.push_back((Inode*) symlink_inode_copy);
        }
        else if (inode_mode == 0){
            copied_files.push_back((Inode*) Inodes[i]);
        }
        else {
            std::cerr << "The checkpointed inode mode "<< inode_mode << " is not correct.\n";
            ret = -EINVAL;
            goto err;
        }
    }
    // insert state
    ret = insert_state(key, std::make_tuple(copied_files, DeletedInodes, m_stbuf));
    if (ret != 0){
        goto err;
    }
    #ifdef DUMP_TESTING
    ret = dump_inodes_verifs2(Inodes, DeletedInodes, "During/After the checkpoint():");
    if (ret != 0){
        goto err;
    }
    #endif
    return ret;
err:
    // clear copied_files vector
    std::cerr << "Checkpointing went to error.\n";
    std::vector<Inode *>().swap(copied_files);
    // Unlock
    return ret;
}

void FuseRamFs::invalidate_kernel_states()
{
    for (std::vector<Inode *>::iterator it = Inodes.begin(); it != Inodes.end(); ++it){
        if ((*it) == nullptr){
            continue;
        }
        /* Invalidate possible kernel inode cache */
        // if m_markedForDeletion is false (the inode exists and is not marked as deleted)
        if (!(*it)->m_markedForDeletion){
            fuse_lowlevel_notify_inval_inode(ch, (*it)->GetIno(), 0, 0);
        }
        /* Invalidate potential d-cache */
        if(S_ISDIR((*it)->GetMode())){
            Directory *parent_dir = dynamic_cast<Directory *>(*it);
            /* If parent_dir has child dir*/
            for (auto it_child = parent_dir->m_children.begin(); it_child != parent_dir->m_children.end(); ++it_child){
                if (it_child->second > 0 && it_child->first != "." && it_child->first != ".."){
                    fuse_lowlevel_notify_inval_entry(ch, (*it)->GetIno(), (it_child->first).c_str(), (it_child->first).size());
                }
            }
        }
    }
}


int FuseRamFs::restore(uint64_t key)
{
    // Lock
    std::unique_lock<std::shared_mutex> lk(crMutex);
    int ret = 0;
    #ifdef DUMP_TESTING
    ret = dump_inodes_verifs2(Inodes, DeletedInodes, "Before the restore():");

    if (ret != 0){
        return ret;
    }
    #endif
    std::tuple<std::vector <Inode *>, std::queue<fuse_ino_t>, struct statvfs> stored_states = find_state(key);

    std::vector <Inode *> stored_files = std::get<0>(stored_states);
    std::queue<fuse_ino_t> stored_DeletedInodes = std::get<1>(stored_states);
    struct statvfs stored_m_stbuf = std::get<2>(stored_states);

    if (stored_files.empty() && stored_DeletedInodes.empty()){
        ret = -ENOENT;
        std::cerr << "Not found state in state pool with key " << key << std::endl;
        return ret;
    }

    invalidate_kernel_states();

    // tmp_DeletedInodes stores the current DeletedInodes in case restoration failed
    std::queue<fuse_ino_t> tmp_DeletedInodes = DeletedInodes;
    // Restore DeletedInodes First
    DeletedInodes = stored_DeletedInodes;
    // Then restore m_stbuf
    struct statvfs tmp_m_stbuf = m_stbuf;
    m_stbuf = stored_m_stbuf;

    std::vector <Inode *> newfiles;
    mode_t inode_mode;

    File *file_inode_stored;
    Directory *dir_inode_stored;
    SpecialInode *special_inode_stored;
    SymLink *symlink_inode_stored;

    for (unsigned i = 0; i < stored_files.size(); i++)
    {
        if (stored_files[i] == nullptr){
            newfiles.push_back(nullptr);
            continue;
        }
        inode_mode = stored_files[i]->GetMode();
        if (S_ISREG(inode_mode)){
            file_inode_stored = dynamic_cast<File *>(stored_files[i]);
            if (file_inode_stored == NULL){
                ret = -EBADF;
                goto err;
            }
            File *file_new = new File(* file_inode_stored);
            newfiles.push_back((Inode*) file_new);
        }
        else if (S_ISDIR(inode_mode)) {
            dir_inode_stored = dynamic_cast<Directory *>(stored_files[i]);
            if (dir_inode_stored == NULL){
                ret = -EBADF;
                goto err;
            }
            Directory *dir_new = new Directory(*dir_inode_stored);
            newfiles.push_back((Inode*) dir_new);            
        }
        else if (S_ISCHR(inode_mode) || S_ISBLK(inode_mode) || S_ISFIFO(inode_mode) || S_ISSOCK(inode_mode)) {
            special_inode_stored = dynamic_cast<SpecialInode *>(stored_files[i]);
            if (special_inode_stored == NULL){
                ret = -EBADF;
                goto err;
            }
            SpecialInode *special_inode_new = new SpecialInode(*special_inode_stored);
            newfiles.push_back((Inode*) special_inode_new);
        }
        else if (S_ISLNK(inode_mode)){
            symlink_inode_stored = dynamic_cast<SymLink *>(stored_files[i]);
            if (symlink_inode_stored == NULL){
                ret = -EBADF;
                goto err;
            }

            SymLink *symlink_new = new SymLink(*symlink_inode_stored);
            newfiles.push_back((Inode*) symlink_new);
        }
        else if (inode_mode == 0){
            newfiles.push_back((Inode*) stored_files[i]);
        }
        else{
            fprintf(stderr, "The restoration inode mode %u is not correct.", inode_mode);
            ret = -EINVAL;
            goto err;
        }
    }
    // clear old Inodes
    std::vector<Inode *>().swap(Inodes);
    Inodes = newfiles;
    // clear stored_files
    std::vector<Inode *>().swap(stored_files);
    ret = remove_state(key);
    if (ret != 0){
        goto err;
    }
    #ifdef DUMP_TESTING
    ret = dump_inodes_verifs2(Inodes, DeletedInodes, "After the restore():");

    if (ret != 0){
        goto err;
    }
    #endif
    return 0;
err:
    std::cerr << "Restoration went to error.\n";
    std::vector<Inode *>().swap(newfiles);
    DeletedInodes = tmp_DeletedInodes;
    m_stbuf = tmp_m_stbuf;
    return ret;
}

void FuseRamFs::FuseIoctl(fuse_req_t req, fuse_ino_t ino, int cmd, void *arg,
                        struct fuse_file_info *fi, unsigned flags,
                        const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
    int ret;
    switch (cmd){
        case VERIFS2_CHECKPOINT:
            ret = checkpoint((uint64_t)arg);
            break;
        
        case VERIFS2_RESTORE:
            ret = restore((uint64_t)arg);
            break;
        
        default:
            std::cerr << "Function Not implemented in FuseIoctl.\n";
            ret = ENOSYS;
            break;
    }
    if (ret == 0) {
        fuse_reply_ioctl(req, 0, NULL, 0);
    } else {
        fuse_reply_err(req, -ret);
    }
}


/**
 Initializes the filesystem. Creates the root directory. The UID and GID are those
 of the creating process.

 @param userdata Any user data carried through FUSE calls.
 @param conn Information on the capabilities of the connection to FUSE.
 */
void FuseRamFs::FuseInit(void *userdata, struct fuse_conn_info *conn)
{
    /* No need for locking because no other threads should be
     * accessing these elements during f/s initialization */
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

    /* Enable ioctl on directory */
    conn->want |= FUSE_CAP_IOCTL_DIR;
}


/**
 Destroys the filesystem.

 @param userdata Any user data carried through FUSE calls.
 */
void FuseRamFs::FuseDestroy(void *userdata)
{
    /* No need for locking because it's destruction of the file system */
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
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *parentInode = GetInode(parent);
    if (parentInode == nullptr || parentInode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

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
    
    Inode *inode = GetInode(ino);
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
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *inode = GetInode(ino);
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
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *inode = GetInode(ino);
    /* return enoent if this inode has been deleted */
    if (inode == nullptr || inode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    /* Non-null fi means the setattr() is called by ftruncate(). */
    if (fi && (to_set & FUSE_SET_ATTR_SIZE)) {
        File *file = dynamic_cast<File *>(inode);
        /* Cannot ftruncate a non-regular file */
        if (file == nullptr) {
            if (S_ISDIR(inode->GetMode())) {
                fuse_reply_err(req, EISDIR);
            } else {
                fuse_reply_err(req, EINVAL);
            }
            return;
        }
        int ret = file->FileTruncate(attr->st_size);
        if (ret == 0) {
            file->ReplyAttr(req);
        } else {
            fuse_reply_err(req, ret);
        }
        return;
    }

    to_set &= (~FUSE_SET_ATTR_SIZE);
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
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *inode = GetInode(ino);
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
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *inode = GetInode(ino);
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
    std::shared_lock<std::shared_mutex> lk(crMutex);
    (void) fi;
   
    Inode *inode = GetInode(ino);
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
    
    Directory::ReadDirCtx *ctx;
    try {
        ctx = dir->PrepareReaddir(off);
    } catch (std::out_of_range &e) {
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
    
    while (entriesAdded < FuseRamFs::kReadDirEntriesPerResponse &&
           ctx->it != ctx->children.end()) {
        fuse_ino_t child_ino = ctx->it->second;
        Inode *childInode = GetInode(child_ino);
        if (childInode == nullptr || childInode->HasNoLinks()) {
            ++(ctx->it);
            continue;
        }

        childInode->GetAttr(&stbuf);
        stbuf.st_ino = ctx->it->second;
        
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
                                        ctx->it->first.c_str(),
                                        &stbuf,
                                        ctx->cookie);
        if (bytesAdded > bufSize) {
            // Oops. There wasn't enough space for that last item. Back up and exit.
            --(ctx->it);
            bytesAdded = oldSize;
            break;
        } else {
            ++(ctx->it);
            ++entriesAdded;
        }
    }

    fuse_reply_buf(req, buf, bytesAdded);
    std::free(buf);
}

void FuseRamFs::FuseOpen(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *inode = GetInode(ino);
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
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *inode_p = GetInode(ino);
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
    std::shared_lock<std::shared_mutex> lk(crMutex);
    if (GetInode(ino) == nullptr) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    fuse_reply_err(req, 0);
}

void FuseRamFs::FuseFsyncDir(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi)
{
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *inode_p = GetInode(ino);

    if (inode_p == nullptr || inode_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
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
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *parentInode = GetInode(parent);
    /* return ENOENT if this inode has been deleted */
    if (parentInode == nullptr || parentInode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    // You can only make something inside a directory
    Directory *parentDir_p = dynamic_cast<Directory *>(parentInode);
    if (parentDir_p == NULL) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }

    /* Check if the file system has free inode slots */
    if (GetFreeInodes() <= 0) {
        fuse_reply_err(req, ENOSPC);
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
        GetInode(ino)->ReplyEntry(req);
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
long FuseRamFs::do_create_node(Directory *parent, const char *name, mode_t mode, dev_t dev, const struct fuse_ctx *ctx, const char *symlink)
{
    Inode *new_node;
    nlink_t links = 1;

    /* Create object based on mode */
    try {
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
        } else if (S_ISLNK(mode)) {
            if (symlink == nullptr) {
                return -EINVAL;
            }
            new_node = new SymLink(std::string(symlink));
        } else {
            return -EINVAL;
        }
    } catch (std::bad_alloc &e) {
        return -ENOSPC;
    }

    if (GetFreeInodes() <= 0) {
        delete new_node;
        return -ENOSPC;
    }

    /* Register the new inode into file system */
    fuse_ino_t ino = RegisterInode(new_node, mode, links, ctx->gid, ctx->uid);
    int ret = 0;

    /* Special treatment for directories */
    if (S_ISDIR(mode)) {
        /* Initialize the new directory: Add '.' and '..' */
        Directory *dir_p = dynamic_cast<Directory *>(new_node);
        ret = dir_p->AddChild(string("."), ino);
        ret = dir_p->AddChild(string(".."), parent->GetIno());
    }

    // Insert the new entry into the directory.
    ret = parent->AddChild(string(name), ino);
    /* If the first AddChild failed with ENOSPC/ENOMEM, the second one
     * will certainly fail because the space is already run out */
    if (ret < 0) {
        FuseRamFs::UpdateUsedInodes(-1);
        FuseRamFs::UpdateUsedBlocks(new_node->UsedBlocks());
        delete new_node;
        FuseRamFs::DeleteInode(ino);
        return ret;
    }
    /* Only add hard link to the parent dir if everything above succeeded */
    if (S_ISDIR(mode)) {
        parent->AddHardLink();
    }
    return ino;
}

void FuseRamFs::FuseMkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode)
{
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *parentInode = GetInode(parent);
    
    if (parentInode == nullptr || parentInode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    // You can only make something inside a directory
    Directory *parentDir_p = dynamic_cast<Directory *>(parentInode);
    if (parentDir_p == nullptr) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }

    if (GetFreeInodes() <= 0) {
        fuse_reply_err(req, ENOSPC);
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
        GetInode(ino)->ReplyEntry(req);
    } else {
        fuse_reply_err(req, -ino);
    }
}

void FuseRamFs::FuseUnlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *parentInode = GetInode(parent);
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
    if (ino == INO_NOTFOUND) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    // Point the name to the deleted block
    parentDir_p->RemoveChild(string(name));
    
    Inode *inode_p = GetInode(ino);
    // TODO: Any way we can fail here? What if the inode doesn't exist? That probably indicates
    // a problem that happened earlier.
    assert(inode_p);
    
    // Update the number of hardlinks in the target
    inode_p->RemoveHardLink();

    // Reply with no error. TODO: Where is ESUCCESS?
    fuse_reply_err(req, 0);
}

void FuseRamFs::FuseRmdir(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *parentInode = GetInode(parent);
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
    if (ino == INO_NOTFOUND) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    /* Prevent removing '.': raise error if ino == parent */
    if (ino == parent) {
        fuse_reply_err(req, EINVAL);
        return;
    }

    Inode *inode_p = GetInode(ino);
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
    if (!dir_p->IsEmpty()) {
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
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *inode_p = GetInode(ino);
    
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
            /* Atomically erase the record in inodes table and
             * push this ino to the DeletedInodes list */
            DeleteInode(ino);
            FuseRamFs::UpdateUsedInodes(-1);
            FuseRamFs::UpdateUsedBlocks(-blocks_freed);
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
    std::shared_lock<std::shared_mutex> lk(crMutex);
    // TODO: Fuse seems to have problems writing with a null (buf) buffer.
    if (buf == NULL) {
        fuse_reply_err(req, EINVAL);
        return;
    }
    
    Inode *inode_p = GetInode(ino);
    if (inode_p == nullptr || inode_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    // TODO: Handle info in fi.

    inode_p->WriteAndReply(req, buf, size, off);
}

void FuseRamFs::FuseFlush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    // TODO: Handle info in fi.
    
    fuse_reply_err(req, 0);
}


void FuseRamFs::FuseRead(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *inode_p = GetInode(ino);
    
    if (inode_p == nullptr || inode_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    // TODO: Handle info in fi.
    
    inode_p->ReadAndReply(req, size, off);
}


void FuseRamFs::FuseRename(fuse_req_t req, fuse_ino_t parent, const char *name, fuse_ino_t newparent, const char *newname)
{
    std::shared_lock<std::shared_mutex> lk(crMutex);
    // Make sure the parents still exists.
    Inode *parentInode = GetInode(parent);
    Inode *newParentInode = GetInode(newparent);
    
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

    std::unique_lock<std::mutex> G(FuseRamFs::renameMutex, std::defer_lock);
    std::unique_lock<std::shared_mutex> L1(parentDir->DirLock(), std::defer_lock);
    std::unique_lock<std::shared_mutex> L2(newParentDir->DirLock(), std::defer_lock);
    
    // TODO: Handle permissions on dirs. You can't just rename anything you please!:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    
    // Return an error if the source doesn't exist.
    fuse_ino_t srcIno = parentDir->ChildInodeNumberWithName(string(name));
    Inode *srcInode = GetInode(srcIno);
    if (srcInode == nullptr || srcInode->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
 
    /* Lock directories */
    if (S_ISDIR(srcInode->GetMode())) {
        if (parent == newparent) {
            std::lock(G, L1);
        } else {
            std::lock(G, L1, L2);
        }
    } else {
        if (parent == newparent) {
            L1.lock();
        } else {
            std::lock(L1, L2);
        }
    }

    // Look for an existing child with the same name in the new parent
    // directory
    fuse_ino_t existingIno = newParentDir->_ChildInodeNumberWithName(string(newname));
    Inode *existingInode = GetInode(existingIno);

    /* If the newname (or destination) already exists, rename() should replace
     * the destination with the source.
     * HOWEVER, rename() will NOT replace if the destination is a non-empty
     * directory, or the destination is a directory while the source is not,
     * or the source is a directory but the dest is not.
     */
    if (existingInode != nullptr && !existingInode->HasNoLinks()) {
        /* src is directory but dest is not: return ENOTDIR */
        if (S_ISDIR(srcInode->GetMode()) && !S_ISDIR(existingInode->GetMode())) {
            fuse_reply_err(req, ENOTDIR);
            return;
        }
        /* If dest is a non-empty directory, return ENOTEMPTY */
        if (S_ISDIR(existingInode->GetMode())) {
            Directory *existingDir = dynamic_cast<Directory *>(existingInode);
            /* If the mode indicates a directory but it's not,
               something bad might have happened */
            assert(existingDir);
            if (!existingDir->IsEmpty()) {
                fuse_reply_err(req, ENOTEMPTY);
                return;
            }
        }
        /* Vise versa: return EISDIR */
        if (!S_ISDIR(srcInode->GetMode()) && S_ISDIR(existingInode->GetMode())) {
            fuse_reply_err(req, EISDIR);
            return;
        }

        /* Otherwise, let's replace the existing dest */
        newParentDir->_UpdateChild(string(newname), srcIno);
        parentDir->_RemoveChild(string(name));
        existingInode->RemoveHardLink();
        if (S_ISDIR(existingInode->GetMode())) {
            /* An empty dir has two hard links
             * so we need to decrement one more time */
            existingInode->RemoveHardLink();
            /* Decrement one link for the old parent because the source
             * dir has been moved out */
            parentDir->RemoveHardLink();
        }
        fuse_reply_err(req, 0);
    } else {
        /* If the destination does not exist */
        newParentDir->_AddChild(string(newname), srcIno);
        parentDir->_RemoveChild(string(name));
        if (S_ISDIR(srcInode->GetMode())) {
            /* Decrement one link for the old parent because the source
             * dir has been moved out */
            parentDir->RemoveHardLink();
            /* Increment one link for the new parent because of moving in */
            newParentDir->AddHardLink();
        }
        fuse_reply_err(req, 0);
    }
}

void FuseRamFs::FuseLink(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname)
{
    std::shared_lock<std::shared_mutex> lk(crMutex);
    // Make sure the source inode and the parent exists.
    Inode *parent = GetInode(newparent);
    Inode *src = GetInode(ino);
    
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
        return;
    }
    
    // Create the new name and point it to the inode.
    int ret = parentDir->AddChild(string(newname), ino);
    if (ret < 0) {
        fuse_reply_err(req, -ret);
        return;
    }
    
    // Update the number of hardlinks in the target
    src->AddHardLink();
    
    parent->ReplyEntry(req);
}

void FuseRamFs::FuseSymlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name)
{
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *parent_p = GetInode(parent);
    
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

    /* Check free space and free inodes */
    if (!FuseRamFs::CheckHasSpaceFor(nullptr, strnlen(name, PATH_MAX))) {
        fuse_reply_err(req, ENOSPC);
    }
    if (FuseRamFs::GetFreeInodes() <= 0) {
        fuse_reply_err(req, ENOSPC);
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

    long ino = do_create_node(dir, name, 0777 | S_IFLNK, 0, ctx_p, link);

    if (ino > 0) {
        FuseRamFs::GetInode(ino)->ReplyEntry(req);
    } else {
        fuse_reply_err(req, -ino);
    }
    
}

void FuseRamFs::FuseReadLink(fuse_req_t req, fuse_ino_t ino)
{
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *inode_p = GetInode(ino);
    
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
    std::shared_lock<std::shared_mutex> lk(crMutex);
    struct statvfs info;
    FuseRamFs::FsStat(&info);
    fuse_reply_statfs(req, &info);
}

#ifdef __APPLE__
void FuseRamFs::FuseSetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags, uint32_t position)
#else
void FuseRamFs::FuseSetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags)
#endif
{
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *inode_p = GetInode(ino);
    
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
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *inode_p = GetInode(ino);
    
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
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *inode_p = GetInode(ino);

    if (inode_p == nullptr || inode_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    inode_p->ListXAttrAndReply(req, size);
}

void FuseRamFs::FuseRemoveXAttr(fuse_req_t req, fuse_ino_t ino, const char *name)
{
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *inode_p = GetInode(ino);
    
    if (inode_p == nullptr || inode_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    inode_p->RemoveXAttrAndReply(req, string(name));
}

void FuseRamFs::FuseAccess(fuse_req_t req, fuse_ino_t ino, int mask)
{
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *inode_p = GetInode(ino);
    
    if (inode_p == nullptr || inode_p->HasNoLinks()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    const struct fuse_ctx* ctx_p = fuse_req_ctx(req);
    
    inode_p->ReplyAccess(req, mask, ctx_p->gid, ctx_p->uid);
}

void FuseRamFs::FuseCreate(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi)
{
    std::shared_lock<std::shared_mutex> lk(crMutex);
    Inode *parent_p = GetInode(parent);
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
    
    long ino = do_create_node(parentDir_p, name, mode | S_IFREG, 0, ctx_p);
    if (ino > 0) {
        FuseRamFs::GetInode(ino)->ReplyCreate(req, fi);
    } else {
        fuse_reply_err(req, -ino);
    }
}

void FuseRamFs::FuseGetLock(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock)
{
    // TODO: implement locking (Custom lock impl is only needed for distributed file systems)
    //inode_p->ReplyGetLock(req, lock);
}

fuse_ino_t FuseRamFs::RegisterInode(Inode *inode_p, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid)
{
    // Either re-use a deleted inode or push one back depending on whether we're reclaiming inodes now or
    // not.
    fuse_ino_t ino;
    if (DeletedInodes.empty()) {
        ino = FuseRamFs::AddInode(inode_p);
    } else {
        ino = PopOneDeletedInode();
        FuseRamFs::UpdateInode(ino, inode_p);
    }
    FuseRamFs::UpdateUsedInodes(1);

    inode_p->Initialize(ino, mode, nlink, gid, uid);
    FuseRamFs::UpdateUsedBlocks(inode_p->UsedBlocks());
    return ino;
}
