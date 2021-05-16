#ifndef _PICKLE_HPP_
#define _PICKLE_HPP_

int pickle_file_system(int fd, std::vector<Inode *>& inodes,
                       std::queue<fuse_ino_t>& pending_delete_inodes,
                       struct statvfs &fs_stat, SHA256_CTX *hashctx);
int verify_state_file(int fd);
ssize_t load_file_system(const void *data, std::vector<Inode *>& inodes,
                         std::queue<fuse_ino_t>& pending_del_inodes,
                         struct statvfs &fs_stat);

#endif // _PICKLE_HPP_