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


DEST_DIR=$(pwd)
DEST_NAME=mount.fuse.fuse-cpp-ramfs
DEST=$DEST_DIR/$DEST_NAME
FUSE_RAMFS_BINDIR=$(pwd)

echo '#!/bin/bash' > $DEST
echo '' >> $DEST
echo 'if [ "$#" -lt 2 ]; then' >> $DEST
echo '    echo "Usage: $0 <mount-name> <mount-dir> [-o options]";' >> $DEST
echo '    exit 1;' >> $DEST
echo 'fi' >> $DEST
echo '' >> $DEST
echo "FUSE_RAMFS_BINDIR=$FUSE_RAMFS_BINDIR" >> $DEST
echo 'MNT_NAME=$1' >> $DEST
echo 'MNT_DIR=$2' >> $DEST
echo 'OPTS=' >> $DEST
echo 'FUSE_CPP_RAMFS_PROG=$FUSE_RAMFS_BINDIR/fuse-cpp-ramfs' >> $DEST
echo 'FUSE_CPP_RAMFS_MOUNT=$FUSE_RAMFS_BINDIR/mnts' >> $DEST
echo '' >> $DEST
echo '# If there is options string?' >> $DEST
echo 'if [ "$#" -gt 2 ] && [ "$3" == "-o" ]; then' >> $DEST
echo '    OPTS=$4;' >> $DEST
echo 'fi' >> $DEST
echo '' >> $DEST
echo 'mkdir -p $FUSE_CPP_RAMFS_MOUNT' >> $DEST
echo 'ln -fs $FUSE_CPP_RAMFS_PROG $FUSE_CPP_RAMFS_MOUNT/$MNT_NAME' >> $DEST
echo '' >> $DEST
echo 'if [ -n "$OPTS" ]; then' >> $DEST
echo '    nohup $FUSE_CPP_RAMFS_MOUNT/$MNT_NAME -o $OPTS $MNT_DIR 2>&1 > $FUSE_CPP_RAMFS_MOUNT/$MNT_NAME.out &' >> $DEST
echo 'else' >> $DEST
echo '    nohup $FUSE_CPP_RAMFS_MOUNT/$MNT_NAME $MNT_DIR 2>&1 > $FUSE_CPP_RAMFS_MOUNT/$MNT_NAME.out &' >> $DEST
echo 'fi' >> $DEST
echo '' >> $DEST
echo 'if [ -z "$(pgrep $MNT_NAME)" ]; then' >> $DEST
echo '    echo "Mount failed.";' >> $DEST
echo '    exit 1;' >> $DEST
echo 'fi' >> $DEST
echo '' >> $DEST
echo 'exit 0;' >> $DEST

chmod +x $DEST
