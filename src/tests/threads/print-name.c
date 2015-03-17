/* Creates N threads, each of which sleeps a different, fixed
   duration, M times.  Records the wake-up order and verifies
   that it is valid. */

#include "tests/threads/tests.h"

void
test_print_name (void) 
{
  msg ("Course : SWE3004");
  msg ("ID : 2012311437");
  msg ("Name : Hyunha Park");
}
