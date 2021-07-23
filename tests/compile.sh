#!/bin/bash

rm -rf ../build/*
cd ../build/ || exit
cmake ../src
make
sudo make install