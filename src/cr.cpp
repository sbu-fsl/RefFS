#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cerrno>
#include "cr.hpp"

#define PRINT_CLASS(x) std::cout << "Class Name: " << typeid(x).name() << std::endl
#define PRINT_VAL(x) std::cout << #x" : " << x << std::endl

bool isExistInDeleted(fuse_ino_t curr_ino, std::queue<fuse_ino_t> DeletedInodes);
void print_ino_queue(std::queue<fuse_ino_t> DeletedInodes);

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

/* Dump functionalites to verify Checkpoint/Restore APIs */

void dump_File(File* file)
{
  PRINT_VAL((char*)(file->m_buf));
}

void dump_Directory(Directory* dir)
{
  for (auto& t : dir->m_children){
    std::cout << "Child name: " << t.first << "  |  " 
              << "Child inode number: " << t.second << "\n";
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


static int dump_each_inode_type(std::vector<Inode *>::iterator it)
{
  int ret = 0;
  if (S_ISREG((*it)->GetMode())){
    std::cout << "---Dump File" << std::endl;
    File *file = dynamic_cast<File *>(*it);
    dump_File(file);
  } 
  else if (S_ISDIR((*it)->GetMode())){
    std::cout << "---Dump Directory" << std::endl;
    Directory *dir = dynamic_cast<Directory *>(*it);
    dump_Directory(dir);
  } 
  else if (S_ISCHR((*it)->GetMode()) || S_ISBLK((*it)->GetMode()) || S_ISFIFO((*it)->GetMode())
              || S_ISSOCK((*it)->GetMode()))
  {
    std::cout << "---Dump SpecialInode" << std::endl;
    SpecialInode *sinode = dynamic_cast<SpecialInode *>(*it);
    dump_SpecialInode(sinode);
  } 
  else if (S_ISLNK((*it)->GetMode())){
    std::cout << "---Dump SymLink" << std::endl;
    SymLink *symlink = dynamic_cast<SymLink *>(*it);
    dump_SymLink(symlink);
  }
  else if ((*it)->GetMode() == 0){
    return ret;
  }
  else{
    std::cerr << "dumping inode has incorrect inode type " << (*it)->GetMode() << "\n";
    ret = -EINVAL;
  }
  return ret;
}


static int _dump_inodes_verifs2(std::vector<Inode *> Inodes, 
                                std::queue<fuse_ino_t> DeletedInodes)
{
  int ret; 
  for (std::vector<Inode *>::iterator it = Inodes.begin(); it != Inodes.end(); ++it){
    // Print current DeletedInodes
    print_ino_queue(DeletedInodes);
    int dist = std::distance(Inodes.begin(), it);
    if (dist == 0){
      continue;
    }
    // If this inode is not in DeletedInodes
    fuse_ino_t curr_ino = (fuse_ino_t)dist;
    if (!isExistInDeleted(curr_ino, DeletedInodes)){
      ret = dump_each_inode_type(it);
      if (ret != 0){
        return ret;
      }
    }
    else{
      std::cout << "\033[1;36m***Inode number " << curr_ino 
                << " had been deleted\033[0m\n";
    }
  }
  return ret;
}

bool isExistInDeleted(fuse_ino_t curr_ino, std::queue<fuse_ino_t> DeletedInodes)
{
  bool ret = false;
  while(!DeletedInodes.empty()){
    if (DeletedInodes.front() == curr_ino){
      ret = true;
      break;
    }
    DeletedInodes.pop();
  }
  return ret;
}

void print_ino_queue(std::queue<fuse_ino_t> DeletedInodes)
{
  std::cout << "Print out Queue: ";
  while (!DeletedInodes.empty())
  {
    std::cout << DeletedInodes.front() << " ";
    DeletedInodes.pop();
  }
  std::cout << std::endl;
}


int dump_inodes_verifs2(std::vector<Inode *> Inodes, std::queue<fuse_ino_t> DeletedInodes, 
                        std::string info)
{
  std::cout << "\033[1;35m" + info + "\033[0m" << std::endl;
  std::cout << "Size of Inodes: " << Inodes.size() << std::endl;
  int ret = _dump_inodes_verifs2(Inodes, DeletedInodes);
  std::cout << "\033[1;32mdump_inodes_verifs2 Finished!\033[0m" << std::endl;
  return ret;
}


int dump_state_pool()
{
  uint64_t key;
  int ret = 0;
  std::vector <Inode *> value_inode;
  int state_cnt = 0;
  for (const auto &each_state : state_pool) {
    std::cout << "\033[1;35mDump the "<< state_cnt <<"-th state\033[0m\n";
    state_cnt++;
    key = each_state.first;
    value_inode = each_state.second;
    std::cout << "Key: " << each_state.first << std::endl;
    std::cout << "value_inode.size(): " << value_inode.size() << std::endl;
    for (std::vector<Inode *>::iterator it = value_inode.begin(); it != value_inode.end(); ++it){
      ret = dump_each_inode_type(it);
      if (ret != 0){
        return ret;
      }
    }
  }
  return ret;
}

