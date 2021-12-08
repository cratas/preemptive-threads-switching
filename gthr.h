#ifndef __GTHR_H
#define __GTHR_H

#include <stdint.h>
#include <time.h>

#ifndef GT_PREEMPTIVE 

#define GT_PREEMPTIVE 1

#endif

static const char *STATES[] = {
    "Unused", "Running", "Ready", "Blocked", "Suspended",
};

enum {
    MaxGThreads = 5,                // Maximum number of threads, used as array size for gttbl
    StackSize = 0x400000,           // Size of stack of each thread
    TimePeriod = 10,                // Time period of Timer
};


// Thread state
typedef enum {
    Unused,
    Running,
    Ready,
    Blocked,
    Suspended,
} gt_thread_state_t;


struct gt_context_t {
    struct gt_regs {                // Saved context, switched by gt_*swtch.S (see for detail)
        uint64_t rsp;
        uint64_t rbp;
#if ( GT_PREEMPTIVE == 0 )
        uint64_t r15;
        uint64_t r14;
        uint64_t r13;
        uint64_t r12;
        uint64_t rbx;
#endif
    }
    regs;

    gt_thread_state_t thread_state;         // process state
    int tid;                                // position of thread in thread table
    char name[20];                          // name of thread
    void* argument;                         // thread argument
    clock_t start_time;                     // time of starting thread   
    clock_t time;                           // running time
    int ticks_count;                        // count of ticks
    int ticks_delay;                        // count of delay 
};


void gt_init( void );                       // initialize gttbl
int  gt_go( void ( * t_run )( void ), char* name, void* argument );     // create new thread and set f as new "run" function
void gt_stop( void );                       // terminate current thread
int  gt_yield( void );                      // yield and switch to another thread
void gt_scheduler( void );                  // start scheduler, wait for all tasks

void gt_ret( int t_ret );                   // terminate thread
int gettid();                               // function returning position of current thread
char* gt_getname();                         // function returning name of current thread
void* gt_getarg();                          // function returning argument of current thread 
char* gt_task_list();                       // function returning list of threads as text   
void gt_task_suspend( int tid );                     // function to suspend thread by id
void gt_task_resume( int tid );                      // function to resume thread by id
void gt_task_delay( int ticks_count, int tid );               // function to sleep thread to n ticks sended as function parameter


#if ( GT_PREEMPTIVE == 0 )
void gt_swtch( struct gt_regs * t_old, struct gt_regs * t_new );        // declaration from gtswtch.S
#else
void gt_pree_swtch( struct gt_regs * t_old, struct gt_regs * t_new );   // declaration from gtswtch.S
#endif

int uninterruptibleNanoSleep( time_t sec, long nanosec );   // uninterruptible sleep

#endif // __GTHR_H