#include "vm/page.h"
#include <hash.h>
#include <stdio.h>
#include <stdlib.h>
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#define automalloc(x)  ( (__typeof__(x)) malloc(sizeof *x) )

void
page_init (void)
{
}


bool
page_alloc (uint8_t *upage, struct file *backing, off_t ofs, 
		uint32_t read_bytes, uint32_t zero_bytes, bool writable, int segtype)
{
	//TODO
	ASSERT ((read_bytes + zero_bytes) == PGSIZE);
	ASSERT (pg_ofs (upage) == 0);

	struct spte *spte = automalloc(spte);
	spte->writable = writable;
	spte->segtype = segtype;
	spte->bpage.file = backing;
	spte->bpage.file_ofs = ofs;
	spte->bpage.zero_bytes = zero_bytes;
	if (backing)
		spte->bpage.type = BACKING_TYPE_FILE;
	else
		spte->bpage.type = BACKING_TYPE_ZERO;
	spte->vaddr = upage;

	if (hash_insert (&thread_current()->spt, &spte->helem)) {
		free (spte);
		return false;
	} else {
		return true;
	}
}

unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
	const struct spte *p = hash_entry (p_, struct spte, helem);
	return hash_bytes (&p->vaddr, sizeof p->vaddr);
}

bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
		void *aux UNUSED)
{
	const struct spte *a = hash_entry (a_, struct spte, helem);
	const struct spte *b = hash_entry (b_, struct spte, helem);

	return a->vaddr < b->vaddr;
}

void
page_destructor (struct hash_elem *a, void *aux UNUSED)
{
	free (hash_entry (a, struct spte, helem));
}

