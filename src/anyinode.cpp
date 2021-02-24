#include "common.h"
#include "anyinode.hpp"

AnyInode::~AnyInode() {
    free(m_buf);
}