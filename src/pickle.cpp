#include "common.h"
#include <sys/stat.h>
#include <mcfs/errnoname.h>
#include <exception>
#include <openssl/sha.h>
#include <sys/mman.h>

#include "inode.hpp"
#include "file.hpp"
#include "directory.hpp"
#include "symlink.hpp"
#include "special_inode.hpp"
#include "fuse_cpp_ramfs.hpp"

class pickle_error : public std::exception {
public:
    int _errno;
    std::string func;
    int line;

    pickle_error(int err, std::string func, int line) noexcept {
        _errno = err;
        this->func = func;
        this->line;
        fprintf(stderr, "Pickling error %s(%d) at %s:%d.\n",
                errnoname(_errno), _errno, func.c_str(), line);
    }

    pickle_error(const pickle_error& other) {
        _errno = other._errno;
        func = other.func;
        line = other.line;
    }

    const char *what() const noexcept {
        return errnoname(errno);
    }

    int get_errno() const noexcept {
        return _errno;
    }
};

struct state_file_header {
    size_t fsize;
    unsigned char hash[SHA256_DIGEST_LENGTH];
};

struct inode_state {
    bool exist;
    mode_t mode;
};

static void feed_hash(SHA256_CTX *hashctx, const void *data, size_t len) {
    if (hashctx == nullptr)
        return;
    int ret = SHA256_Update(hashctx, data, len);
    if (ret == 0)
        throw pickle_error(EPROTO, __func__, __LINE__);
}

static size_t write_to_file(int fd, const void *buf, size_t count) {
    errno = 0;
    size_t res = write(fd, buf, count);
    if (res < count) {
        throw pickle_error(errno, __func__, __LINE__);
    }
    return res;
}

static inline size_t write_and_hash(int fd, SHA256_CTX *hashctx,
                                    const void *data, size_t count) {
    size_t res = write_to_file(fd, data, count);
    feed_hash(hashctx, data, count);
    return res;
}

int pickle_file_system(int fd, std::vector<Inode *>& inodes,
                       std::queue<fuse_ino_t>& pending_delete_inodes,
                       struct statvfs &fs_stat, SHA256_CTX *hashctx) {
    /* Remember the current file cursor;
     * if pickling fails, move the cursor here. */
    off_t fpos = lseek(fd, 0, SEEK_CUR);
    try {
        // pickle statvfs
        write_and_hash(fd, hashctx, &fs_stat, sizeof(fs_stat));
        // pickle inodes
        size_t num_inodes = inodes.size();
        write_and_hash(fd, hashctx, &num_inodes, sizeof(num_inodes));
        for (size_t i = 0; i < num_inodes; ++i) {
            Inode *inode = inodes[i];
            struct inode_state iinfo = {0};
            if (inode == nullptr) {
                iinfo.exist = false;
                write_and_hash(fd, hashctx, &iinfo, sizeof(iinfo));
                continue;
            }
            size_t pickled_size = inode->GetPickledSize();
            void *data = malloc(pickled_size);
            if (data == nullptr) {
                throw pickle_error(ENOMEM, __func__, __LINE__);
            }
            /* Should not fail, because the buffer is preallocated */
            inode->Pickle(data);
            iinfo.mode = inode->GetMode();
            iinfo.exist = true;
            write_and_hash(fd, hashctx, &iinfo, sizeof(iinfo));
            write_and_hash(fd, hashctx, data, pickled_size);
        }
        // pickle the list of pending delete inodes
        size_t num_pending_delete = pending_delete_inodes.size();
        write_and_hash(fd, hashctx, &num_pending_delete,
                       sizeof(num_pending_delete));
        /* Note that pending_delete_inodes is a queue, therefore the only way
         * to iterate through it is to pop all the elements in a vector */
        for (size_t i = 0; i < num_pending_delete; ++i) {
            fuse_ino_t ino = pending_delete_inodes.front();
            write_and_hash(fd, hashctx, &ino, sizeof(ino));
            pending_delete_inodes.pop();
            pending_delete_inodes.push(ino);
        }

        /*size_t num_state_pool = state_pool.size();
        write_and_hash(fd, hashctx, &num_state_pool, sizeof(num_state_pool));
        for (const auto &state: state_pool) {
            uint64_t key=state.first;
            write_and_hash(fd, hashctx, &key, sizeof(key));
            std::tuple
                    <std::vector <Inode *>, std::queue<fuse_ino_t>,
                    struct statvfs> stored_states=state.second;
            std::vector <Inode *> stored_files = std::get<0>(stored_states);
            size_t num_stored_files = stored_files.size();
            write_and_hash(fd, hashctx, &num_stored_files, sizeof(num_stored_files));

            for(const auto &stored_file:stored_files){
                write_and_hash(fd, hashctx, &stored_file, sizeof(stored_file));
            }

            std::queue<fuse_ino_t> stored_DeletedInodes = std::get<1>(stored_states);
            size_t num_stored_DeletedInodes = stored_DeletedInodes.size();
            write_and_hash(fd, hashctx, &num_stored_DeletedInodes, sizeof(num_stored_DeletedInodes));

            for (size_t i = 0; i < num_stored_DeletedInodes; ++i) {
                fuse_ino_t ino = stored_DeletedInodes.front();
                write_and_hash(fd, hashctx, &ino, sizeof(ino));
                stored_DeletedInodes.pop();
                stored_DeletedInodes.push(ino);
            }

            struct statvfs stored_m_stbuf = std::get<2>(stored_states);
            write_and_hash(fd, hashctx, &stored_m_stbuf, sizeof(stored_m_stbuf));
        }*/

    } catch (const pickle_error& e) {
        lseek(fd, fpos, SEEK_SET);
        return -e.get_errno();
    }
    return 0;
}

static char *fetch_filepath(const char *cfgpath) {
    int cfgfd = open(cfgpath, O_RDONLY);
    if (cfgfd < 0)
        throw pickle_error(errno, __func__, __LINE__);

    char *path = (char *)calloc(PATH_MAX, 1);
    if (path == nullptr) {
        close(cfgfd);
        throw pickle_error(ENOMEM, __func__, __LINE__);
    }

    ssize_t res = read(cfgfd, path, PATH_MAX);
    if (res < 0) {
        close(cfgfd);
        throw pickle_error(errno, __func__, __LINE__);
    }
    
    size_t pathlen = strnlen(path, PATH_MAX);
    path = (char *)realloc(path, pathlen + 1);
    close(cfgfd);
    return path;
}

int FuseRamFs::pickle_verifs2(void) {
    char *path = nullptr;
    int fd = -1;
    int res = 0;
    try {
        path = fetch_filepath(VERIFS_PICKLE_CFG);
        fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (fd < 0)
            throw pickle_error(errno, __func__, __LINE__);
        // skip the header
        res = lseek(fd, sizeof(struct state_file_header), SEEK_SET);
        if (res < 0)
            throw pickle_error(errno, __func__, __LINE__);
        // pickle the file system data and metadata
        SHA256_CTX hashctx;
        SHA256_Init(&hashctx);
        res = pickle_file_system(fd, FuseRamFs::Inodes,
                                 FuseRamFs::DeletedInodes,
                                 FuseRamFs::m_stbuf, &hashctx);
        if (res < 0)
            throw pickle_error(errno, __func__, __LINE__);
        // lastly: write the header
        off_t filelen = lseek(fd, 0, SEEK_CUR);
        struct state_file_header header = {0};
        header.fsize = filelen;
        SHA256_Final(header.hash, &hashctx);
        res = lseek(fd, 0, SEEK_SET);
        if (res < 0)
            throw pickle_error(errno, __func__, __LINE__);
        res = write(fd, &header, sizeof(header));
        if (res >= 0) {
            res = 0;
        } else {
            res = -errno;
        }
    } catch (const pickle_error &e) {
        res = -e.get_errno();
    }
    if (path)
        free(path);
    if (fd > 0)
        close(fd);
    return res;
}

static int verify_file_size(int fd, size_t expected) {
    struct stat info;
    int res = fstat(fd, &info);
    if (res < 0)
        return errno;
    return (info.st_size == expected) ? 0 : -1;
}

static int verify_file_integrity(int fd, unsigned char *expected) {
    const size_t blocksize = 4096;
    unsigned char hashres[SHA256_DIGEST_LENGTH] = {0};
    char buf[blocksize];
    SHA256_CTX hashctx;

    int res = SHA256_Init(&hashctx);
    if (res == 0)
        return -2;

    ssize_t readsz;
    while ((readsz = read(fd, buf, blocksize)) > 0) {
        SHA256_Update(&hashctx, buf, readsz);
    }
    if (readsz < 0)
        return errno;

    SHA256_Final(hashres, &hashctx);
    return (memcmp(hashres, expected, SHA256_DIGEST_LENGTH) == 0) ? 0 : -3;
}

/* verify_state_file: Verify the integrity of the state file
 *
 * @param[fd] - File descriptor
 *
 * @return: 0 for success; positive integer for an error number resulted from
 * failed system call; -1 for file size mismatch; -2 for hash error; -3 for
 * mismatch sha256 hash digest.
 */
int verify_state_file(int fd) {
    struct state_file_header header;
    // read the header
    int res = lseek(fd, 0, SEEK_SET);
    if (res < 0)
        return errno;

    ssize_t readsz = read(fd, &header, sizeof(header));
    if (readsz < 0)
        return errno;
    else if (readsz < sizeof(header))
        return ENOSPC;

    // validate if the file size and the size recorded in the header match
    if ((res = verify_file_size(fd, header.fsize)) != 0)
        return res;

    // start reading the whole file and calculating the hash
    if ((res = verify_file_integrity(fd, header.hash)) != 0)
        return res;

    lseek(fd, sizeof(header), SEEK_SET);
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
                     std::queue<fuse_ino_t>& pending_delete_inodes,
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
            struct inode_state iinfo;
            memcpy(&iinfo, ptr, sizeof(iinfo));
            ptr += sizeof(iinfo);
            if (!iinfo.exist)
                continue;

            size_t res;
            const void *ptr2 = (const void *)ptr;
            if (S_ISREG(iinfo.mode)) {
                File *file = new File();
                res = file->Load(ptr2);
                inodes.push_back(file);
            } else if (S_ISDIR(iinfo.mode)) {
                auto *dir = new Directory();
                res = dir->Load(ptr2);
                inodes.push_back(dir);
            } else if (S_ISLNK(iinfo.mode)) {
                auto *link = new SymLink();
                res = link->Load(ptr2);
                inodes.push_back(link);
            } else if (S_ISCHR(iinfo.mode) || S_ISBLK(iinfo.mode) ||
                S_ISSOCK(iinfo.mode) || S_ISFIFO(iinfo.mode) || iinfo.mode == 0) {
                auto *special = new SpecialInode();
                res = special->Load(ptr2);
                inodes.push_back(special);
            } else {
                throw pickle_error(EINVAL, __func__, __LINE__);
            }

            if (res == 0) {
                throw pickle_error(ENOMEM, __func__, __LINE__);
            }
            ptr += res;
        }
        size_t num_pending_delete;
        memcpy(&num_pending_delete, ptr, sizeof(num_pending_delete));
        ptr += sizeof(num_pending_delete);
        for (size_t i = 0; i < num_pending_delete; ++i) {
            fuse_ino_t ino;
            memcpy(&ino, ptr, sizeof(ino));
            ptr += sizeof(ino);
            pending_delete_inodes.push(ino);
        }


    } catch (const pickle_error& e) {
        return -e.get_errno();
    }
    return ptr - (const char *)data;
}

static size_t get_fsize(int fd) {
    struct stat info{};
    int res = fstat(fd, &info);
    if (res < 0) {
        throw pickle_error(errno, __func__, __LINE__);
    }
    return info.st_size;
}

int FuseRamFs::load_verifs2(void) {
    char *path = nullptr;
    void *mapped = nullptr;
    int fd = -1, res = 0;
    size_t content_size = 0;
    try {
        path = fetch_filepath(VERIFS_LOAD_CFG);
        fd = open(path, O_RDONLY);
        if (fd < 0)
            throw pickle_error(errno, __func__, __LINE__);
        // verify integrity of the input state file
        res = verify_state_file(fd);
        if (res > 0) {
            throw pickle_error(res, __func__, __LINE__);
        } else if (res == -1) {
            // res == -1: size mismatches
            throw pickle_error(EMSGSIZE, __func__, __LINE__);
        } else if (res == -2) {
            // res == -2: error occurred when hashing
            throw pickle_error(EPROTO, __func__, __LINE__);
        } else if (res == -3) {
            // res == -3: hash mismatch
            throw pickle_error(EINVAL, __func__, __LINE__);
        }
        // mmap the file
        content_size = get_fsize(fd);
        mapped = mmap(nullptr, content_size, PROT_READ, MAP_SHARED, fd, 0);
        if (mapped == MAP_FAILED)
            throw pickle_error(errno, __func__, __LINE__);
        // load the file system
        FuseRamFs::Inodes.clear();
        while (!FuseRamFs::DeletedInodes.empty())
            FuseRamFs::DeletedInodes.pop();
        char *ptr = (char *)mapped + sizeof(state_file_header);
        load_file_system(ptr, FuseRamFs::Inodes, FuseRamFs::DeletedInodes,
                         FuseRamFs::m_stbuf);
    } catch (const pickle_error &e) {
        res = -e.get_errno();
    }
    if (mapped)
        munmap(mapped, content_size);
    if (fd >= 0)
        close(fd);
    if (path)
        free(path);
    
    return res;
}