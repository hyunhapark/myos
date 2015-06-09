#ifndef VM_CLOCK_H
#define VM_CLOCK_H

#include "vm/frame.h"
#include <clist.h>

struct fte *clock_get_victim (struct clist *);

#endif
