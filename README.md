# RefFS: a small, fast file system for Metis as a reference with efficient ioctl API

## Introducation

RefFS, or formerly known as VeriFS2, is a FUSE-based in-memory file system used as 
a reference file system for the [Metis](https://github.com/sbu-fsl/nfs-validator) model 
checking framework.  RefFS is based on Peter Watkins's [fuse-cpp-ramfs](https://github.com/watkipet/fuse-cpp-ramfs),
but we made major changes to the entire file system structure and provided 
four novel APIs: checkpoint/restore and pickle/load to support more 
efficient model checking than other file systems.  Please refer to our 
FAST '24 paper **"Metis: File System Model Checking via Versatile Input and State Exploration"**
for details.

## Requirements

We used and tested RefFS on Ubuntu 20.04 and Ubuntu 22.04 with Linux kernel version 
5.4 or greater.  RefFS builds 
with CMake version 3.0 or greater and requires the libfuse2-2.9 (or later) filesystem-in-userspace 
library and header files for successful compilation. libfuse is available at: 
https://github.com/libfuse/libfuse (for Linux) and
https://osxfuse.github.io (for Mac)

## Quick run

To build and mount RefFS, you can run the script `setup_verifs2.sh`.

```bash 
sudo ./setup_verifs2.sh
```

If you run the `mount` command, you should be able to see the following 
output to indicate that RefFS is mounted successfully:

```
verifs2 on /mnt/test-verifs2 type fuse.verifs2 (rw,nosuid,nodev,relatime,user_id=0,group_id=0,default_permissions,allow_other)
```

Alternatively, you can build and mount RefFS manually by the following commands:

```bash
mkdir build
cd build
cmake ../src
make
sudo make install
sudo mkdir -p /mnt/test-verifs2
sudo mount -t fuse.fuse-cpp-ramfs verifs2 /mnt/test-verifs2
```

## Contact 
For any question, please feel free to contact Yifei Liu ([yifeliu@cs.stonybrook.edu](mailto:yifeliu@cs.stonybrook.edu))
and Erez Zadok ([ezk@cs.stonybrook.edu](mailto:ezk@cs.stonybrook.edu)).
