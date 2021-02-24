#ifndef any_node_hpp
#define any_node_hpp

#include "file.hpp"
#include "directory.hpp"
#include "special_inode.hpp"
#include "symlink.hpp"

class AnyInode : public File, public Directory, public SpecialInode, public SymLink {
public:
    AnyInode (const AnyInode &obj) {};
    AnyInode();

    AnyInode() : File() {};
    AnyInode& operator= (const File& obj) {  }

    ~AnyInode();

};

#endif /* any_node_hpp */