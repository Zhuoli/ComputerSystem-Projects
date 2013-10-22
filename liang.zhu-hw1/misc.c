/*
 * file:        misc.c
 * description: Support functions for homework 1
 *
 * CS 3600, Systems & Networks, Northeastern CCIS
 * Peter Desnoyers, Sept. 2010
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/select.h>

void *proc1;
void *proc1_stack;
void *proc2;
void *proc2_stack;
void **vector;

/*
 * do_switch - save stack pointer to *location_for_old_sp, set
 *             stack pointer to 'new_value', and return.
 *             Note that the return takes place on the new stack.
 *
 * For more details, see:
 *  http://pdos.csail.mit.edu/6.828/2004/lec/l2.html - GCC calling conventions
 *  http://www.ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html
*/
typedef int *stack_ptr_t;
void do_switch(stack_ptr_t *location_for_old_sp, stack_ptr_t new_value)
{
    /* C calling conventions require that we preserve %ebp, %ebx,
     * %esi, and %edi. GCC saves %ebp already, so save the remaining
     * 3 registers on the stack.
     */
    asm("push %%ebx;" 
	"push %%esi;"
	"push %%edi" : :);

    if (location_for_old_sp != NULL)
	asm("mov %%esp,%0" : : "m"(*location_for_old_sp));
    
    asm("mov %0,%%esp" : "=m"(new_value) :); /* switch! */

    asm("pop %%edi;"		/* Restore "callee-save" registers */
	"pop %%esi;"		
	"pop %%ebx" : :);
}


/*
 * setup_stack(stack, function) - sets up a stack so that switching to
 * it from 'do_switch' will call 'function'. Returns the resulting
 * stack pointer.
 */
stack_ptr_t setup_stack(int *stack, void *func)
{
    int old_bp = (int)stack;	/* top frame - SP = BP */
    
    *(--stack) = (int)func;	//PUSH/* return address */
    *(--stack) = old_bp;	/* %ebp */
    *(--stack) = 0;		/* %ebx */
    *(--stack) = 0;		/* %esi */
    *(--stack) = 0;		/* %edi */

    return stack;
}


/*
 * init_memory - initialize the following variables:
 *   proc1, proc1_stack - bottom and top of process 1 address space
 *   proc2, proc2_stack - ditto for process 2
 *   vector - OS interface vector
 */
void init_memory(void)
{
    void *base = (void*)0x09000000;
    size_t len = 3 * 4096;	/* 1 page each for proc1,proc2,vector */
    char *buf = mmap(base, len, PROT_READ | PROT_WRITE |
		     PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (buf == (char*)-1) {
	fprintf(stderr, "Error mapping memory: %s\n", strerror(errno));
	exit(1);
    }
    
    proc1 = buf;
    proc1_stack = buf + 4096 - 4;
    
    proc2 = buf+4096;
    proc2_stack = buf + 8192 - 4;

    /*
     * note that the value of 'vector' - 0x09002000 - is hard-coded in
     * vector.s 
     */
    vector = (void*)(buf + 8192);
}
    
extern void q1(void);
extern void q2(void);
extern void q3(void);

void usage(char *prog)
{
    printf("usage:\t%s q1\n"
           "or:   \t%s q2, or q3\n", prog, prog);
    exit(1);
}

int main(int argc, char **argv)
{
    int tmp1 = 0x12345678, tmp2 = 0xa5b4c3d2;
    
    init_memory();

    if (argc != 2) 
        usage(argv[0]);
    
    if (!strcmp(argv[1], "q1"))
        q1();
    else if (!strcmp(argv[1], "q2"))
        q2();
    else if (!strcmp(argv[1], "q3"))
        q3();
    else
        usage(argv[0]);

    if (tmp1 != 0x12345678 || tmp2 != 0xa5b4c3d2)
        printf("*** ERROR: stack corruption (0x%x, 0x%x)\n", tmp1, tmp2);

    return 0;
}

