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