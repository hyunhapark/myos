#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdbool.h>
#include "devices/block.h"

#define SWAP_NONE ((uint32_t) -1)

void swap_init (void);
block_sector_t swap_get_slot (void);
void swap_free_slot (block_sector_t);
bool swap_store (block_sector_t, const void *);
bool swap_load (block_sector_t, void *);

#endif
