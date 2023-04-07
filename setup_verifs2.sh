#!/bin/bash

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

