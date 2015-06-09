#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include <hash.h>
#include <string.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

extern struct lock filesys_lock;
extern struct lock filesys_rlock;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
			exit (-1);

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
			exit (-1);
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PGF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  //bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  //bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  //not_present = (f->error_code & PGF_P) == 0;
  write = (f->error_code & PGF_W) != 0;
  //user = (f->error_code & PGF_U) != 0;

#ifdef VM
	if (demand_paging (fault_addr, write)) {
		return;
	}
#endif

#ifdef USERPROG
	/* Check if it is fault of user process by system call. */
	if (intr_syscall_context ()) {
		f->cs = SEL_UCSEG;
	}
#endif

  kill (f);
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

#ifdef VM
bool
demand_paging (const void *paging_addr, bool write)
{
	// Do frame_alloc if valid access. else return false;
  /* TODO : code sharing.
		 block_sector_t sector_idx = byte_to_sector (inode, offset); */
	struct spte search;
	struct hash_elem *e;
	struct spte *p;

	search.vaddr = pg_round_down (paging_addr);
	e = hash_find (&thread_current()->spt, &search.helem);
	if (e!=NULL) { /* Valid page */
		p = hash_entry (e, struct spte, helem);
		if (p->writable || !write) { /* But was write to non-writable page. */
			void *fr=NULL;
			if (pagedir_get_page (thread_current ()->pagedir, p->vaddr) == NULL)
				{
					bool dirty = false;
					switch (p->bpage.type) {
					case BACKING_TYPE_FILE: /* C, clean D, clean F */
						fr = frame_alloc (p->vaddr);
						lock_acquire (&filesys_rlock);
						lock_acquire (&filesys_lock);
						file_seek (p->bpage.file, p->bpage.file_ofs);
						if (file_read (p->bpage.file, fr, PGSIZE - p->bpage.zero_bytes) 
								!= (off_t)(PGSIZE - p->bpage.zero_bytes)) {
							lock_release (&filesys_lock);
							lock_release (&filesys_rlock);
							frame_free (fr);
							return false;
							//PANIC ("page_fault(): Read binary failed.");
						}
						lock_release (&filesys_lock);
						lock_release (&filesys_rlock);
						memset (fr + (PGSIZE - p->bpage.zero_bytes),
								0, p->bpage.zero_bytes);
						break;
					case BACKING_TYPE_SWAP: /* dirty D, S, dirty F */
						fr = frame_alloc (p->vaddr);
						swap_load (p->bpage.sector_idx, fr);
						swap_free_slot (p->bpage.sector_idx);
						dirty = true;
						break;
					case BACKING_TYPE_ZERO:
						fr = frame_alloc(p->vaddr);
						memset (fr, 0, PGSIZE);
							break;
					default: break;
					}
					ASSERT (fr);
					/* Add the page to the process's address space. */
					if (!install_page (p->vaddr, fr, p->writable)) 
						{
							frame_free (fr);
							PANIC ("page_fault(): page install failed.");
						}
					pagedir_set_dirty (thread_current ()->pagedir, p->vaddr, dirty);
				}
			return true;  /* Valid access. */
		}
	}
	return false;		/* Invalid access. Let Kernel to handle. */
}
#endif

