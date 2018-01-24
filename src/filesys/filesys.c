#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "threads/malloc.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  buffer_cache_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();

  /* write back dirty data from cache */
  buffer_cache_write_back ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *path, off_t initial_size)
{
  block_sector_t inode_sector = 0;
  //struct dir *dir = dir_open_root ();
  char *filename = get_filename (path);
  struct dir *dir = dir_get_leaf (path);
  block_sector_t parent = inode_get_inumber (dir_get_inode (dir));

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, false, parent)
                  && dir_add (dir, filename, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  free (filename);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *path)
{
  //struct dir *dir = dir_open_root ();
  char *filename = get_filename (path);
  struct dir *dir = dir_get_leaf (path);
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, filename, &inode);
  dir_close (dir);

  free (filename);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  // TODO
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16, "/"))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

/* returns filename from path */
char *
get_filename (const char *path)
{
  char s[strlen (path) + 1];
  char *token, *save_ptr, *prev = "";

  memcpy(s, path, strlen (path) + 1);

  if (path[strlen (path) - 1] == '/')
     return NULL;

  for (token = strtok_r (s, " ", &save_ptr); token != NULL;
      token = strtok_r (NULL, " ", &save_ptr))
    prev = token;

  char *ret_val = (char *) malloc ((sizeof (char)) * (strlen (prev) + 1));
  memcpy (ret_val, prev, strlen (prev) + 1);

  return ret_val;
}
