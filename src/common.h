#ifndef _COMMON_INCLUDE_H
#define _COMMON_INCLUDE_H

#include <vector>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <string>
#include <iostream>
#include <stdexcept>

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cassert>
#include <cstring>

#ifdef __APPLE__
#include <osxfuse/fuse/fuse_lowlevel.h>
#else
#include <fuse/fuse_lowlevel.h>
#endif
#include <unistd.h>

#include "util.hpp"

#endif