#!/bin/bash
#
# file:        q3test.sh
# description: test script for FUSE version of on-disk FS
#
# CS 7600, Intensive Computer Systems, Northeastern CCIS
# Peter Desnoyers, April 2010
# $Id: q4test.sh 163 2010-04-20 19:51:46Z pjd $
#

MNT=/tmp/mnt-$$
DISK=/tmp/disk.$$.img
rm -f $DISK
./mkfs-cs5600fs --create 1m $DISK
rm -rf $MNT
mkdir -p $MNT

cat <<EOF
Now start debugging with the following command (note - the '-d' is crucial):
  gdb --args ./homework -d $DISK $MNT

and then run the test in another window with the command:
  sh q3debug.sh $MNT

Finally when you're done, clean up with these commands:
  fusermount -u $MNT
  rm $DISK
  rmdir $MNT
EOF
