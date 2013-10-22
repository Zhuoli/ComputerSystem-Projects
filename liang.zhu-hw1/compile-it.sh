#
# file:        compile.sh
# description: compile main and ancillary files for Homework 1
#
UPROGS='q1prog q2prog q3prog1 q3prog2'

if [ x"$1" = xclean ] ; then
    rm -f *.o homework *.elf $UPROGS
    exit
fi

# compiling the main program is straightforward
#
gcc -Wall -O0 -g -c homework.c
gcc -Wall -O0 -g -c misc.c
gcc -Wall -O0 -g misc.o homework.o -o homework

# Compiling the micro-programs is quite tricky. This set of commands
# will compile them with position-independent code, as well as mashing
# the various sections (.text, .rodata, .data, .bss) into a single 4K
# section.
#
# In addition we disable re-ordering functions, so that we're
# guaranteed that the first function in the file starts at offset 0.
#
as -o vector.o vector.s

CC_OPTS='-fno-builtin -fno-stack-protector -fpie -fno-zero-initialized-in-bss -Wa,-R
             -mpreferred-stack-boundary=2 -march=i386 -g -Wa,--no-warn'
for f in $UPROGS; do
    gcc  ${CC_OPTS} -c $f.c -o $f.o
    ld -pie --entry=main --section-start .text=0x1000 -o $f.elf $f.o vector.o
    objcopy --output-target=binary -j .text -j .rodata $f.elf $f
    size=$(wc -c < $f)
    if [ $size -gt 3000 ] ; then
	echo "$f too large: $size bytes, max 3000"
	echo "   global variables too large?"
    fi
done

