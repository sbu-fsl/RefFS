#include "common.h"
#include <sys/stat.h>
#include <mcfs/errnoname.h>
#include <exception>
#include <openssl/sha.h>

#include "inode.hpp"
#include "file.hpp"
#include "directory.hpp"
#include "symlink.hpp"
#include "special_inode.hpp"

class pickle_error : public std::exception {
public:
    int _errno;

    pickle_error(int err) noexcept {
        _errno = err;
    }

    pickle_error(const pickle_error& other) {
        _errno = other._errno;
    }

    const char *what() const noexcept {
        return errnoname(errno);
    }

    int get_errno() const noexcept {
        return _errno;
    }
};

void feed_hash(SHA256_CTX *hashctx, const void *data, size_t len) {
    if (hashctx == nullptr)
        return;
    int ret = SHA256_Update(hashctx, data, len);
    if (ret == 0)
        throw pickle_error(EPROTO);
}

size_t write_to_file(FILE *fp, const void *buf, size_t count) {
    errno = 0;
    size_t res = fwrite(buf, 1, count, fp);
    if (res < count) {
        throw pickle_error(errno);
    }
    return res;
}

static inline size_t write_and_hash(FILE *fp, SHA256_CTX *hashctx,
                                    const void *data, size_t count) {
    size_t res = write_to_file(fp, data, count);
    feed_hash(hashctx, data, count);
    return res;
}

int pickle_file_system(FILE *fp, std::vector<Inode *>& inodes,
                       std::queue<fuse_ino_t>& pending_delete_inodes,
                       struct statvfs &fs_stat, SHA256_CTX *hashctx) {
    /* Remember the current file cursor;
     * if pickling fails, move the cursor here. */
    long fpos = ftell(fp);
    try {
        // pickle statvfs
        write_and_hash(fp, hashctx, &fs_stat, sizeof(fs_stat));
        // pickle inodes
        size_t num_inodes = inodes.size();
        write_and_hash(fp, hashctx, &num_inodes, sizeof(num_inodes));
        for (size_t i = 0; i < num_inodes; ++i) {
            Inode *inode = inodes[i];
            size_t pickled_size = inode->GetPickledSize();
            void *data = malloc(pickled_size);
            if (data == nullptr) {
                throw pickle_error(ENOMEM);
            }
            /* Should not fail, because the buffer is preallocated */
            inode->Pickle(data);
            mode_t filemode = inode->GetMode();
            write_and_hash(fp, hashctx, &filemode, sizeof(filemode));
            write_and_hash(fp, hashctx, data, pickled_size);
        }
        // pickle the list of pending delete inodes
        size_t num_pending_delete = pending_delete_inodes.size();
        write_and_hash(fp, hashctx, &num_pending_delete,
                       sizeof(num_pending_delete));
        /* Note that pending_delete_inodes is a queue, therefore the only way
         * to iterate through it is to pop all the elements in a vector */
        for (size_t i = 0; i < num_pending_delete; ++i) {
            fuse_ino_t ino = pending_delete_inodes.front();
            write_and_hash(fp, hashctx, &ino, sizeof(ino));
            pending_delete_inodes.pop();
            pending_delete_inodes.push(ino);
        }
    } catch (const pickle_error& e) {
        fseek(fp, fpos, SEEK_SET);
        return -e.get_errno();
    }
    return 0;
}

/* load_file_system: Load the file system from pickled data.
 * 
 * NOTE: load_file_system() expects a memory buffer or a mmap'ed area
 * instead of a FILE object.
 * 
 * @param[in]  data: pointer to the pickled data
 * @param[out] inodes: Inode table
 * @param[out] pending_del_inodes: The queue of pending deleted inodes
 * @param[out] fs_stat: File system statistics info
 * 
 * @return: bytes used
 */
ssize_t load_file_system(const void *data, std::vector<Inode *>& inodes,
                     std::queue<fuse_ino_t>& pending_del_inodes,
                     struct statvfs &fs_stat) {
    const char *ptr = (const char *)data;
    try {
        // load statvfs
        memcpy(&fs_stat, ptr, sizeof(fs_stat));
        ptr += sizeof(fs_stat);
        // load inodes
        size_t num_inodes;
        memcpy(&num_inodes, ptr, sizeof(num_inodes));
        ptr += sizeof(num_inodes);
        for (size_t i = 0; i < num_inodes; ++i) {
            mode_t filemode;
            size_t res;
            memcpy(&filemode, ptr, sizeof(filemode));
            ptr += filemode;
            const void *ptr2 = (const void *)ptr;

            if (S_ISREG(filemode)) {
                File *file = new File();
                res = file->Load(ptr2);
                inodes.push_back(file);
            } else if (S_ISDIR(filemode)) {
                Directory *dir = new Directory();
                res = dir->Load(ptr2);
                inodes.push_back(dir);
            } else if (S_ISLNK(filemode)) {
                SymLink *link = new SymLink();
                res = link->Load(ptr2);
                inodes.push_back(link);
            } else if (S_ISCHR(filemode) || S_ISBLK(filemode) ||
                S_ISSOCK(filemode) || S_ISFIFO(filemode) || filemode == 0) {
                SpecialInode *special = new SpecialInode();
                res = special->Load(ptr2);
                inodes.push_back(special);
            } else {
                throw pickle_error(EINVAL);
            }

            if (res == 0) {
                throw pickle_error(ENOMEM);
            }
            ptr += res;
        }
    } catch (const pickle_error& e) {
        return -e.get_errno();
    }
    return ptr - (const char *)data;
}