//***************************************************************************
//
// GThreads - preemptive threads in userspace. 
// Inspired by https://c9x.me/articles/gthreads/code0.html.
//
// Program created for subject OSMZ and OSY. 
//
// Michal Krumnikl, Dept. of Computer Sciente, michal.krumnikl@vsb.cz 2019
// Petr Olivka, Dept. of Computer Science, petr.olivka@vsb.cz, 2021
//
// Program simulates preemptice switching of user space threads. 
//
//***************************************************************************

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "gthr.h"

#define COUNT 100

// Dummy function to simulate some thread work
void f( void ) {
    static int x;
    int count = COUNT;
    int id = ++x;
    gt_task_suspend(1);

    while ( count-- )
    {
        printf( "F Thread id: %d, count: %d, index: %d, name: %s, arg: %d\n", id, count, gettid(), gt_getname(), (int) gt_getarg() );
        uninterruptibleNanoSleep( 0, 1000000 );
#if ( GT_PREEMPTIVE == 0 )
        gt_yield();
#endif
    }
}

// Dummy function to simulate some thread work
void g( void ) {
    static int x = 0;
    int count = COUNT;
    int id = ++x;

    while ( count-- )
    {
        printf( "G Thread id: %d, count: %d, index: %d, name: %s, arg: %d\n", id, count, gettid(), gt_getname(), (int) gt_getarg() );
        uninterruptibleNanoSleep( 0, 1000000 );
#if ( GT_PREEMPTIVE == 0 )
        gt_yield();
#endif
    }
}

int main(void) {
    gt_init();      // initialize threads, see gthr.c
    gt_go( f, "First thread" , (void*)1);     // set f() as first thread
    gt_go( f, "Second thread" , (void*)3);     // set f() as second thread
    gt_go( g, "Third thread" , (void*)6);     // set g() as third thread
    gt_go( g, "Fourth thread" , (void*)9);     // set g() as fourth thread
    printf("%s\n", gt_task_list()+6);
    gt_scheduler(); // wait until all threads terminate
    printf( "Threads finished\n" );
}