#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "devices/block.h"
#include "filesys/directory.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"

/* lock for filesystem. */
struct lock filesys_lock;

static void syscall_handler (struct intr_frame *);
int (*syscall_table[SYSCALL_TOTAL]) (struct intr_frame *);

/* Checks the validity of the user vaddr to vaddr + (size - 1).
   Currently, size is hardcoded to 3 as we only check for int
   or unsigned variables here.
   Returns true if uaddr is not null, is not a pointer to kernel
   vitrual addr. space and is not a pointer to unmapped memory. */
static bool
is_uaddr_valid (void *uaddr)
{
  bool valid = true;
  int size = 3;

  int i;
  for (i = 0; i < size; i++)
    {
      if (((char *) uaddr + i == NULL) || ((is_user_vaddr ((char *) uaddr + i)) == false) ||
          (pagedir_get_page (thread_current()->pagedir, (char *) uaddr + i) == NULL))
        {
          valid = false;
          break;
        }
    }

  return valid;
}

/* Checks the validity of the user str. Returns true if the string
   is valid i.e. in user virtual memory. */
static bool
is_string_valid (const void *str)
{
  bool valid = true;
  bool null_term = false;

  while (true)
    {
      if (is_uaddr_valid (str) == false)
        {
          valid = false;
          break;
        }

      if (*((char *) str) == '\0')
        {
          null_term = true;
          break;
        }

      str = (char *) str + 1;
    }

  if (null_term == true)
    return valid;
  else
    return false;
}

void
syscall_init (void)
{
  lock_init (&filesys_lock);

  syscall_table[SYS_HALT] = _syscall_halt;
  syscall_table[SYS_EXIT] = _syscall_exit;
  syscall_table[SYS_EXEC] = _syscall_exec;
  syscall_table[SYS_WAIT] = _syscall_wait;
  syscall_table[SYS_CREATE] = _syscall_create;
  syscall_table[SYS_REMOVE] = _syscall_remove;
  syscall_table[SYS_OPEN] = _syscall_open;
  syscall_table[SYS_FILESIZE] = _syscall_filesize;
  syscall_table[SYS_READ] = _syscall_read;
  syscall_table[SYS_WRITE] = _syscall_write;
  syscall_table[SYS_SEEK] = _syscall_seek;
  syscall_table[SYS_TELL] = _syscall_tell;
  syscall_table[SYS_CLOSE] = _syscall_close;
  syscall_table[SYS_CHDIR] = _syscall_chdir;
  syscall_table[SYS_MKDIR] = _syscall_mkdir;
  syscall_table[SYS_READDIR] = _syscall_readdir;
  syscall_table[SYS_ISDIR] = _syscall_isdir;
  syscall_table[SYS_INUMBER] = _syscall_inumber;
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* simple calls syscall_halt */
int
_syscall_halt (struct intr_frame *f UNUSED)
{
  syscall_halt ();

  return 0;
}

/* validates user addresses and calls syscall_exit */
int
_syscall_exit (struct intr_frame *f)
{
  if (is_uaddr_valid (((int *)f->esp) + 1) == false)
    {
      syscall_exit (-1);
    }

  int status = *((int *)(f->esp) + 1);

  syscall_exit (status);

  return 0;
}

/* validates user addresses and calls syscall_exec */
int
_syscall_exec (struct intr_frame *f)
{
  const char *cmd_line;

  if ((is_uaddr_valid ((char *)f->esp + 4) == false) ||
      (is_string_valid (*((char **) ((char *)f->esp + 4))) == false))
    syscall_exit (-1);

  cmd_line = *((char **) ((char *)f->esp + 4));

  f->eax = syscall_exec (cmd_line);

  return 0;
}

/* validates user addresses and calls syscall_wait */
int
_syscall_wait (struct intr_frame *f)
{
  if (is_uaddr_valid ((int *)f->esp + 1) == false)
    syscall_exit (-1);

  int pid = *((int *)f->esp + 1);

  f->eax = syscall_wait (pid);

  return 0;
}

int
_syscall_isdir (struct intr_frame *f)
{
  if (is_uaddr_valid ((int *)f->esp + 1) == false)
    syscall_exit (-1);

  int fd = *((int *)f->esp + 1);

  f->eax = syscall_isdir (fd);

  return 0;
}

int
_syscall_inumber (struct intr_frame *f)
{
  if (is_uaddr_valid ((int *)f->esp + 1) == false)
    syscall_exit (-1);

  int fd = *((int *)f->esp + 1);

  f->eax = syscall_inumber (fd);

  return 0;
}

/* validates user addresses and calls syscall_create */
int
_syscall_create (struct intr_frame *f)
{
  const char *file;
  unsigned initial_size;

  if ((is_uaddr_valid ((char *)f->esp + 4) == false) ||
      (is_string_valid (*((char **) ((char *)f->esp + 4))) == false) ||
      (is_uaddr_valid ((unsigned *)f->esp + 2) == false))
    syscall_exit (-1);

  file = *((char **) ((char *)f->esp + 4));
  initial_size = *((unsigned *)f->esp + 2);

  f->eax = syscall_create (file, initial_size);

  return 0;
}

/* validates user addresses and calls syscall_remove */
int
_syscall_remove (struct intr_frame *f)
{
  const char *file;

  if ((is_uaddr_valid ((char *)f->esp + 4) == false) ||
      (is_string_valid (*((char **) ((char *)f->esp + 4))) == false))
    syscall_exit (-1);

  file = *((char **) ((char *)f->esp + 4));

  f->eax = syscall_remove (file);

  return 0;
}

/* validates user addresses and calls syscall_open */
int
_syscall_open (struct intr_frame *f)
{
  const char *file;

  if ((is_uaddr_valid ((char *)f->esp + 4) == false) ||
      (is_string_valid (*((char **) ((char *)f->esp + 4))) == false))
    syscall_exit (-1);

  file = *((char **) ((char *)f->esp + 4));

  f->eax = syscall_open (file);

  return 0;
}

/* validates user addresses and calls syscall_filesize */
int
_syscall_filesize (struct intr_frame *f)
{
  int fd;

  if (is_uaddr_valid ((int *)f->esp + 1) == false)
    syscall_exit (-1);

  fd = *((int *)f->esp + 1);

  f->eax = syscall_filesize (fd);

  return 0;
}

/* validates user addresses and calls syscall_read */
int
_syscall_read (struct intr_frame *f)
{
  int fd;
  char *buffer;
  unsigned size;

  if ((is_uaddr_valid ((int *)f->esp + 1) == false) ||
      (is_uaddr_valid ((char *)f->esp + 8) == false) ||
      (is_string_valid (*((char **) ((char *)f->esp + 8))) == false) ||
      (is_uaddr_valid ((unsigned *)f->esp + 3) == false))
    syscall_exit (-1);

  fd = *((int *)f->esp + 1);
  buffer = *((char **) ((char *)f->esp + 8));
  size = *((unsigned *)f->esp + 3);

  f->eax = syscall_read (fd, buffer, size);

  return 0;
}

/* validates user addresses and calls syscall_write */
int
_syscall_write (struct intr_frame *f)
{
  int fd;
  char* buffer;
  unsigned size;

  if ((is_uaddr_valid ((int *)f->esp + 1) == false) ||
      (is_uaddr_valid ((char *)f->esp + 8) == false) ||
      (is_string_valid (*((char **) ((char *)f->esp + 8))) == false) ||
      (is_uaddr_valid ((unsigned *)f->esp + 3) == false))
    syscall_exit (-1);

  fd = *((int *)f->esp + 1);
  buffer = *((char **) ((char *)f->esp + 8));
  size = *((unsigned *)f->esp + 3);

  f->eax = syscall_write (fd, buffer, size);

  return 0;
}

/* validates user addresses and calls syscall_seek */
int
_syscall_seek (struct intr_frame *f)
{
  int fd;
  unsigned position;

  if ((is_uaddr_valid ((int *)f->esp + 1) == false) ||
      (is_uaddr_valid ((unsigned *)f->esp + 2) == false))
    syscall_exit (-1);

  fd = *((int *)f->esp + 1);
  position = *((unsigned *)f->esp + 2);

  syscall_seek (fd, position);

  return 0;
}

/* validates user addresses and calls syscall_tell */
int
_syscall_tell (struct intr_frame *f)
{
  int fd;

  if (is_uaddr_valid ((int *)f->esp + 1) == false)
    syscall_exit (-1);

  fd = *((int *)f->esp + 1);

  f->eax = syscall_tell (fd);

  return 0;
}

/* validates user addresses and calls syscall_close */
int
_syscall_close(struct intr_frame *f)
{
  int fd;

  if (is_uaddr_valid ((int *)f->esp + 1) == false)
    syscall_exit (-1);

  fd = *((int *)f->esp + 1);

  syscall_close (fd);

  return 0;
}


int
_syscall_mkdir(struct intr_frame *f)
{
  char *filename;

  if ((is_uaddr_valid ((char *)f->esp + 4) == false) ||
        (is_string_valid (*((char **) ((char *)f->esp + 4))) == false))
      syscall_exit (-1);

    filename = *((char **) ((char *)f->esp + 4));

    f->eax = syscall_mkdir (filename);


  return 0;
}


int
_syscall_chdir(struct intr_frame *f)
{
  char *name;

  if ((is_uaddr_valid ((char *)f->esp + 4) == false) ||
      (is_string_valid (*((char **) ((char *)f->esp + 4))) == false))
    syscall_exit (-1);

    name = *((char **) ((char *)f->esp + 4));

    f->eax = syscall_chdir (name);


  return 0;
}

int
_syscall_readdir (struct intr_frame *f)
{
  int fd;
  char *name;

  if ((is_uaddr_valid ((int *)f->esp + 1) == false))
    syscall_exit (-1);

  fd = *((int *)f->esp + 1);

  if ((is_uaddr_valid ((char *)f->esp + 8) == false) ||
        (is_string_valid (*((char **) ((char *)f->esp + 8))) == false))
      syscall_exit (-1);

  name = *((char **) ((char *)f->esp + 8));

  f->eax = syscall_readdir (fd, name);

  return 0;
}


void
syscall_halt(void)
{
  shutdown_power_off ();
}

void
syscall_exit (int status)
{
  printf ("%s: exit(%d)\n", thread_current ()->name, status);
  thread_exit (status);
}

pid_t
syscall_exec(const char* cmd_line)
{
  return process_execute (cmd_line);
}

int
syscall_wait(pid_t pid)
{
  return process_wait (pid);
}

bool
syscall_create(const char* file, unsigned initial_size)
{
  bool success=false;
  lock_acquire(&filesys_lock);
  success = filesys_create(file,initial_size);
  lock_release(&filesys_lock);
  return success;
}

bool
syscall_remove(const char* file)
{
  bool success = false;
  lock_acquire(&filesys_lock);
  success = filesys_remove(file);
  lock_release(&filesys_lock);
  return success;
}

int
syscall_open(const char* file)
{
  lock_acquire(&filesys_lock);
    struct file *f = filesys_open(file);
    if (!f)
      {
        lock_release(&filesys_lock);
        return -1;
      }
    int fd = process_add_file(f);
    lock_release(&filesys_lock);
    return fd;
}

int
syscall_filesize(int fd)
{
  lock_acquire(&filesys_lock);
  struct file *f = process_get_file(fd);

  if(!f)
    {
      lock_release(&filesys_lock);
      return -1;
    }
  int size = file_length(f);
  lock_release(&filesys_lock);
  return size;
}

int
syscall_read (int fd, void *buffer, unsigned size)
{
  if(fd == STDIN_FILENO)
    {
      unsigned i;
      uint8_t* temp_buffer = (uint8_t*)buffer;
      for(i=0;i<size;i++)
        {
          temp_buffer[i] = input_getc();
        }
      return size;
    }
  else
    {
      lock_acquire(&filesys_lock);
      struct file *f = process_get_file(fd);
      if(!f)
        {
          lock_release(&filesys_lock);
          return -1;
        }

      int bytes = file_read(f, buffer, size);
      lock_release(&filesys_lock);
      return bytes;
    }

}

int
syscall_write(int fd, const void *buffer,unsigned size)
{
  int status = -1;
  /* for console write */
  if(fd == STDOUT_FILENO){
      putbuf((char *)buffer, (size_t)size);
      status = (int)size;
  }else{
      /* write to filesystem */
      lock_acquire(&filesys_lock);
         struct file *f = process_get_file(fd);
         if (!f)
           {
             /* error because file was null */
             lock_release(&filesys_lock);
             return -1;
           }
         int bytes = file_write(f, buffer, size);
         lock_release(&filesys_lock);
    return bytes;
  }

return status;
}

void
syscall_seek(int fd,unsigned position)
{

  lock_acquire(&filesys_lock);
  struct file *f = process_get_file(fd);
  if(!f)
    {
      lock_release(&filesys_lock);
    }
  else{
      file_seek(f,position);
      lock_release(&filesys_lock);
  }

}

unsigned
syscall_tell(int fd)
{
  lock_acquire(&filesys_lock);
  struct file *f = process_get_file(fd);

  if(!f)
    {
      lock_release(&filesys_lock);
      return -1;
    }

  off_t offset = file_tell(f);
  lock_release(&filesys_lock);
  return offset;
}

void
syscall_close(int fd)
{
  lock_acquire(&filesys_lock);
  process_close_file(fd);
  lock_release(&filesys_lock);
}

bool
syscall_mkdir(char* path)
{
  bool success = false;
  lock_acquire(&filesys_lock);
  block_sector_t sector;

  if(free_map_allocate (1, &sector))
    success = dir_create(sector,1,path);

  lock_release(&filesys_lock);
  return success;
}

bool
syscall_chdir(char* name)
{
  bool success = false;
  lock_acquire(&filesys_lock);
  success = dir_chdir (name);
  lock_release(&filesys_lock);
  return success;
}

bool
syscall_readdir(int fd, char* name)
{

  bool success = false;
  struct process_file *f_desc;

  lock_acquire(&filesys_lock);
  f_desc = process_get_struct(fd);

  if(f_desc==NULL) goto done;

  struct inode *inode;
   inode = file_get_inode(f_desc->file); // file descriptor -> inode
    if(inode == NULL) goto done;

      // check whether it is a valid directory
    if(! inode_is_dir(inode)) goto done;

    ASSERT (f_desc->dir != NULL); // see sys_open() -> add_process_file
    success = dir_readdir (f_desc->dir, name);

  done:
  lock_release (&filesys_lock);
  return success;
}

bool
syscall_isdir(int fd)
{
	bool success = false;

	lock_acquire(&filesys_lock);
	 struct process_file* file_d = process_get_struct(fd);
	 success = inode_is_dir (file_get_inode(file_d->file));
	 lock_release(&filesys_lock);
	return success;
}

int
syscall_inumber(int fd)
{
	int success=-1;
	lock_acquire(&filesys_lock);
	struct process_file* file_d = process_get_struct(fd);
	success = (int) inode_get_inumber (file_get_inode(file_d->file));
	lock_release(&filesys_lock);
	return success;
}


static void
syscall_handler (struct intr_frame *f)
{
  if (is_uaddr_valid (f->esp) == false)
    {
      syscall_exit (-1);
    }

  /* get the syscall no. from the stack. */
  int syscall_no = *((int *)(f->esp));

  /* if syscall no. is greater than total syscalls, there is a problem */
  if (syscall_no >= SYSCALL_TOTAL)
    PANIC ("SYSCALL NUMBER exceeds total syscalls");

  /* invoke syscall */
  syscall_table[syscall_no] (f);
}
