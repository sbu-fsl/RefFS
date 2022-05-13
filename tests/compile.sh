#!/bin/bash

xargs rm < ../build/install_manifest.txt
#rm -rf ../build/*
cd ../build/ || exit
cmake ../src
make
sudo make install