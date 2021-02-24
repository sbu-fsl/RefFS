#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cerrno>
#include "cr.hpp"

#define PRINT_CLASS(x) std::cout << "---------------Class Name: " << typeid(x).name() << std::endl
#define PRINT_VAL(x) std::cout << #x"=" << x << std::endl

std::unordered_map<uint64_t, std::vector <Inode *> > state_pool;

int insert_state(uint64_t key, std::vector <Inode *> inode_vec)
{
  auto it = state_pool.find(key);
  if (it != state_pool.end()) {
    return -EEXIST;
  }
  state_pool.insert({key, inode_vec});

  printf("Running dump_state_pool() to dump current states\n");
  dump_state_pool();

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

void dump_File(File* file)
{
  PRINT_VAL((char*)(file->m_buf));
}

void dump_Directory(Directory* dir)
{
for (auto& t : dir->m_children){
    std::cout << t.first << " " 
              << t.second << "\n";
  }
}

void dump_SpecialInode(SpecialInode* sinode)
{
  PRINT_VAL(sinode->m_type);
}

void dump_SymLink(SymLink* symlink)
{
  PRINT_VAL(symlink->m_link);
}

int dump_state_pool()
{
  std::cout << "Start dump_state_pool()..." << std::endl;
  std::cout << "state_pool size: " << state_pool.size() << std::endl;
  uint64_t key;
  int ret = 0;
  std::vector <Inode *> value_inode;
  for (const auto &each_state : state_pool) {
    std::cout << "For each state..." << std::endl;
    std::cout << "each state first:" << each_state.first << std::endl;
    std::cout << "each state second:" << each_state.second[1] << std::endl;
    std::cout << "each_state.second.size():" << each_state.second.size() << std::endl;
    key = each_state.first;
    for (int i = 0; i < each_state.second.size(); i++){
      if (i == 0){
        continue;
      }
      std::cout << "Start getting mode..." << std::endl;
      std::cout << "each_state.second: " << each_state.second[i]->GetIno() << std::endl;
    }

    value_inode = each_state.second;
    std::cout << "value_inode.size(): " << value_inode.size() << std::endl;
    std::cout << "Key: " << each_state.first << std::endl;
    for (std::vector<Inode *>::iterator it = value_inode.begin(); it != value_inode.end(); ++it){
      if (S_ISREG((*it)->GetMode())){
        std::cout << "Dump File..." << std::endl;
        File *file = dynamic_cast<File *>(*it);
        PRINT_CLASS(file);
        dump_File(file);
      } 
      else if (S_ISDIR((*it)->GetMode())){
        std::cout << "Dump Directory..." << std::endl;
        Directory *dir = dynamic_cast<Directory *>(*it);
        PRINT_CLASS(dir);
        dump_Directory(dir);
      } 
      else if (S_ISCHR((*it)->GetMode()) || S_ISBLK((*it)->GetMode()) || S_ISFIFO((*it)->GetMode())
                  || S_ISSOCK((*it)->GetMode()))
      {
        std::cout << "Dump SpecialInode..." << std::endl;
        SpecialInode *sinode = dynamic_cast<SpecialInode *>(*it);
        PRINT_CLASS(sinode);
        dump_SpecialInode(sinode);
      } 
      else if (S_ISLNK((*it)->GetMode())){
        std::cout << "Dump SymLink..." << std::endl;
        SymLink *symlink = dynamic_cast<SymLink *>(*it);
        PRINT_CLASS(symlink);
        dump_SymLink(symlink);
      }
      else{
        fprintf(stderr, "dump_state_pool has incorrect inode type %u", (*it)->GetMode());
        ret = -EINVAL;
        return ret;
      }
    }
  }
  return ret;
}

