.. image:: https://travis-ci.org/watkipet/fuse-cpp-ramfs.svg?branch=master
    :target: https://travis-ci.org/watkipet/fuse-cpp-ramfs

======================================================================
fuse-cpp-ramfs: An example RAM filesystem using FUSE written in C++
======================================================================

.. contents::

Quick Run
=========
::

	cd build
	cmake ../src
	make
	sudo make install
    sudo mkdir /mnt/test-verifs2
    sudo mount -t fuse.fuse-cpp-ramfs verifs2 /mnt/test-verifs2
    
Requirements
============
fuse-cpp-ramfs builds with CMake version 3.0 or greater.

fuse-cpp-ramfs requires the libfuse2-2.9 (or later) 
filesystem-in-userspace library and header files for successful 
compilation.  libfuse is available
at: 
https://github.com/libfuse/libfuse
https://osxfuse.github.io

--
Peter Watkins
19-May-2017

