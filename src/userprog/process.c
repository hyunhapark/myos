#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include "vm/frame.h"
#include "vm/page.h"

extern struct lock filesys_lock;
extern struct lock filesys_wlock;
extern struct lock filesys_rlock;

static bool load (const char *file_name, void (**eip) (void), void **esp, char *arg_start, int arg_len, int argc);

bool install_page (void *upage, void *kpage, bool writable);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR) {
    palloc_free_page (fn_copy); 
		return tid;
	}

	/* Wait for child finish loading. */
	struct thread *child = get_thread_by_tid (tid);
	ASSERT (child);
	sema_down (&child->loaded);

	/* If failed to load, return -1. */
	if (child->load_failed)
		return TID_ERROR;

	return tid;
}

/* A thread function that loads a user process and starts it
   running. */
void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

	int argc=0;
	int offset=0;
	char *p=0;
	char *ptr=0;

	/* Parse cmdline. */
	p = strtok_r(file_name, " \t\n", &ptr);
	argc++;
	offset += (strlen(p)+1);
	while (1) {
		int len=0;
		p = strtok_r(NULL, " \t\n", &ptr);
		if (p == NULL) break;
		len = strlen(p);
		argc++;
		if ( file_name+offset != p )
			strlcpy(file_name+offset, p, len+1);
		offset += len+1;
	}

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp, file_name, offset, argc);

  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success) {
		struct thread *cur = thread_current ();

		/* Tell parent that load is failed. */
		cur->load_failed = true;
		sema_up (&cur->loaded);

		exit (-1);
	}

	/* Tell parent that load is finished. */
	sema_up (&thread_current ()->loaded);
	
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
	int status = 0;

	struct thread *child = get_thread_by_tid (child_tid);
	if (child==NULL) {
		return -1;
	}
	sema_down (&child->exit_wait_sema);
	status = child->exit_status;

	enum intr_level old_level = intr_disable();

	/* Remove process from all_list. */
	list_remove (&child->allelem);

	/* Free memory of child's TCB. */
	palloc_free_page (child);

	intr_set_level (old_level);
  return status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

	lock_acquire (&filesys_lock);
	struct list_elem *e;
	for (e = list_begin (&cur->open_list); e != list_end (&cur->open_list);)
		{
			struct openfile *of = list_entry (e, struct openfile, openelem);
			if (of->f != NULL) {
				file_close (of->f);
			}
			e = list_remove (&of->openelem);
			free (of);
		}
	lock_release (&filesys_lock);

#ifdef VM
	hash_destroy (&cur->spt, page_destructor);
#endif

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, char *arg_start, int arg_len, int argc);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp, char *arg_start, int arg_len, int argc) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

	lock_acquire (&filesys_rlock);
	lock_acquire (&filesys_lock);

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

	t->my_binary = file;

	//if (!file->deny_write){
		file_deny_write (file);
	//}

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, arg_start, arg_len, argc))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
	lock_release (&filesys_lock);
	lock_release (&filesys_rlock);
  /* We arrive here whether the load is successful or not. */
  return success;
}

/* load() helpers. */

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

#ifdef VM
	if (file_length (file) < (off_t)(ofs + read_bytes))
		return false;
#else
	file_seek (file, ofs);
#endif
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

#ifdef VM
      /* Just record that it is valid page to the SPT. */
			uint8_t segtype = writable ? SEGTYPE_DATA : SEGTYPE_CODE;
			if (!page_alloc (upage, file, ofs, page_read_bytes, page_zero_bytes, 
					writable, segtype)){
				return false;
			}
			ofs += page_read_bytes;
#else
			/* Get a page of memory. */
			uint8_t *kpage = palloc_get_page (PAL_USER);
			if (kpage == NULL)
				return false;

			/* Load this page. */
			if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
				{
					palloc_free_page (kpage);
					return false;
				}
			memset (kpage + page_read_bytes, 0, page_zero_bytes);

			/* Add the page to the process's address space. */
			if (!install_page (upage, kpage, writable))
				{
					palloc_free_page (kpage);
					return false;
				}
#endif

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, char *arg_start, int arg_len, int argc) 
{
  uint8_t *kpage;
	uint8_t *upage = ((uint8_t *) PHYS_BASE) - PGSIZE;
  bool success = false;

#ifdef VM
	success = page_alloc (upage, NULL, 0, 0, PGSIZE, 
			true, SEGTYPE_STACK);
	if (!success)
		return false;
	kpage = frame_alloc (upage);
	memset (kpage, 0, PGSIZE);
#else
	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
#endif
	if (kpage == NULL)
		return false;

	// TODO : page_alloc for 8MB sized Stack.
	/* Mark valid to 2046 pages for stack. */
#ifdef VM
	int i;
	for (i=1; i < STACK_PAGES; i++) {
		if (!page_alloc (upage - (PGSIZE*i), NULL, 0, 0, PGSIZE,
				true, SEGTYPE_STACK)) {
			return false;
		}
	}
#endif

	/* Add the page to the process's address space. */
	success = install_page (upage, kpage, true);

  if (success)
		{
			memcpy(kpage + PGSIZE - arg_len, arg_start, arg_len);
			uint32_t *kp = (uint32_t *) ((( ( ((unsigned)kpage+PGSIZE) - arg_len )>>2 )-1-argc-2)*4);
			uint32_t *up = (uint32_t *) ((( ( ((unsigned)PHYS_BASE) - arg_len )>>2 )-1-argc)*4);
			*kp = (uint32_t) argc;
			*(kp+1) = (uint32_t) up;
			*(kp+2) = (uint32_t) (up+argc+1);
			char *p= (char *) *(kp+2);
			if(*p!=0){
			}else if(*(p+1)!=0){
				*(kp+2) = (uint32_t) (p+1);
			}else if(*(p+2)!=0){
				*(kp+2) = (uint32_t) (p+2);
			}else if(*(p+3)!=0){
				*(kp+2) = (uint32_t) (p+3);
			}
			int i;
			for (i=1;i<argc;i++){
				char *s = (char *) *(kp+1+i);
			*(kp+2+i) = (uint32_t) (s+strlen(s)+1);
			}
			*(kp+2+argc)= (uint32_t) 0;

			*esp = up-3;
		}
	else
		{
#ifdef VM
			frame_free (kpage);
#else
			palloc_free_page (kpage);
#endif
		}
  return success;
}

