#include "clist.h"
#include "../debug.h"

/* Our doubly linked lists have two header elements: the "head"
   just before the first element and the "tail" just after the
   last element.  The `prev' link of the front header is null, as
   is the `next' link of the back header.  Their other two links
   point toward each other via the interior elements of the list.

   An empty list looks like this:

                      +------+     +------+
                  <---| head |<--->| tail |--->
                      +------+     +------+

   A list with two elements in it looks like this:

        +------+     +-------+     +-------+     +------+
    <---| head |<--->|   1   |<--->|   2   |<--->| tail |<--->
        +------+     +-------+     +-------+     +------+

   The symmetry of this arrangement eliminates lots of special
   cases in list processing.  For example, take a look at
   list_remove(): it takes only two pointer assignments and no
   conditionals.  That's a lot simpler than the code would be
   without header elements.

   (Because only one of the pointers in each header element is used,
   we could in fact combine them into a single header element
   without sacrificing this simplicity.  But using two separate
   elements allows us to do a little bit of checking on some
   operations, which can be valuable.) */



/* Returns true if ELEM is a head, false otherwise. */
static inline bool
is_hand (struct clist *cl, struct list_elem *elem)
{
  return cl != NULL && elem != NULL && cl->hand != NULL && elem == cl->hand;
}

/* Returns true if ELEM is a tail, false otherwise. */
static inline bool
is_back (struct clist *cl, struct list_elem *elem)
{
  return cl != NULL && elem != NULL && cl->hand != NULL && elem->next == cl->hand;
}

/* Initializes LIST as an empty list. */
void
clist_init (struct clist *cl)
{
  ASSERT (cl != NULL);
  cl->hand = NULL;
  cl->size = 0;
}

struct list_elem *
clist_hand (struct clist *cl)
{
	ASSERT (cl != NULL);
	return cl->hand;
}

struct list_elem *
clist_back (struct clist *cl)
{
	ASSERT (cl != NULL);
	return (cl->hand != NULL) ? cl->hand->prev : NULL;
}

struct list_elem *
clist_next (struct list_elem *e)
{
	ASSERT (e != NULL);
	return e->next;
}

struct list_elem *
clist_prev (struct list_elem *e)
{
	ASSERT (e != NULL);
	return e->prev;
}

struct list_elem *
clist_go (struct clist *cl) /* Moves hand to next element. */
{
	ASSERT (cl != NULL);

	if(cl->hand == NULL)
		return NULL;
	return cl->hand = cl->hand->next;
}

void
clist_insert (struct clist *cl, struct list_elem *e, struct list_elem *enew)
{
	ASSERT (cl != NULL);
	ASSERT (e != NULL);
	ASSERT (enew != NULL);

	enew->prev = e;
	enew->next = e->next;
	e->next->prev = enew;
	e->next = enew;
	cl->size++;
}

void
clist_push_back (struct clist *cl, struct list_elem *e)
{
	ASSERT (cl != NULL);
	ASSERT (e != NULL);

	if (cl->size==0) {
		cl->hand = e;
		cl->size++;
	} else {
		ASSERT (cl->hand != NULL);
		clist_insert (cl, cl->hand->prev, e);
	}
}

struct list_elem *
clist_remove (struct clist *cl, struct list_elem *e)
{
	struct list_elem *ret = NULL;
	ASSERT (cl != NULL);
	ASSERT (e != NULL);

	if (cl->size == 1) {
		cl->hand = NULL;
		e->prev = NULL;
		e->next = NULL;
		ret = NULL;
	} else {
		if (cl->hand == e) {
			cl->hand = e->next;
			e->prev->next = e->next;
			e->next->prev = e->prev;
		} else {
			e->prev->next = e->next;
			e->next->prev = e->prev;
		}
		ret = e->next;
		e->prev = NULL;
		e->next = NULL;
	}
	cl->size--;
	return ret;
}

struct list_elem *
clist_pop_hand (struct clist *cl)
{
	ASSERT (cl != NULL);

	return clist_remove (cl, cl->hand);
}

size_t
clist_size (struct clist *cl)
{
	ASSERT (cl != NULL);

	return cl->size;
}

bool
clist_empty (struct clist *cl)
{
	ASSERT (cl != NULL);

	return cl->size==0;
}

