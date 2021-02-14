#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cerrno>
#include "inode.hpp"
#include "cr.hpp"

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
