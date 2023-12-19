/*
 * This file is part of RefFS.
 * 
 * Copyright (c) 2020-2024 Yifei Liu
 * Copyright (c) 2020-2024 Pei Liu
 * Copyright (c) 2020-2024 Wei Su
 * Copyright (c) 2020-2024 Erez Zadok
 * Copyright (c) 2020-2024 Stony Brook University
 * Copyright (c) 2020-2024 The Research Foundation of SUNY
 * Original Copyright (C) Peter Watkins
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

#ifndef _COMMON_INCLUDE_H
#define _COMMON_INCLUDE_H

#if !defined(FUSE_USE_VERSION) || FUSE_USE_VERSION < 30
#define FUSE_USE_VERSION 30
#endif

#include <vector>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <string>
#include <iostream>
#include <stdexcept>
#include <atomic>
#include <algorithm>
#include <unordered_map>

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cassert>
#include <cstring>
#include <ctime>
#include <climits>

#ifdef __APPLE__
#include <osxfuse/fuse/fuse_lowlevel.h>
#else
#include <fuse/fuse_lowlevel.h>
#endif
#include <unistd.h>
#include <sys/xattr.h>
#include <fcntl.h>

#include "util.hpp"

extern struct fuse_chan *ch;

#endif
