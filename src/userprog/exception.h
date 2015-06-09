#ifndef USERPROG_EXCEPTION_H
#define USERPROG_EXCEPTION_H

#include <stdbool.h>

/* Page fault error code bits that describe the cause of the exception.  */
#define PGF_P 0x1    /* 0: not-present page. 1: access rights violation. */
#define PGF_W 0x2    /* 0: read, 1: write. */
#define PGF_U 0x4    /* 0: kernel, 1: user process. */

void exception_init (void);
void exception_print_stats (void);
bool install_page (void *upage, void *kpage, bool writable);
bool demand_paging (const void *paging_addr, bool write);

#endif /* userprog/exception.h */
