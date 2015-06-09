#ifndef VM_WSCLOCK_H
#define VM_WSCLOCK_H

#include "vm/frame.h"
#include <clist.h>

struct fte *wsclock_get_victim (struct clist *);

#endif
