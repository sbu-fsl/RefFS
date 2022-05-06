#!/bin/bash

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
echo 'FUSE_CPP_RAMFS_PROG=$FUSE_RAMFS_BINDIR/src/fuse-cpp-ramfs' >> $DEST
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
