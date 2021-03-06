#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#include "gthr.h"

struct gt_context_t g_gttbl[ MaxGThreads ]; // statically allocated table for thread control
struct gt_context_t * g_gtcur;              // pointer to current thread


#if ( GT_PREEMPTIVE != 0 )

void gt_sig_handle( int t_sig );

// initialize SIGALRM
void gt_sig_start( void )
{
    struct sigaction l_sig_act;
    bzero( &l_sig_act, sizeof( l_sig_act ) );
    l_sig_act.sa_handler = gt_sig_handle;

    sigaction( SIGALRM, &l_sig_act, NULL );
    ualarm( TimePeriod * 1000, TimePeriod * 1000 );
}

// unblock SIGALRM
void gt_sig_reset( void ) 
{
    sigset_t l_set;                     // Create signal set
    sigemptyset( & l_set );             // Clear it
    sigaddset( & l_set, SIGALRM );      // Set signal (we use SIGALRM)

    sigprocmask( SIG_UNBLOCK, & l_set, NULL );  // Fetch and change the signal mask
}

// function triggered periodically by timer (SIGALRM)
void gt_sig_handle( int t_sig ) 
{
    gt_sig_reset();                     // enable SIGALRM again

    g_gtcur -> time = g_gtcur -> time + ( clock() - g_gtcur -> start_time );
    g_gtcur -> ticks_count++;

    struct gt_context_t * thread;    

    for ( thread = & g_gttbl[ 0 ];; thread++)                             // find an empty slot
    {
        if ( thread == & g_gttbl[ MaxGThreads ] )                         // if we have reached the end, gttbl is full and we cannot create a new thread
            break;

        if( thread -> ticks_delay == 0 )
        {
            if(thread -> thread_state == Blocked) thread -> thread_state = Ready;
        }
        else
        {
            thread -> ticks_delay--;
        }
    }

    g_gtcur -> ticks_to_switch_current--;

    if(g_gtcur -> ticks_to_switch_current == 0) 
    {
        g_gtcur -> ticks_to_switch_current = g_gtcur -> ticks_to_switch;
        printf("SWITCHING TO ANOTHER THREAD\n");
        gt_yield();                         // calling sheduler
        g_gtcur -> start_time = clock();
    }
}

#endif

// initialize first thread as current context (and start timer)
void gt_init( void ) 
{
    g_gtcur = & g_gttbl[ 0 ];           // initialize current thread with thread #0
    g_gtcur -> thread_state = Running;  // set current to running
    g_gtcur -> tid = 0;                 //set current position in thread table to 0
    g_gtcur -> time = clock();
    g_gtcur -> ticks_count = 0;
    g_gtcur -> ticks_delay = 0;
    g_gtcur -> ticks_to_switch_current = 1;
    g_gtcur -> ticks_to_switch = 1;

//BLOCK OF CODE USED IN NON PREEMPTIVE THREADS
#if ( GT_PREEMPTIVE != 0 )
    gt_sig_start();
#endif
}


// exit thread
void gt_ret( int t_ret ) 
{
    if ( g_gtcur != & g_gttbl[ 0 ] )    // if not an initial thread,
    {
        g_gtcur -> thread_state = Unused;// set current thread as unused
        gt_yield();                     // yield and make possible to switch to another thread
        assert( !"reachable" );         // this code should never be reachable ... (if yes, returning function on stack was corrupted)
    }
}

void gt_scheduler( void )
{
    while ( gt_yield() )                // if initial thread, wait for other to terminate
    {
        if ( GT_PREEMPTIVE )
            usleep( TimePeriod * 1000 );// idle, simulate CPU HW sleep
    }
}


// switch from one thread to other
int gt_yield( void ) 
{
    struct gt_context_t * thread;
    struct gt_regs * l_old, * l_new;
    int l_no_ready = 0;                 // not ready processes

    thread = g_gtcur;
    while ( thread -> thread_state != Ready )// iterate through g_gttbl[] until we find new thread in state Ready 
    {
        if ( thread -> thread_state == Blocked || thread -> thread_state == Suspended ) 
            l_no_ready++;               // number of Blocked and Suspend processes

        if ( ++thread == & g_gttbl[ MaxGThreads ] )// at the end rotate to the beginning
            thread = & g_gttbl[ 0 ];
        if ( thread == g_gtcur )             // did not find any other Ready threads
        {
            return - l_no_ready;        // no task ready, or task table empty
        }
    }

    if ( g_gtcur -> thread_state == Running )// switch current to Ready and new thread found in previous loop to Running
        g_gtcur -> thread_state = Ready;
    thread -> thread_state = Running;
    l_old = & g_gtcur -> regs;          // prepare pointers to context of current (will become old) 
    l_new = & thread -> regs;                // and new to new thread found in previous loop
    g_gtcur = thread;                        // switch current indicator to new thread
#if ( GT_PREEMPTIVE != 0 )
    gt_pree_swtch( l_old, l_new );      // perform context switch (assembly in gtswtch.S)
#else
    gt_swtch( l_old, l_new );           // perform context switch (assembly in gtswtch.S)
#endif
    return 1;
}


// return function for terminating thread
void gt_stop( void ) 
{
    gt_ret( 0 );
}


// create new thread by providing pointer to function that will act like "run" method
int gt_go( void ( * t_run )( void ), char* t_name, void* t_argument , int ticks_to_switch ) 
{
    char * l_stack;
    struct gt_context_t * thread;                // creating an empty struct with thread info
    int l_tid;                              // creating local variable l_tid for getting current index of empty slot    

    for ( thread = & g_gttbl[ 0 ], l_tid = 0;; thread++, l_tid++ )                // find an empty slot
        if ( thread == & g_gttbl[ MaxGThreads ] )                            // if we have reached the end, gttbl is full and we cannot create a new thread
            return -1;
        else if ( thread -> thread_state == Unused )
        {
            thread -> tid = l_tid;                                               // getting current thread id
            thread -> argument = t_argument;                                     // getting argument passed as parameter
            thread -> start_time = clock();                                      // setting runtime on 0
            thread -> ticks_count = 0;                                           // setting ticks_count on 0
            thread -> ticks_delay = 0;                                           // setting ticks_delay on 0
            thread -> ticks_to_switch = ticks_to_switch;                         // setting count of ticks for runtime before switch
            thread -> ticks_to_switch_current = ticks_to_switch;                 // setting current count of ticks for max

            snprintf( thread -> name, sizeof( thread -> name ) + 1, "%s", t_name );   // getting name passed as parameter limited on thread name size
            break; 
        }
                                         // new slot was found

    l_stack = ( char * ) malloc( StackSize );   // allocate memory for stack of newly created thread
    if ( !l_stack )
        return -1;

    *( uint64_t * ) & l_stack[ StackSize - 8 ] = ( uint64_t ) gt_stop;  //  put into the stack returning function gt_stop in case function calls return
    *( uint64_t * ) & l_stack[ StackSize - 16 ] = ( uint64_t ) t_run;   //  put provided function as a main "run" function
    thread -> regs.rsp = ( uint64_t ) & l_stack[ StackSize - 16 ];           //  set stack pointer
    thread -> thread_state = Ready;                                          //  set state

    return l_tid;
}


int uninterruptibleNanoSleep( time_t t_sec, long t_nanosec ) 
{
#if 0
    struct timeval l_tv_cur, l_tv_limit;
    struct timeval l_tv_tmp = { t_sec, t_nanosec / 1000 };
    gettimeofday( &l_tv_cur, NULL );
    timeradd( &l_tv_cur, &l_tv_tmp, &l_tv_limit );
    while ( timercmp( &l_tv_cur, &l_tv_limit, <= ) )
    {
        timersub( &l_tv_limit, &l_tv_cur, &l_tv_tmp );
        struct timespec l_tout = { l_tv_tmp.tv_sec, l_tv_tmp.tv_usec * 1000 }, l_res;    
        if ( nanosleep( & l_tout, &l_res ) < 0 )
        {
            if ( errno != EINTR )
                return -1;
        }
        else
        {
            break;
        }
        gettimeofday( &l_tv_cur, NULL );
    }

#else
    struct timespec req;
    req.tv_sec = t_sec;
    req.tv_nsec = t_nanosec;
    
    do {
        if (0 != nanosleep( & req, & req)) {
        if (errno != EINTR)
            return -1;
        } else {
            break;
        }
    } while (req.tv_sec > 0 || req.tv_nsec > 0);
#endif
    return 0; /* Return success */
}


// function returning current thread position in table
int gettid()
{
    return g_gtcur -> tid;
}

// function returning current thread position in table
char* gt_getname()
{
    return g_gtcur -> name;
}

// function returning current thread argument
void* gt_getarg()
{
    return g_gtcur -> argument;
}

// function returning task list as string
char* gt_task_list()
{
    struct gt_context_t * thread;    
    char task_list[1024]; 
    char* result = (char*)malloc(sizeof(char)*1024);

    strcat(task_list, "\nID \t | \t Name \t\t | \t Time \t\t | \t Ticks\n");
    strcat(task_list, "______________________________________________________________________________\n");


    for ( thread = & g_gttbl[ 1 ];; thread++)                             // find an empty slot
    {
        if ( thread == & g_gttbl[ MaxGThreads ] )                         // if we have reached the end, gttbl is full and we cannot create a new thread
            break;

        char list_line[128];
        char* state = STATES[thread -> thread_state];                     // getting thread state from enum via STATES static array

        sprintf(list_line, "%d \t | \t %s \t | \t %f \t | \t %d\n", thread -> tid, thread -> name, (double)(thread -> time) / CLOCKS_PER_SEC, thread -> ticks_count);
        strcat(task_list, list_line); 
    }

    strcpy(result, task_list);

    return result;
}

// function to suspend thread by id
void gt_task_suspend( int tid )
{
    struct gt_context_t *thread;
    thread = &g_gttbl[ tid ];

    thread -> thread_state = Suspended;
    if(thread -> thread_state == Suspended) gt_yield();     // if we want to suspend thread immediatly we gotta call sheduler (only if thread is currently running)
}

// function to resume thread by id
void gt_task_resume( int tid )
{
    struct gt_context_t *thread;

    thread = &g_gttbl[ tid ];
    if(thread -> thread_state == Suspended) thread -> thread_state = Ready;
}

// function to sleep thread on n ticks
void gt_task_delay( int ticks_count, int tid )
{
    struct gt_context_t *thread;
    thread = &g_gttbl[ tid ];
    thread -> ticks_delay = ticks_count;
    thread -> thread_state = Blocked;
}
