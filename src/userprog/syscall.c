#include "userprog/syscall.h"
#include <list.h>
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "devices/shutdown.h"
#include "threads/palloc.h"
#include "userprog/process.h"
#include "threads/synch.h"
#include "devices/input.h"

#define MIN(x, y)	(((x)>(y))?(y):(x))
#define MAX(x, y)	(((x)>(y))?(x):(y))

/* Lock of whole file system. [TODO]Should be divided into 
	 multiple small locks later. */
struct lock filesys_lock;

static void syscall_handler (struct intr_frame *);
static bool str_over_boundary (const char *);
static char *strlbond (char *, const char *, size_t);
static int get_next_fd (struct thread *);

/* Projects 2 and later. */
static void halt (void);
static pid_t exec (const char *file);
static int wait (pid_t);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned length);
static int write (int fd, const void *buffer, unsigned length);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);

/* Project 3 and optionally project 4. */
static mapid_t mmap (int fd, void *addr) UNUSED;
static void munmap (mapid_t) UNUSED;

/* Project 4 only. */
static bool chdir (const char *dir) UNUSED;
static bool mkdir (const char *dir) UNUSED;
static bool readdir (int fd, char name[READDIR_MAX_LEN + 1]) UNUSED;
static bool isdir (int fd) UNUSED;
static int inumber (int fd) UNUSED;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
	lock_init (&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
	uint32_t *esp, *kesp = f->esp;

	/* Check %esp. */
	if ((void *)kesp >= PHYS_BASE) exit (-1);
	
	esp = (uint32_t *)user_vtop ((const void *) kesp);

	int syscall_num = *esp;

	/* Check %esp more. */
	switch(syscall_num) {
	/* If argument is one. */
	case SYS_EXIT: case SYS_EXEC: case SYS_WAIT: case SYS_REMOVE:
	case SYS_OPEN: case SYS_FILESIZE: case SYS_TELL: case SYS_CLOSE:
	case SYS_MUNMAP: case SYS_CHDIR: case SYS_MKDIR: case SYS_ISDIR:
	case SYS_INUMBER:
		if ((void *)(kesp+1) >= PHYS_BASE) exit (-1);
		break;
	/* If argument is two. */
	case SYS_CREATE: case SYS_SEEK: case SYS_MMAP: case SYS_READDIR:
		if ((void *)(kesp+2) >= PHYS_BASE) exit (-1);
		break;
	/* If argument is three. */
	case SYS_READ: case SYS_WRITE:
		if ((void *)(kesp+3) >= PHYS_BASE) exit (-1);
		break;
	}

	switch(syscall_num){
  /* Projects 2 and later. */
	case SYS_HALT:     /*void*/      halt ();  break;
	case SYS_EXIT:     /*void*/      exit ((int) *(esp+1));  break;
	case SYS_EXEC:     f->eax =      exec ((const char *) *(esp+1));  break;
	case SYS_WAIT:     f->eax =      wait ((pid_t) *(esp+1));  break;
	case SYS_CREATE:   f->eax =    create ((const char *) *(esp+1), (unsigned) *(esp+2));  break;
	case SYS_REMOVE:   f->eax =    remove ((const char *) *(esp+1));  break;
	case SYS_OPEN:     f->eax =      open ((const char *) *(esp+1));  break;
	case SYS_FILESIZE: f->eax =  filesize ((int) *(esp+1));  break;
	case SYS_READ:     f->eax =      read ((int) *(esp+1), (void *) *(esp+2), (unsigned) *(esp+3));  break;
	case SYS_WRITE:    f->eax =     write ((int) *(esp+1), (const void *) *(esp+2), (unsigned) *(esp+3));  break;
	case SYS_SEEK:     /*void*/      seek ((int) *(esp+1), (unsigned) *(esp+2));  break;
	case SYS_TELL:     f->eax =      tell ((int) *(esp+1));  break;
	case SYS_CLOSE:    /*void*/     close ((int) *(esp+1));  break;

  /* Project 3 and optionally project 4. */
	case SYS_MMAP:     printf("SYS_MMAP\n");  break;
	case SYS_MUNMAP:   printf("SYS_MUNMAP\n");  break;

  /* Project 4 only. */
	case SYS_CHDIR:    printf("SYS_CHDIR\n");  break;
	case SYS_MKDIR:    printf("SYS_MKDIR\n");  break;
	case SYS_READDIR:  printf("SYS_READDIR\n");  break;
	case SYS_ISDIR:    printf("SYS_ISDIR\n");  break;
	case SYS_INUMBER:  printf("SYS_INUMBER\n");  break;
	default:	PANIC ("Wrong system call number.\n");  break;
	}
}

static bool
str_over_boundary (const char *s){
	char *page = (char *) pg_round_down (s);
	uintptr_t offset = pg_ofs (s);

	for (; offset<PGSIZE; offset++)
		if(page[offset]=='\0')
			return false;
	return true;
}

/* Bond and copy string from user virtual address src to kernel 
	 page dst. It cannot copy string over one page. */
static char *
strlbond (char *dst, const char *src, size_t size){
	
	const char *s = (const char *) user_vtop ((const void *) src);
	uintptr_t soffset = pg_ofs(src);
	uintptr_t doffset = 0;
	if (str_over_boundary (s)){
		memcpy (dst, s, MIN(PGSIZE-soffset, size-1));
		doffset += MIN(PGSIZE-soffset, size-1);
		dst[doffset] = '\0';
		s = pg_round_down (src + PGSIZE);
	}

	ASSERT (!str_over_boundary (s));

	strlcpy (dst+doffset, s, size-doffset);

	return dst;
}

/* Search file that has opened with fd in thread's open_list. */
struct file *
get_file_by_fd (int fd) {
	struct thread *t = thread_current ();
	struct list_elem *e;

  for (e = list_begin (&t->open_list); e != list_end (&t->open_list);
       e = list_next (e))
    {
			struct openfile *of = list_entry (e, struct openfile, openelem);
			if ( of->fd == fd )
				return of->f;
		}
	return NULL;
}

/* Search openfile that has opened with fd in thread's open_list. */
struct openfile *
get_openfile_by_fd (int fd) {
	struct thread *t = thread_current ();
	struct list_elem *e;

  for (e = list_begin (&t->open_list); e != list_end (&t->open_list);
       e = list_next (e))
    {
			struct openfile *of = list_entry (e, struct openfile, openelem);
			if ( of->fd == fd )
				return of;
		}
	return NULL;
}

static int
get_next_fd (struct thread *t) {
	return (++t->lastfd);
}

static void
halt (void) 
{
  shutdown_power_off ();
}

void
exit (int status)
{
	struct thread *cur = thread_current ();
	struct file *f = cur->my_binary;
	if (f!=NULL){
		if (f->deny_write)
			file_allow_write (f);
		lock_acquire (&filesys_lock);
		file_close (f);
		lock_release (&filesys_lock);
	}

	cur->exit_status = status;
	printf ("%s: exit(%d)\n", cur->name, status);
	thread_exit ();
}

static pid_t
exec (const char *_cmd_line)
{
	if ((void *)_cmd_line >= PHYS_BASE) {
		exit (-1);
	}
	char *cmd_line = (char *) palloc_get_page (0);
	if (cmd_line==NULL)
		return -1;
	strlbond (cmd_line, _cmd_line, PGSIZE);
	pid_t pid = (pid_t) process_execute (cmd_line);
	palloc_free_page (cmd_line);

	return pid;
}

static int
wait (pid_t pid)
{
	return process_wait((tid_t) pid);
}

static bool
create (const char *_file, unsigned initial_size)
{
	if ((void *)_file >= PHYS_BASE)
		exit (-1);

	bool success = false;
	char *file = (char *) palloc_get_page (0);
	if (file==NULL)
		return false;
	strlbond (file, _file, PGSIZE);

	lock_acquire (&filesys_lock);
	success = filesys_create (file, initial_size);
	lock_release (&filesys_lock);

	palloc_free_page (file);
  return success;
}

static bool
remove (const char *_file)
{
	if ((void *)_file >= PHYS_BASE)
		exit (-1);

	bool success = false;
	char *file = (char *) palloc_get_page (0);
	if (file==NULL)
		return false;
	strlbond (file, _file, PGSIZE);

	lock_acquire (&filesys_lock);
	success = filesys_remove (file);
	lock_release (&filesys_lock);

	palloc_free_page (file);
  return success;
}

static int
open (const char *_file)
{
	if ((void *)_file >= PHYS_BASE)
		exit (-1);

	struct thread *t = thread_current ();
	struct file *f;

	char *file = (char *) palloc_get_page (0);
	if (file==NULL)
		return -1;
	strlbond (file, _file, PGSIZE);

	lock_acquire (&filesys_lock);
	f = filesys_open (file);

	palloc_free_page (file);

	if (f==NULL) { /* File open fail. */
		lock_release (&filesys_lock);
		return -1;
	}

	/* Add (fd, f) mapping into thread's open_list */
	struct openfile *of = (struct openfile *) calloc (1, sizeof(struct openfile));
	if (of==NULL) {
		lock_release (&filesys_lock);
		return -1;
	}
	of->fd = get_next_fd(t);
	of->f = f;
	list_push_back (&t->open_list, &of->openelem);

	lock_release (&filesys_lock);
  return of->fd;
}

static int
filesize (int fd) 
{
	int len;

	lock_acquire (&filesys_lock);
	struct file *f = get_file_by_fd (fd);

	if (f==NULL) {
		lock_release (&filesys_lock);
		return -1;
	}

	len = (int) file_length (f);
	lock_release (&filesys_lock);

  return len;
}

static int
read (int fd, void *_buffer, unsigned size)
{
	off_t offset = 0;

	if ((void *)(_buffer+size-1) >= PHYS_BASE)
		exit (-1);

	char *buffer = (char *) user_vtop (_buffer);
	uintptr_t remain = (uintptr_t) pg_round_down(buffer+PGSIZE) - (uintptr_t) buffer;

	if (fd == STDIN_FILENO)
		{
			while (size>0){
				off_t st_offset = offset;
				for (; size>0 && (pg_ofs(buffer+offset)!=0 || offset==st_offset);
						offset++, size--) {
					buffer[offset] = input_getc ();
				}
				if(size>0){
					buffer = (char *) (user_vtop (_buffer+offset) - offset);
				}
			}
		}
	else
		{
			lock_acquire (&filesys_lock);
			struct file *f = get_file_by_fd (fd);
			if (f==NULL) {
				lock_release (&filesys_lock);
				return -1;
			}

			while (size>0) {
				off_t read_now = file_read (f, buffer+offset, (off_t) MIN(size, remain));

				if (read_now==0) {
					lock_release (&filesys_lock);
					return  (int) offset;
				}

				offset += (int) read_now;
				size -= (unsigned) read_now;
				if(size>0) {
					buffer = (char *) (user_vtop (_buffer+offset) - offset);
					remain = PGSIZE-pg_ofs(buffer+offset);
					ASSERT (remain == PGSIZE);
				}
			}
			lock_release (&filesys_lock);
		}
  return  (int) offset;
}

static int
write (int fd, const void *_buffer, unsigned size)
{
	int wrote = 0;

	if ((void *)(_buffer+size-1) >= PHYS_BASE)
		exit (-1);

	char *buffer = (char *) malloc (PGSIZE+1);
	if (buffer==NULL)
		return -1;

	if (fd == STDOUT_FILENO)
		{
			lock_acquire (&filesys_lock);
			while (size>0){
				strlbond (buffer, _buffer+wrote, MIN(size+1, PGSIZE+1));

				putbuf (buffer, MIN(size, PGSIZE));
				wrote += MIN(size, PGSIZE);

				barrier ();
				size -= MIN(size, PGSIZE);
			}
			lock_release (&filesys_lock);
		}
	else
		{
			struct file *f = get_file_by_fd (fd);
			if (f==NULL) {
				free (buffer);
				return -1;
			} else if (f->deny_write) {
				free (buffer);
				return 0;
			}

			/*TODO - lock all and not use strlbond */
			while (size>0){
				strlbond (buffer, _buffer+wrote, MIN(size+1, PGSIZE+1));

				lock_acquire (&filesys_lock);
				off_t wrote_now = file_write (f, buffer, (off_t) MIN(size, PGSIZE));
				lock_release (&filesys_lock);

				barrier ();
				if (wrote_now==0)
					break;

				wrote += (int) wrote_now;
				size -= (unsigned) wrote_now;
			}
		}
	free (buffer);
  return wrote;
}

static void
seek (int fd, unsigned position) 
{
	lock_acquire (&filesys_lock);
	struct file *f = get_file_by_fd (fd);
	if (f==NULL) {
		lock_release (&filesys_lock);
		return;
	}

	file_seek (f, (off_t) position);
	lock_release (&filesys_lock);
}

static unsigned
tell (int fd) 
{
	lock_acquire (&filesys_lock);
	struct file *f = get_file_by_fd (fd);
	if (f==NULL) {
		lock_release (&filesys_lock);
		return -1;
	}
	unsigned ret = file_tell (f);
	lock_release (&filesys_lock);
	return ret;
}

static void
close (int fd)
{
	lock_acquire (&filesys_lock);
	struct openfile *of = get_openfile_by_fd (fd);
	if (of==NULL) {
		lock_release (&filesys_lock);
		return;
	}

	file_close (of->f);
	list_remove (&of->openelem);
	free (of);
	
	lock_release (&filesys_lock);
}

/* ----- til here, enough for project2 ----- */

static mapid_t
mmap (int fd UNUSED, void *_addr UNUSED)
{
	//TODO
  return 0;
}

static void
munmap (mapid_t mapid UNUSED)
{
  //TODO
}

/* ----- til here, enough for project3 ----- */

static bool
chdir (const char *_dir UNUSED)
{
	//TODO
  return false;
}

static bool
mkdir (const char *_dir UNUSED)
{
	//TODO
  return false;
}

static bool
readdir (int fd UNUSED, char name[READDIR_MAX_LEN + 1] UNUSED) 
{
	//TODO
  return false;
}

static bool
isdir (int fd UNUSED) 
{
	//TODO
  return false;
}

static int
inumber (int fd UNUSED) 
{
	//TODO
  return 0;
}
