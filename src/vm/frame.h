#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include <stdint.h>
#include "threads/thread.h"

/* Frame Table Entry. */
struct fte
  {
		struct list_elem celem;     /* For circular list. */
		void *paddr;                /* Physical address of the frame. */
		struct list reference_list; /* List of reference. Process, vaddr */
		uint32_t refcnt;            /* Reference count. */
  };

/* FTE reference. (Process, vaddr) */
struct fte_reference
  {
		struct thread *process;     /* Process who has page that refers 
                                   the frame. */
		void *vaddr;                /* Virtual address of that page. */
		struct list_elem refelem;   /* List element for reference_list
                                   in the FTE. */
  };

void frame_init (void);

void *frame_alloc (void *);  /* Allocate new frame from physical memory. 
                                And create FTE(Frame Table Entry) to manage
                                the frame. And put it into FT(Frame Table; 
                                implemented by a circular list.) */
void frame_free (void *);
void init_fte (struct fte *fte);

#endif
