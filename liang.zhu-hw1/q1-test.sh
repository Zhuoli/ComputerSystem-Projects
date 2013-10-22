
#!/bin/sh

TMP=/tmp/$USER.q1.$$
trap "rm -f $TMP" 0

EXE=./homework
RESULT=PASSED
echo Question 1

$EXE q1 > $TMP || RESULT=FAILED
if [ "$(cat $TMP | cksum)" != '3083891038 12' ] ; then
    RESULT=FAILED
    echo ' *** ERROR: output mismatch'
    echo Hello World | diff -u --label='your code' --label='reference' $TMP -
else
    echo ' - Output OK'
fi

valgrind $EXE q1 > $TMP 2<&1 || RESULT=FAILED
if grep -q 'ERROR SUMMARY: 0' $TMP ; then
    echo ' - Valgrind memory check OK'
else
    RESULT=FAILED
    echo ' *** ERROR: Valgrind detects memory errors'
    cat $TMP
fi

echo Result: $RESULT
