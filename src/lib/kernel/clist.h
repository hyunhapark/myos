#ifndef __LIB_KERNEL_CLIST_H
#define __LIB_KERNEL_CLIST_H

/* Doubly linked circular list.

   This implementation of a doubly linked circular list does not require
   use of dynamically allocated memory.  Instead, each structure
   that is a potential list element must embed a struct list_elem
   member.  All of the list functions operate on these `struct
   list_elem's.  The list_entry macro allows conversion from a
   struct list_elem back to a structure object that contains it.

   Glossary of clist terms:

     - "hand": The element that current hand is pointing.  Undefined
        in an empty clist.  Returned by clist_hand().

     - "back": The element before hand in a clist.  Undefined in 
       an empty list.  Returned by clist_back().

*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "list.h"

/* Clist. */
struct clist 
  {
    struct list_elem *hand;      /* Hand of clist. */
		size_t size;                    /* Size of clist. */
  };

/* Clist initialization.

   A clist may be initialized by calling clist_init():

       struct clist my_list;
       clist_init (&my_list); */

void clist_init (struct clist *);

/* Clist traversal. */
struct list_elem *clist_hand (struct clist *);
struct list_elem *clist_back (struct clist *);
struct list_elem *clist_next (struct list_elem *);
struct list_elem *clist_prev (struct list_elem *);
struct list_elem *clist_go (struct clist *); /* Moves hand to next element. */

/* Clist insertion. */
void clist_insert (struct clist *, struct list_elem *, struct list_elem *);
void clist_push_back (struct clist *, struct list_elem *);

/* Clist removal. */
struct list_elem *clist_remove (struct clist *, struct list_elem *);
struct list_elem *clist_pop_hand (struct clist *);

/* Clist properties. */
size_t clist_size (struct clist *);
bool clist_empty (struct clist *);

/* Converts pointer to clist element LIST_ELEM into a pointer to
   the structure that LIST_ELEM is embedded inside.  Supply the
   name of the outer structure STRUCT and the member name MEMBER
   of the list element. */
#define clist_entry(LIST_ELEM, STRUCT, MEMBER)          \
        ((STRUCT *) ((uint8_t *) &(LIST_ELEM)->next     \
                     - offsetof (STRUCT, MEMBER.next)))

#endif /* lib/kernel/clist.h */
