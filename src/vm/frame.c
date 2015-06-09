#include "vm/frame.h"
#include <list.h>
#include <clist.h>
#include <hash.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "vm/clock.h"
#include "vm/wsclock.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "vm/swap.h"
#include "vm/page.h"
#include "userprog/pagedir.h"
#define automalloc(x)  ( (__typeof__(x)) malloc(sizeof *x) )

/* A list of available frames. */
//static struct list avail_frames;

/* FT(Frame Table). Circular list of every preemtible frames. */
static struct clist ft;

void
frame_init (void)
{
	//list_init (&avail_frames);
	clist_init (&ft);
}

static struct fte *
frame_get_victim (void)
{
	struct fte *ret = NULL;
#ifdef WSCLOCK  /* WSclock algorithm. */
	ret = wsclock_get_victim (&ft);
#else  /* For default, use clock algorithm. */
	ret = clock_get_victim (&ft);
#endif
	return ret;
}

void *
frame_alloc (void *vaddr)
{
	// TODO : lock??
	void *fr = palloc_get_page (PAL_USER);
	if (fr == NULL) { /* Out of frame. */
		enum intr_level old_level = intr_disable ();
		struct fte *victim = frame_get_victim ();
		struct list *rl = &victim->reference_list;
		struct list_elem *e;
		block_sector_t swap = SWAP_NONE;
		bool swapout = false;
		for (e = list_begin (rl); e != list_end (rl);
				 e = list_next (e))
			{
				struct fte_reference *re = 
						list_entry (e, struct fte_reference, refelem);
				struct spte search;
				search.vaddr = re->vaddr;
				struct hash_elem *he = hash_find (&re->process->spt, &search.helem);
				ASSERT (he);
				struct spte *spte = hash_entry (he, struct spte, helem);
				if (spte->bpage.type == SEGTYPE_CODE
						|| (spte->bpage.type == SEGTYPE_DATA 
								&& pagedir_is_dirty (re->process->pagedir, re->vaddr)) ) {
					/* Simply discard the page. */
					pagedir_clear_page (re->process->pagedir, re->vaddr);
				} else {
					swapout = true;
					pagedir_clear_page (re->process->pagedir, re->vaddr);
				}
			}
		if (swapout) {
			for (e = list_begin (rl); e != list_end (rl);
					 e = list_next (e))
				{
					struct fte_reference *re = 
							list_entry (e, struct fte_reference, refelem);
					struct spte search;
					search.vaddr = re->vaddr;
					struct hash_elem *he = 
							hash_find (&re->process->spt, &search.helem);
					ASSERT (he);
					struct spte *spte = hash_entry (he, struct spte, helem);
					spte->bpage.type = BACKING_TYPE_SWAP;
					if (swap == SWAP_NONE)
						swap = swap_get_slot ();
					spte->bpage.sector_idx = swap;
					spte->bpage.zero_bytes = 0;
				}
		}
		intr_set_level (old_level);
		fr = victim->paddr;

		/* Swap out. */
		if (swapout) {
			swap_store (swap, fr);
		}
	}

	struct fte *fte = automalloc (fte);
	if (fte == NULL)
		goto this_is_disaster;
	init_fte (fte);
	clist_push_back (&ft, &fte->celem);
	fte->paddr = fr;

	struct fte_reference *fte_ref = automalloc (fte_ref);
	if (fte_ref == NULL)
		goto this_is_disaster;
	fte_ref->process = thread_current ();
	ASSERT (fte_ref->process->is_process);
	fte_ref->vaddr = vaddr;

	list_push_back (&fte->reference_list, &fte_ref->refelem);
	fte->refcnt = 1;

	memset (fr, 0, PGSIZE);

	return fr;

this_is_disaster:
	palloc_free_page (fr);
	return NULL;
}

void
frame_free (void *fr)
{
	struct list_elem *e;
	struct fte *p;

	e = clist_hand(&ft);
	if (e != NULL) {
		p = clist_entry (e, struct fte, celem);
		ASSERT (e && p);
		if (p->paddr == fr)
			goto do_frame_free;
		for (e=e->next;e!=clist_hand(&ft); e=e->next)
			{
				p = clist_entry (e, struct fte, celem);
				ASSERT (e && p);
				if (p->paddr == fr)
					goto do_frame_free;
			}
	}
	return;

do_frame_free:
	p->refcnt--;
	struct thread *cur = thread_current ();
	struct list_elem *re;
	for (re = list_begin (&p->reference_list); 
			 re != list_end (&p->reference_list); re = list_next(re))
		{
			struct fte_reference *fter = 
					list_entry (re, struct fte_reference, refelem);
			if (fter->process == cur) {
				list_remove (re);
				free (fter);
				break;
			}
		}
	if (p->refcnt==0) {
		palloc_free_page (fr);
		clist_remove (&ft, &p->celem);
		free(p);
	}
}

void
init_fte (struct fte *fte)
{
	fte->celem.prev = NULL;
	fte->celem.next = NULL;
	fte->paddr = NULL;
	list_init (&fte->reference_list);
	fte->refcnt=0;
}


