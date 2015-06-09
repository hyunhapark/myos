#include "vm/clock.h"
#include <clist.h>
#include "userprog/pagedir.h"

struct fte *
clock_get_victim (struct clist *ft)
{
	//TODO
	struct list_elem *e;
	for (e = clist_hand(ft); clist_size (ft) > 0; e = clist_go (ft))
		{
			int accessed_cnt = 0;
			struct fte *fte = clist_entry (e, struct fte, celem);
			struct list *l = &fte->reference_list;
			struct list_elem *ee;
			for (ee = list_begin (l); ee != list_end (l);
					 ee = list_next (ee))
				{
					struct fte_reference *fte_r = 
							list_entry (ee, struct fte_reference, refelem);
					if (pagedir_is_accessed (fte_r->process->pagedir, fte_r->vaddr))
						{
							accessed_cnt++;
							pagedir_set_accessed (fte_r->process->pagedir, 
									fte_r->vaddr, false);
						}
				}
			if (accessed_cnt > 0) {   /* Give a second chance. */
				continue;
			} else {   /* This is now the victim. */
				clist_remove (ft, &fte->celem);
				return fte;
			}
		}
	return NULL;
}

