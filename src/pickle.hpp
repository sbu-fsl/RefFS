/*
 * This file is part of RefFS.
 * 
 * Copyright (c) 2020-2024 Yifei Liu
 * Copyright (c) 2020-2024 Wei Su
 * Copyright (c) 2020-2024 Erez Zadok
 * Copyright (c) 2020-2024 Stony Brook University
 * Copyright (c) 2020-2024 The Research Foundation of SUNY
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