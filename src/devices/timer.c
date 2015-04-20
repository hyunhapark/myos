#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "devices/pit.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
  
/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);
static void real_time_delay (int64_t num, int32_t denom);

/* List of sleeping processes, that is, processes that should be 
	 checked each tick whether it should be awake or not. */
extern struct list sleep_list;            /* extern from threads/threads.c */
extern struct list pri_list[PRI_MAX+1];   /* extern from threads/threads.c */
extern struct list rcc_list;              /* extern from threads/threads.c */
extern int thread_priority;               /* extern from threads/threads.c */   

/* Sets up the timer to interrupt TIMER_FREQ times per second,
   and registers the corresponding interrupt. */
void
timer_init (void) 
{
	pit_configure_channel (0, 2, REAL_TIMER_FREQ); /* Use REAL_TIMER_FREQ for timer emulation. */
  intr_register_ext (0x20, timer_interrupt, "8254 Timer");
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) 
{
  unsigned high_bit, test_bit;

  ASSERT (intr_get_level () == INTR_ON);
  printf ("Calibrating timer...  ");

  /* Approximate loops_per_tick as the largest power-of-two
     still less than one timer tick. */
  loops_per_tick = 1u << 10;
  while (!too_many_loops (loops_per_tick << 1)) 
    {
      loops_per_tick <<= 1;
      ASSERT (loops_per_tick != 0);
    }

  /* Refine the next 8 bits of loops_per_tick. */
  high_bit = loops_per_tick;
  for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
    if (!too_many_loops (high_bit | test_bit))
      loops_per_tick |= test_bit;

  printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
int64_t
timer_ticks (void) 
{
  enum intr_level old_level = intr_disable ();
  int64_t t = ticks;
  intr_set_level (old_level);
  return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t
timer_elapsed (int64_t then) 
{
  return timer_ticks () - then;
}

/* Sleeps for approximately TICKS timer ticks.  Interrupts must
   be turned on. */
void
timer_sleep (int64_t ticks) 
{
  int64_t start = timer_ticks ();
	struct thread *t;

  ASSERT (intr_get_level () == INTR_ON);

	intr_disable ();
	t = thread_current ();
	list_push_back (&sleep_list, &t->sleepelem);
	t->awake_tick = start + ticks;
	thread_block ();
	intr_enable ();
}

/* Sleeps for approximately MS milliseconds.  Interrupts must be
   turned on. */
void
timer_msleep (int64_t ms) 
{
  real_time_sleep (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts must be
   turned on. */
void
timer_usleep (int64_t us) 
{
  real_time_sleep (us, 1000 * 1000);
}

/* Sleeps for approximately NS nanoseconds.  Interrupts must be
   turned on. */
void
timer_nsleep (int64_t ns) 
{
  real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Busy-waits for approximately MS milliseconds.  Interrupts need
   not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_msleep()
   instead if interrupts are enabled. */
void
timer_mdelay (int64_t ms) 
{
  real_time_delay (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts need not
   be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_usleep()
   instead if interrupts are enabled. */
void
timer_udelay (int64_t us) 
{
  real_time_delay (us, 1000 * 1000);
}

/* Sleeps execution for approximately NS nanoseconds.  Interrupts
   need not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_nsleep()
   instead if interrupts are enabled.*/
void
timer_ndelay (int64_t ns) 
{
  real_time_delay (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) 
{
  printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. */
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
	/* Timer tick emulation. */
	static int64_t real_ticks = 0;
	real_ticks++;
	register int ticks_per_upper_tick = thread_mlfqs
			? TIMER_FREQ_FAKENESS 
			: TIMER_FREQ_FAKENESS * TIMER_FREQ_REDUCE_FACTOR;
	if(real_ticks % ticks_per_upper_tick != 0){
		return;
	}

  ASSERT (intr_get_level () == INTR_OFF);

  ticks++;  /* Emulated timer tick. */
	barrier();
  int64_t now = ticks;
	struct list_elem *e;
	bool yield=false;

  thread_tick ();

	/* Per second job. */
	if (thread_mlfqs && (now % TIMER_FREQ == 0) ) {
		update_load_avg();
		update_recent_cpu();
	}
	
	/* Per 4 ticks job. */
	if (thread_mlfqs && (now % 4 == 0)) {
		/* For recent_cpu changed threads, update it's priority. */
		for ( e = list_begin (&rcc_list); e != list_end (&rcc_list) ;) {
			int i, new_priority;
			register int a;
			struct thread *t = list_entry (e, struct thread, rccelem);

			/* Recalculated it's priority. */
			a = ftopc ( t->recent_cpu ) / 40;
			a = a%10 > 5 ? a/10 + 1 : a/10;  /* .5 should be rounded down.
																					Because we'll use (-a). */
			new_priority = PRI_MAX - a - t->nice * 2;

			/* Adjust it's range. */
			if (new_priority < PRI_MIN)
				new_priority = PRI_MIN;
			else if (new_priority > PRI_MAX)
				new_priority = PRI_MAX;

			/* Set priority to new value. */
			t->priority = new_priority;
			t->original_priority = new_priority;

			/* If it's in ready_list, reset the location to new priority. */
			if (t->status==THREAD_READY){
				list_remove (&t->prielem);
				list_push_back (&pri_list[t->priority], &t->prielem);
			}

			/* Remove from recent_cpu changed list. And unmark the thread. */
			e = list_remove (e);   /* Now `e' is set to the next element. */
			t->rcc = false;

			if (!yield)  for ( i=PRI_MAX ; i>thread_priority ; i-- )
				if (!list_empty (&pri_list[i])){
					yield=true;
					intr_yield_on_return ();
					break;
				}
		}
	}

	/* Wake sleeping threads. */
	for ( e = list_begin (&sleep_list); e != list_end (&sleep_list); e = list_next (e)){
		struct thread *t = list_entry(e, struct thread, sleepelem);
		if ( t->awake_tick <= now ){
			e = list_remove (e)->prev;
			thread_unblock (t);
		}
	}
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) 
{
  /* Wait for a timer tick. */
  int64_t start = ticks;
  while (ticks == start)
    barrier ();

  /* Run LOOPS loops. */
  start = ticks;
  busy_wait (loops);

  /* If the tick count changed, we iterated too long. */
  barrier ();
  return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) 
{
  while (loops-- > 0)
    barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) 
{
  /* Convert NUM/DENOM seconds into timer ticks, rounding down.
          
        (NUM / DENOM) s          
     ---------------------- = NUM * TIMER_FREQ / DENOM ticks. 
     1 s / TIMER_FREQ ticks
  */
  int64_t ticks = num * TIMER_FREQ / denom;

  ASSERT (intr_get_level () == INTR_ON);
  if (ticks > 0)
    {
      /* We're waiting for at least one full timer tick.  Use
         timer_sleep() because it will yield the CPU to other
         processes. */                
      timer_sleep (ticks); 
    }
  else 
    {
      /* Otherwise, use a busy-wait loop for more accurate
         sub-tick timing. */
      real_time_delay (num, denom); 
    }
}

/* Busy-wait for approximately NUM/DENOM seconds. */
static void
real_time_delay (int64_t num, int32_t denom)
{
  /* Scale the numerator and denominator down by 1000 to avoid
     the possibility of overflow. */
  ASSERT (denom % 1000 == 0);
  busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000)); 
}
