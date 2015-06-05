#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <hash.h>
#include "filesys/file.h"
#include "filesys/off_t.h"
#include "devices/block.h"

struct backing_page
  {
		uint8_t type;                /* Backing type */
#define BACKING_TYPE_FILE   0x01
#define BACKING_TYPE_SWAP   0x02
#define BACKING_TYPE_ZERO   0x03
		struct file *file;           /* File. */
		off_t file_ofs;              /* Offset of file. */
		block_sector_t sector_idx;   /* Index of starting sector of swap. */
		uint32_t zero_bytes;         /* Number of padding zeros. */
  };

/* Supplemental Page Table Entry. */
struct spte
  {
		bool writable;               /* Writable? */
		uint8_t segtype;             /* Which segment? or mmaped file? */
#define SEGTYPE_CODE   0x01
#define SEGTYPE_DATA   0x02
#define SEGTYPE_HEAP   0x03      /* Unused. */
#define SEGTYPE_STACK  0x04
#define SEGTYPE_FILE   0x05      /* Memory maped file. */
		struct backing_page bpage;   /* Backing info. */
		struct hash_elem helem;      /* Hash table (SPT; 
                                    Supplemental Page Table) element. */
		void *vaddr;                 /* [Key] Virtual address. */
  };

void page_init (void);

bool page_alloc (uint8_t *upage, struct file *backing, off_t ofs, 
		uint32_t read_bytes, uint32_t zero_bytes, bool writable, int segtype);

unsigned page_hash (const struct hash_elem *p_, void *aux);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_,
		void *aux UNUSED);

#endif
