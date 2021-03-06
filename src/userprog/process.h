#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "userprog/syscall.h"

struct process_info
  {
    tid_t tid;          /* tid of the process */
    tid_t parent_tid;   /* tid of parent process */
    int status_code;    /* set by exit () syscall */
    struct list_elem elem;
  };

struct process_wait_info
  {
    struct semaphore sema;
    tid_t waiting_for_tid;   /* tid of process the parent process is waiting for */
    struct list_elem elem;
  };

struct process_load_info
  {
    tid_t parent_tid;       /* tid of the parent process */
    bool loaded;            /* the load status of the process if it loaded successfully or not */
    struct semaphore sema;
    struct list_elem elem;
  };

/* for filesystem */
struct process_file
  {
    struct file *file;
    int fd;
    struct list_elem elem;
    struct dir* dir;
  };

void process_init (void);
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (int status);
void process_activate (void);

/* for filesystem */
struct file* process_get_file (int fd);
struct process_file* process_get_struct (int fd);
int process_add_file(struct file *f);
void process_close_file (int fd);
#endif /* userprog/process.h */
