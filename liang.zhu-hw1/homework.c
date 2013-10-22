/*
 * file:        homework.c
 * description: Skeleton for homework 1
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, Sep. 2012
 * $Id: homework.c 500 2012-01-15 16:15:23Z pjd $
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include "uprog.h"

/***********************************/
/* Declarations for code in misc.c */
/***********************************/

typedef int *stack_ptr_t;
extern void init_memory(void);
extern void do_switch(stack_ptr_t *location_for_old_sp, stack_ptr_t new_value);
extern stack_ptr_t setup_stack(int *stack, void *func);
extern int get_term_input(char *buf, size_t len);
extern void init_terms(void);

extern void  *proc1;
extern void  *proc1_stack;
extern void  *proc2;
extern void  *proc2_stack;
extern void **vector;


/***********************************************/
/********* Your code starts here ***************/
/***********************************************/

char argv[10][128];
stack_ptr_t ptr_proc1_stack=NULL;
stack_ptr_t ptr_proc2_stack=NULL;
stack_ptr_t main_stack=NULL;
/*
 * Question 1.
 *
 * The micro-program q1prog.c has already been written, and uses the
 * 'print' micro-system-call (index 0 in the vector table) to print
 * out "Hello world".
 *
 * You'll need to write the (very simple) print() function below, and
 * then put a pointer to it in vector[0].
 *
 * Then you read the micro-program 'q1prog' into memory starting at
 * address 'proc1', and execute it, printing "Hello world".
 *
 */
void print(char *line)
{
  printf("%s",line);
    /*
     * Your code goes here. 
     */
}

void q1(void)
{
 char *temp[]={NULL,"q1prog",NULL};
 FILE *fp=NULL;
 void (*ptr)(void);
 vector[0]=print;
 if((fp=fopen("q1prog","r"))==NULL)
  {
   perror("Open:");
   exit(-1);}
 
 if(!fgets(proc1,4096,fp))
    perror("Fread:"); 
  ptr=proc1;
  execv("q1prog",temp) ;
  perror("execv:");
}


/*1
 * Question 2.
 *
 * Add two more functions to the vector table:
 *   void readline(char *buf, int len) - read a line of input into 'buf'
 *   char *getarg(int i) - gets the i'th argument (see below)

 * Write a simple command line which prints a prompt and reads command
 * lines of the form 'cmd arg1 arg2 ...'. For each command line:
 *   - save arg1, arg2, ... in a location where they can be retrieved
 *     by 'getarg'
 *   - load and run the micro-program named by 'cmd'
 *   - if the command is "quit", then exit rather than running anything
 *
 * Note that this should be a general command line, allowing you to
 * execute arbitrary commands that you may not have written yet. You
 * are provided with a command that will work with this - 'q2prog',
 * which is a simple version of the 'grep' command.
 *
 * NOTE - your vector assignments have to mirror the ones in vector.s:
 *   0 = print
 *   1 = readline
 *   2 = getarg
 */
void readline(char *buf, int len) /* vector index = 1 */
{
  FILE *fp=stdin;
 if( (fgets(buf,len,fp)==NULL))
   {  perror("fgets:");
      exit(-1);
   };
 buf[strlen(buf)-1]='\0';
}

char *getarg(int i)		/* vector index = 2 */
{
  if(argv[i+1][0]=='\0')
    return 0; 
  return argv[i+1];
}
char *strwrd(char *s,char *buf,size_t len,char* delim)
{
	s +=strspn(s,delim);           
	int n=strcspn(s,delim);
	if(len-1<n)
	    n=len-1;
        memset(buf,0,len);
	memcpy(buf,s,n);
	s +=n;
	return (*s==0)? NULL : s;
}
/*
 * Note - see c-programming.pdf for sample code to split a line into
 * separate tokens. 
 */
void q2(void)
{ 
    
    FILE *fp=NULL;
    int (*ptr)(void);
    int argc;
    char* buf=(char*) malloc(280*sizeof(char));
    char* ptr_buf=NULL;
    vector[0]=print; 
    vector[1]=readline;
    vector[2]=getarg;
    while (1) {
        ptr_buf=buf;
        readline(ptr_buf,280);	/* get a line */
        for(argc=0;argc<10;argc++){
           ptr_buf=strwrd(ptr_buf,argv[argc],sizeof(argv[argc])," \t");
           if(ptr_buf==NULL)
	   break;
           };
        if(!strlen(argv[0])) /* if zero words, continue */
	 { continue;}
        else if(strcmp("quit",argv[0])==0) 
         {
           exit(0);    /* if first word is "quit", break */
         }
        else
        {
         fp=fopen(argv[0],"r");
	if(fp==NULL){
	  perror("Can't open file");
          exit(-1);
 	  } 
	fread(proc2,4096,1,fp);
 	   ptr=proc2;
           (*ptr)();
	 }	
      /* make sure 'getarg' can find the remaining words */
	/* load and run the command */
    }
    ptr_buf=NULL;
    free(buf);
    exit(0);
     /*
     * Note that you should allow the user to load an arbitrary command,
     * not just 'ugrep' and 'upcase', and print an error if you can't
     * find and load the command binary.
     */
}

/*
 * Question 3.
 *
 * Create two processes which switch back and forth.
 *
 * You will need to add another 3 functions to the table:
 *   void yield12(void) - save process 1, switch to process 2
 *   void yield21(void) - save process 2, switch to process 1
 *   void uexit(void) - return to original homework.c stack
 *
 * The code for this question will load 2 micro-programs, q3prog1 and
 * q3prog2, which are provided and merely consists of interleaved
 * calls to yield12() or yield21() and print(), finishing with uexit().
 *
 * Hints:
 * - Use setup_stack() to set up the stack for each process. It returns
 *   a stack pointer value which you can switch to.
 * - you need a global variable for each process to store its context
 *   (i.e. stack pointer)
 * - To start you use do_switch() to switch to the stack pointer for 
 *   process 1
 */

void yield12(void)		/* vector index = 3 */
{
  do_switch(&ptr_proc1_stack,ptr_proc2_stack);
}

void yield21(void)		/* vector index = 4 */
{
    /* Your code here */
  do_switch(&ptr_proc2_stack,ptr_proc1_stack);
}

void uexit(void)		/* vector index = 5 */
{
 do_switch(&ptr_proc1_stack,main_stack);
}

void q3(void)
{
    ptr_proc1_stack=proc1_stack;
    ptr_proc2_stack=proc2_stack;    
    vector[0]=print; 
    vector[1]=readline;
    vector[2]=getarg;
    vector[3]=yield12;
    vector[4]=yield21;
    vector[5]=uexit;
    FILE* fp=NULL;
    fp=fopen("q3prog1","r");
    if(fp==NULL){
	perror("fopen q3prog1:");
        exit(-1);
	};
    fread(proc1,4091,1,fp);
    fclose(fp);
    fp=fopen("q3prog2","r");
    if(fp==NULL){
	perror("fopen q3prog2:");
	exit(-1);
	};
    fread(proc2,4096,1,fp);
    fclose(fp);
   ptr_proc1_stack=setup_stack(ptr_proc1_stack,proc1); 
   ptr_proc2_stack=setup_stack(ptr_proc2_stack,proc2);
   do_switch(&main_stack,ptr_proc1_stack);
    
    /* load q3prog1 into process 1 and q3prog2 into process 2 */
    /* then switch to process 1 */
}


/***********************************************/
/*********** Your code ends here ***************/
/***********************************************/
