#!/bin/bash

#
# This file is part of RefFS.
# 
# Copyright (c) 2020-2024 Yifei Liu
# Copyright (c) 2020-2024 Wei Su
# Copyright (c) 2020-2024 Erez Zadok
# Copyright (c) 2020-2024 Stony Brook University
# Copyright (c) 2020-2024 The Research Foundation of SUNY
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

fsname="verifs2"
BUILD_DIR="build"
MOUNT_POINT="/mnt/test-$fsname"

if [ -d ./$BUILD_DIR ]; then
    rm -r ./$BUILD_DIR
fi

mkdir -p ./$BUILD_DIR

cd ./$BUILD_DIR
cmake ../src
make
sudo make install

if [ "$(mount | grep $MOUNT_POINT)" ]; then
    sudo umount -f $MOUNT_POINT;
fi

if [ -d $MOUNT_POINT ]; then
    sudo rm -rf $MOUNT_POINT;
fi

sudo mkdir -p $MOUNT_POINT

# Ensure that MOUNT_POINT is an empty dir
sudo mount -t fuse.fuse-cpp-ramfs $fsname $MOUNT_POINT

