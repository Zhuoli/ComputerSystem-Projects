#!/bin/sh

TMP=/tmp/$USER.q2.$$
trap "rm -f $TMP" 0

EXE=./homework
RESULT=PASSED

echo Question 2

echo '- Basic execution'

$EXE q2 > $TMP <<EOF || RESULT=FAILED
q2prog my_pattern
this is a test of a line
that didn't contain the pattern; nor did the second
but the third one had 'my_pattern'
right at the end, not the fourth

quit
EOF

if grep -q "[-]- but the third one had 'my_pattern'" $TMP ; then
    if grep -q 'this is a test' $TMP || grep -q 'that didn.t contain the' $TMP || grep -q 'right at the' $TMP; then
	RESULT=FAILED
	echo ' *** ERROR: output mismatch'
	echo "should be <prompt>-- but the third one had 'my_pattern'<prompt>"
	echo "output was:"
	cat $TMP | sed -e '$a\'
    else
	echo ' - Output OK'
    fi
fi

echo '- Blank lines'
$EXE q2 > $TMP <<EOF || RESULT=FAILED

q2prog foo
line with
and with foo


quit
EOF
if grep -q "[-]- and with foo" $TMP ; then
    echo ' - Output OK'
else
    RESULT=FAILED
    echo ' *** ERROR: bad output'
    cat $TMP | sed -e '$a\'
fi

echo 'New command names'
EXE2=/tmp/cmd.$$
cp q2prog $EXE2
trap "rm -f $EXE2" 0

$EXE q2 > $TMP <<EOF || RESULT=FAILED
$EXE2 foo
line with
and with foo

quit
EOF
if grep -q "[-]- and with foo" $TMP ; then
    echo ' - Output OK'
else
    RESULT=FAILED
    echo ' *** ERROR: bad output'
    cat $TMP | sed -e '$a\'
fi

TMP2=$TMP.x
trap "rm -f $TMP2" 0

echo 'Missing command file'
sh -c "$EXE q2" > $TMP 2> $TMP2 <<EOF || RESULT=FAILED
zzz a
quit
EOF
if [ $? -ne 0 ] ; then
    RESULT=FAILED
    echo ' *** ERROR: failed with output:'
    cat $TMP | sed -e '$a\'
    echo ' *** and error:'
    cat $TMP2
else
    echo ' - handled OK with output:'
    cat $TMP | sed -e '$a\'
fi

valgrind $EXE q2 > $TMP 2<&1 <<EOF
q2prog foo
line with
and with foo

quit
EOF
if grep -q 'ERROR SUMMARY: 0' $TMP ; then
    echo ' - Valgrind memory check OK'
else
    RESULT=FAILED
    echo ' *** ERROR: Valgrind detects memory errors'
    cat $TMP
fi

echo Result: $RESULT
