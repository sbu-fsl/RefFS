/*
 * This file is part of RefFS.
 * 
 * Copyright (c) 2020-2024 Yifei Liu
 * Copyright (c) 2020-2024 Pei Liu
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

#ifndef cr_util_hpp
#define cr_util_hpp

#include "inode.hpp"
#include "file.hpp"
#include "directory.hpp"
#include "special_inode.hpp"
#include "symlink.hpp"

typedef std::tuple<std::vector<Inode *>, std::queue<fuse_ino_t>, struct statvfs> verifs2_state;

int insert_state(uint64_t key, const verifs2_state &fs_states_vec);

verifs2_state find_state(uint64_t key);

int remove_state(uint64_t key);

std::unordered_map<uint64_t, verifs2_state> get_state_pool();

void clear_states();

#ifdef DUMP_TESTING
void dump_File(File* file);
void dump_Directory(Directory* dir);
void dump_SpecialInode(SpecialInode* sinode);
void dump_SymLink(SymLink* symlink);
int dump_inodes_verifs2(std::vector<Inode *> Inodes, std::queue<fuse_ino_t> DeletedInodes, 
                        std::string info);
int dump_state_pool();
bool isExistInDeleted(fuse_ino_t curr_ino, std::queue<fuse_ino_t> DeletedInodes);
void print_ino_queue(std::queue<fuse_ino_t> DeletedInodes);
#endif

#endif