/** @file file.cpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#include "common.h"

#include "inode.hpp"
#include "fuse_cpp_ramfs.hpp"
#include "file.hpp"

File::~File() {
    free(m_buf);
}

int File::FileTruncate(size_t newSize) {
    size_t newBlocks = get_nblocks(newSize, File::BufBlockSize);
    size_t oldBlocks = Inode::UsedBlocks();
    size_t oldSize = Inode::Size();

    if (!FuseRamFs::CheckHasSpaceFor(this, newSize - oldSize)) {
        return -ENOSPC;
    }

    /* Realloc if needed */
    if (newBlocks != oldBlocks) {
        void *newbuf = realloc(m_buf, newBlocks * File::BufBlockSize);
        if (newbuf == nullptr && newBlocks > 0) {
            /* Probably because newsize exceeds limit;
             * return EINVAL in this case */
            return EINVAL;
        }
        if (newBlocks == 0) {
            newbuf = nullptr;
        }
        m_buf = newbuf;
    }

    /* If the file is expanded, zero out outstanding bytes */
    if (newSize > oldSize) {
        memset((char *)m_buf + oldSize, 0, newSize - oldSize);
    }

    /* Update size / block usage */
    FuseRamFs::UpdateUsedBlocks(newBlocks - oldBlocks);
    std::unique_lock<std::shared_mutex> lk(entryRwSem);
    m_fuseEntryParam.attr.st_blocks = newBlocks;
    m_fuseEntryParam.attr.st_size = newSize;
    
    /* Changes to file content: both mtime and ctime will change */
#ifdef __APPLE__
    clock_gettime(CLOCK_REALTIME, &(m_fuseEntryParam.attr.st_ctimespec));
    m_fuseEntryParam.attr.st_mtimespec = m_fuseEntryParam.attr.st_ctimespec;
#else
    clock_gettime(CLOCK_REALTIME, &(m_fuseEntryParam.attr.st_ctim));
    m_fuseEntryParam.attr.st_mtim = m_fuseEntryParam.attr.st_ctim;
#endif
    return 0;
}

int File::WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off) {
    // Allocate more memory if we don't have space.
    size_t newSize = off + size;
    size_t originalCapacity = Inode::BufBlockSize * File::UsedBlocks();
    size_t newBlocks = newSize/Inode::BufBlockSize + (newSize % Inode::BufBlockSize != 0);

    /* Request for more memory if write() expands the file */
    if (newSize > originalCapacity) {
        if (!FuseRamFs::CheckHasSpaceFor(this, newSize - File::Size())) {
            return fuse_reply_err(req, ENOSPC);
        }
        void *newBuf = realloc(m_buf, newBlocks * Inode::BufBlockSize);
        // If we ran out of memory, let the caller know that no bytes were
        // written.
        if (newBuf == NULL) {
            return fuse_reply_write(req, 0);;
        }
        
        // Update our buffer size
        m_buf = newBuf;
    }
    
    // Write to the buffer. TODO: Check if SRC and DST overlap.
    // We assume that there can be no buffer overflow since realloc has already
    // been called with the new size and offset. If realloc failed, we wouldn't
    // be here.
    memcpy((char *) m_buf + off, buf, size);

    /* Update size and block usage info */
    if (newSize > originalCapacity) {
        FuseRamFs::UpdateUsedBlocks(newBlocks - m_fuseEntryParam.attr.st_blocks);
    }

    std::unique_lock<std::shared_mutex> lk(entryRwSem);
    if (newSize > originalCapacity) {
        m_fuseEntryParam.attr.st_blocks = newBlocks;
        m_fuseEntryParam.attr.st_size = newSize;
    }
    
    /* Changes to file content: both mtime and ctime will change */
#ifdef __APPLE__
    clock_gettime(CLOCK_REALTIME, &(m_fuseEntryParam.attr.st_ctimespec));
    m_fuseEntryParam.attr.st_mtimespec = m_fuseEntryParam.attr.st_ctimespec;
#else
    clock_gettime(CLOCK_REALTIME, &(m_fuseEntryParam.attr.st_ctim));
    m_fuseEntryParam.attr.st_mtim = m_fuseEntryParam.attr.st_ctim;
#endif
    
    return fuse_reply_write(req, size);
}

int File::ReadAndReply(fuse_req_t req, size_t size, off_t off) {    
    // Don't start the read past our file size
    if (off > m_fuseEntryParam.attr.st_size) {
        return fuse_reply_buf(req, (const char *) m_buf, 0);
    }
    
    // Update access time. TODO: This could get very intensive. Some
    // filesystems buffer this with options at mount time. Look into this.
    // TODO: What do we do if this fails? Do we care? Log the event?
    std::unique_lock<std::shared_mutex> lk(entryRwSem);
#ifdef __APPLE__
    clock_gettime(CLOCK_REALTIME, &(m_fuseEntryParam.attr.st_atimespec));
#else
    clock_gettime(CLOCK_REALTIME, &(m_fuseEntryParam.attr.st_atim));
#endif
    
    // Handle reading past the file size as well as inside the size.
    size_t bytesRead = off + size > m_fuseEntryParam.attr.st_size ? m_fuseEntryParam.attr.st_size - off : size;
    
    // TODO: There are all sorts of other replies. What about them?
    return fuse_reply_buf(req, (const char *) m_buf + off, bytesRead);
}

