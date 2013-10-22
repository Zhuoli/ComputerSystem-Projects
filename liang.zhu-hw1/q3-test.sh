#!/bin/sh

TMP=/tmp/$USER.q3.$$
trap "rm -f $TMP" 0
RESULT=PASSED

EXE=./homework
echo Question 3

echo '- Basic execution'
if ! $EXE q3 > $TMP ; then
    echo ' *** ERROR: command failure'
    RESULT=FAILED
else
    echo ' - Command status OK'
fi

set $(cksum $TMP)
if [ $1 -ne 142514976 ] ; then
    RESULT=FAILED
    echo ' *** ERROR: output mismatch'
    diff -u --label='your output' --label='expected output' $TMP - <<EOF
program 1
program 2
program 1
program 2
program 1
program 2
program 1
program 2
EOF
else
    echo ' - Output OK'
fi

echo Result: $RESULT
