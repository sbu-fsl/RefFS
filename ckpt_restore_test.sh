#!/bin/bash

BUILDPATH=/home/yifei/zram_test/VeriFS2-ioctl-20210212/fuse-cpp-ramfs/build

rm -rf $BUILDPATH/*
cd $BUILDPATH
cmake ../src
make
sudo make install
sudo umount -l /mnt/test-verifs2
sudo rm -rf /mnt/test-verifs2/*
sudo umount -l /mnt/test-verifs1
sudo rm -rf /mnt/test-verifs1/*
sudo rm -r /mnt/test-verifs2
sudo rm -r /mnt/test-verifs1
sudo mkdir /mnt/test-verifs2
sudo mkdir /mnt/test-verifs1
sudo chown yifei:yifei /mnt/test-verifs2
sudo chown yifei:yifei /mnt/test-verifs1

