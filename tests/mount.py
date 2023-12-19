#!/usr/bin/env python

#
# This file is part of RefFS.
# 
# Copyright (c) 2020-2024 Yifei Liu
# Copyright (c) 2020-2024 Wei Su
# Copyright (c) 2020-2024 Erez Zadok
# Copyright (c) 2020-2024 Stony Brook University
# Copyright (c) 2020-2024 The Research Foundation of SUNY
# Original Copyright (C) Peter Watkins
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# RefFS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#

from pathlib import Path
import subprocess
import os
import sys
import time
import errno

def make_sure_path_exists(path):
    try:
        os.makedirs(path)
    except OSError as exception:
        if exception.errno != errno.EEXIST:
            raise

make_sure_path_exists('mnt/fuse-cpp-ramfs');

child = subprocess.Popen(['src/fuse-cpp-ramfs',
                          'mnt/fuse-cpp-ramfs'])

# If you unmount too soon, the mountpoint won't be available.
time.sleep(1)

if sys.platform == 'darwin':
  subprocess.run(['umount',
                  'mnt/fuse-cpp-ramfs'])
else:
  subprocess.run(['fusermount', '-u',
                  'mnt/fuse-cpp-ramfs'])

child.wait()
sys.exit(child.returncode)
