#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include <hash.h>
#include "threads/fixed-point.h"
#include "threads/synch.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */
#define NICE_MIN -20                    /* Lowest nice. */
#define NICE_DEFAULT 0                  /* Default nice. */
#define NICE_MAX 20                     /* Highest nice. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    int nice;                           /* Nice. How nice to other threads. */
		fixed recent_cpu;                   /* Recent CPU. how much CPU time each process
																					 has received recently. */
    struct list_elem allelem;           /* List element for all threads list. */
    struct list_elem sleepelem;         /* List element for sleeping threads list. */
    struct list_elem prielem;           /* List element for all threads list. */
		struct list_elem rccelem;           /* List element for recent_cpu changed list. */
		bool rcc;                           /* If recent_cpu changed, it's true. It also means
																				   whether rccelem is in the rcc_list or not. */

    /* Owned by timer.c. */
    int64_t awake_tick;                 /* The time when sleeping thread to awake. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

    /* Owned by synch.c. */
    struct thread *donated_for;         /* If this thread donated it's priority to thread A, 
																					 then A is stored in this variable. */
    struct lock *donated_to_get;        /* The acquired lock when donation has occured. */
    struct list hold_list;              /* List of held locks. */
    int original_priority;              /* Original priority. */
		

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
		int exit_status;                    /* Saved return value(main()) of this process. */
		struct semaphore exit_wait_sema;    /* For parent to wait this thread. */
		int lastfd;                         /* Last value of allocated file descriptor. */
		struct list open_list;              /* List of opend files. */
		struct semaphore loaded;            /* For parent to wait until finish of loading. */
		bool load_failed;                   /* Tells to parent whether loading is failed. */
		struct file *my_binary;             /* The binary excutable file of this process. */
		bool is_process;                    /* Whether if it is a user process. */
		bool in_syscall;                    /* Whether if this process called a system call.  */
#endif
#ifdef VM
		struct hash spt;                    /* SPT(Supplemental Page Table).
                                           Implemented by hash table. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void update_load_avg(void);
void update_recent_cpu(void);

struct thread *get_thread_by_tid (tid_t tid);

#endif /* threads/thread.h */
