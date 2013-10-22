#!/bin/sh
#
# $Id: compile-q3.sh 522 2012-01-31 04:06:32Z pjd $
#

if [ x"$1" = xclean ] ; then
    rm -f q2 q3 libpth.so
    exit
fi

[ -r /usr/lib/libpth.so ] || ln -s -f /usr/lib/libpth.so.?? libpth.so

gcc -I. -g -Wall -o homework-q3 misc.c homework.c -DQ3 -L. -lpth -lm
