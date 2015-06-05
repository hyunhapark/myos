#ifndef DEVICES_TIMER_H
#define DEVICES_TIMER_H

#include <round.h>
#include <stdint.h>

/* Number of timer interrupts per second. */
#define TIMER_FREQ 100              /* Software emulated freq */

#ifndef FILESYS
  #define TIMER_FREQ_FAKENESS 10			/* REAL_TIMER_FREQ / TIMER_FREQ */
  #define REAL_TIMER_FREQ (TIMER_FREQ*TIMER_FREQ_FAKENESS) /* Real hardware(bochs) freq */
  #define TIMER_FREQ_REDUCE_FACTOR 4     /* For non-MLFQS. Reduce timer by this factor. */
#else
  #define TIMER_FREQ_FAKENESS 1
  #define REAL_TIMER_FREQ TIMER_FREQ
  #define TIMER_FREQ_REDUCE_FACTOR 1
#endif

void timer_init (void);
void timer_calibrate (void);

int64_t timer_ticks (void);
int64_t timer_elapsed (int64_t);

/* Sleep and yield the CPU to other threads. */
void timer_sleep (int64_t ticks);
void timer_msleep (int64_t milliseconds);
void timer_usleep (int64_t microseconds);
void timer_nsleep (int64_t nanoseconds);

/* Busy waits. */
void timer_mdelay (int64_t milliseconds);
void timer_udelay (int64_t microseconds);
void timer_ndelay (int64_t nanoseconds);

void timer_print_stats (void);

#endif /* devices/timer.h */
