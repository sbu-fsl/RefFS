#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cerrno>
#include "inode.hpp"
#if TESTING
#endif
#include "file.hpp"
#include "directory.hpp"
#include "special_inode.hpp"
#include "symlink.hpp"
#include "cr.hpp"

#define PRINT_CLASS(x) std::cout << "Class Name: " << typeid(x).name() << endl
#define PRINT_VAL(x) std::cout << #x"=" << x << std::endl

std::unordered_map<uint64_t, std::vector <Inode *> > state_pool;

int insert_state(uint64_t key, std::vector <Inode *> inode_vec)
{
  auto it = state_pool.find(key);
  if (it != state_pool.end()) {
    return -EEXIST;
  }
  state_pool.insert({key, inode_vec});
  return 0;
}

std::vector <Inode *> find_state(uint64_t key)
{
  auto it = state_pool.find(key);
  if (it == state_pool.end()) {
    return std::vector<Inode *>();
  } else {
    return it->second;
  }
}

int remove_state(uint64_t key)
{
  auto it = state_pool.find(key);
  if (it == state_pool.end()) {
    return -ENOENT;
  }
  state_pool.erase(it);
  return 0;
}

#if TESTING
#endif

int dump_File(File* file)
{



}

void dump_Directory(Directory* dir)
{

}

void dump_SpecialInode(SpecialInode* sinode)
{


}

void dump_SymLink(SymLink* symlink)
{

}


int dump_state_pool()
{
  uint64_t key;
  std::vector <Inode *> value_inode;
  for (const auto &each_state : state_pool) {
    key = each_state.first;
    value_inode = each_state.second;
    std::cout << "Key: " << each_state.first << std::endl;
    for (std::vector<Inode *>::iterator it = value_inode.begin(); it != value_inode.end(); ++it){
      if (S_ISREG((*it)->GetMode())){
        File *file = dynamic_cast<File *>(*it);
      } else if (S_ISDIR((*it)->GetMode())){
        Directory *dir = dynamic_cast<Directory *>(*it);
      } else if (S_ISCHR((*it)->GetMode()) || S_ISBLK((*it)->GetMode()) || S_ISFIFO((*it)->GetMode())
                  || S_ISSOCK((*it)->GetMode()))
      {
        SpecialInode *sinode = dynamic_cast<SpecialInode *>(*it);
      } else if (S_ISLNK((*it)->GetMode())){
        SymLink *symlink = dynamic_cast<SymLink *>(*it);
      }

    }


  }


}
