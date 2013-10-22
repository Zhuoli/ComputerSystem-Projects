/* 
 * file:        homework.c
 * description: Skeleton code for CS 5600 Homework 2
 *
 * Peter Desnoyers, Northeastern CCIS, 2012
 * $Id: homework.c 530 2012-01-31 19:55:02Z pjd $
 */

#include <stdio.h>
#include <stdlib.h>
#include "hw2.h"

/********** YOUR CODE STARTS HERE ******************/

/*
 * Here's how you can initialize global mutex and cond variables
 */
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t C = PTHREAD_COND_INITIALIZER;

/* get_forks() method - called before a philospher starts eating.
 *                      'i' identifies the philosopher 0..N-1
 */
void get_forks(int i)
{
    pthread_mutex_lock(&m);
    /* your code here */
    pthread_mutex_unlock(&m);
}

/* release_forks()  - called when a philospher is done eating.
 *                    'i' identifies the philosopher 0..N-1
 */
void release_forks(int i)
{
    pthread_mutex_lock(&m);
    /* your code here */
    pthread_mutex_unlock(&m);
}

/* Threads which call these methods. Note that the pthread create
 * function allows you to pass a single void* pointer value to each
 * thread you create; we actually pass an integer (philosopher number)
 * as that argument instead, using a "cast" to pretend it's a pointer.
 */

/* the customer thread function - create 10 threads, each of which calls
 * this function with its customer number 0..9
 */
void *philosopher_thread(void *context) 
{
    int philosopher_num = (int)context; 

    /* your code goes here */
    
    return 0;
}

void q2(void)
{
    /* to create a thread:
        pthread_t t; 
        pthread_create(&t, NULL, function, argument);
       note that the value of 't' won't be used in this homework
    */

    /* your code goes here */
    
}

/* For question 3 you need to measure the following statistics:
 *
 * 1. The fraction of time there are 3, 2, 1, and 0 philosophers thinking.
 *    (hint - use 4 counters, one for each state, where one of them is
 *    set to 1 and all the others to 0 at any time. The average value
 *    of each counter will be the fraction of time the system is in
 *    the corresponding state)
 * 2. Average time taken by a call to get_forks() (use a timer)
 * 3. Average number of philosophers waiting in the get_forks()
 *    method. (use a counter)
 *
 * The stat_* functions (counter, timer) are described in the PDF. 
 */

void q3(void)
{
    /* your code goes here */
}
