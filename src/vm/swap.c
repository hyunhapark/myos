#include "vm/swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

#define BLOCK_SECTOR_RATIO  (PGSIZE / BLOCK_SECTOR_SIZE)

static struct lock st_lock;
static struct lock swap_lock;
static struct bitmap *st;   /* Swap Table */

struct block *swap_dev;

void
swap_init (void)
{
	lock_init (&st_lock);
	lock_init (&swap_lock);
	swap_dev = block_get_role (BLOCK_SWAP);

	ASSERT(swap_dev);

	block_sector_t sector_cnt = block_size (swap_dev);
	size_t block_cnt = sector_cnt / BLOCK_SECTOR_RATIO;

	size_t bm_size = bitmap_buf_size (block_cnt);
	void *base = malloc (bm_size);
	ASSERT (base);
	st = bitmap_create_in_buf (block_cnt, base, bm_size);
}

block_sector_t
swap_get_slot (void)
{
	block_sector_t idx;
	lock_acquire (&st_lock);
	size_t b_idx = bitmap_scan_and_flip (st, 0, 1, false);
	if (b_idx != BITMAP_ERROR) {
		idx = BLOCK_SECTOR_RATIO * b_idx;
	} else{
		idx = BITMAP_ERROR;
	}
	lock_release (&st_lock);
	return idx;
}

void
swap_free_slot (block_sector_t idx)
{
	size_t b_idx = idx / BLOCK_SECTOR_RATIO;

	ASSERT (idx % BLOCK_SECTOR_RATIO == 0);
	ASSERT (bitmap_test (st, b_idx));

	lock_acquire (&st_lock);
	bitmap_flip (st, b_idx);
	lock_release (&st_lock);
}

bool
swap_store (block_sector_t to, const void *from)
{
	lock_acquire (&swap_lock);
	block_write (swap_dev, to, from);
	lock_release (&swap_lock);
	return true;
}

bool
swap_load (block_sector_t from, void *to)
{
	lock_acquire (&swap_lock);
	block_read (swap_dev, from, to);
	lock_release (&swap_lock);
	return true;
}

