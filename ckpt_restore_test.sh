#!/bin/bash

BUILDPATH=/home/yifei/zram_test/VeriFS2-ioctl-20210212/fuse-cpp-ramfs/build

rm -rf $BUILDPATH/*
cd $BUILDPATH
cmake ../src
make
sudo make install
sudo rm -rf /mnt/test-verifs2/*
sudo umount -l /mnt/test-verifs2
