#include "filesys/cache.h"
#include <stdio.h>
#include "devices/block.h"
#include <string.h>
#include <bitmap.h>
#include <list.h>
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "devices/timer.h"

static struct bitmap *cache_free_map; /* true: sector is free. false: sector is taken */
static struct list list_buffer_cache;
static struct lock cache_lock;

extern struct block *fs_device;

static struct buffer_cache_entry * buffer_cache_find_sector (block_sector_t );
static void thread_entry_read_ahead (void *);
static void thread_entry_write_back (void *);

/* initializes buffer cache related structures */
void
buffer_cache_init (void)
{
  list_init (&list_buffer_cache);
  lock_init (&cache_lock);

  /* initialize cache_free_map and mark all entries as true i.e. free */
  cache_free_map = bitmap_create (BUFFER_CACHE_SIZE);
  if (cache_free_map == NULL)
    PANIC ("cache_free_map creation failed");

  bitmap_set_all (cache_free_map, true);

  (void) thread_create ("buff_cache_wb", 0, thread_entry_write_back, NULL);
}

/* writes to buffer cache. if buffer cache is full, evicts a sector */
void
buffer_cache_write (void *buffer, block_sector_t sector)
{
  lock_acquire (&cache_lock);

  /* first check if the sector is already present */
  struct buffer_cache_entry *entry_present = buffer_cache_find_sector (sector);
  if (entry_present != NULL)
    {
      /* sector entry present in cache. update the data */
      memcpy (entry_present->data, buffer, BLOCK_SECTOR_SIZE);

      entry_present->accessed = true;
      entry_present->dirty = true;
    }
  else
    {
      /* add a new entry in cache */
      struct buffer_cache_entry *entry = (struct buffer_cache_entry *) malloc (sizeof (struct buffer_cache_entry));

      if (entry != NULL)
        {
          /* copy buffer into cache entry */
          memcpy (entry->data, buffer, BLOCK_SECTOR_SIZE);

          /* get the cache entry no or evict if full */
          size_t index = bitmap_scan_and_flip (cache_free_map, 0, 1, true);

          /* need to evict */
          if (index == BITMAP_ERROR)
            {
              entry->cache_entry_no = buffer_cache_evict ();
            }
          /* eviction not required */
          else
            {
              entry->cache_entry_no = index;
            }

          /* set the sector no */
          entry->sector_no = sector;

          entry->ok_to_evict = false;
          entry->accessed = false;
          entry->dirty = false;

          /* insert into cache list */
          if (list_size (&list_buffer_cache) >= BUFFER_CACHE_SIZE)
            {
              /* something went wrong */
              PANIC ("buffer cache overflow");
            }
          else
            {
              list_push_back (&list_buffer_cache, &entry->elem);
              entry->ok_to_evict = true;
            }
        }
    }

  lock_release (&cache_lock);
}

/* first checks if the block at sector is present in cache. if preset, reads the data into buffer
   and returns true. otherwise returns false */
bool
buffer_cache_read (void *buffer, block_sector_t sector)
{
  lock_acquire (&cache_lock);

  bool found = false;

  struct buffer_cache_entry *entry = buffer_cache_find_sector (sector);

  if (entry != NULL)
    {
      memcpy (buffer, entry->data, BLOCK_SECTOR_SIZE);
      entry->accessed = true;
      found = true;
    }

  lock_release (&cache_lock);

  /* read ahead */
  spawn_thread_read_ahead (sector);

  return found;
}

/* evicts a sector from the buffer cache and returns its entry no */
size_t
buffer_cache_evict (void)
{
  /* clock algo for eviction */
  struct list_elem *e;
  bool candidate_found = false;
  struct buffer_cache_entry *candidate = NULL;
  size_t ret_val;

  while (true)
    {
      for (e = list_begin (&list_buffer_cache); e != list_end (&list_buffer_cache);
          e = list_next (e))
        {
          struct buffer_cache_entry *entry = list_entry (e, struct buffer_cache_entry, elem);

          if (!entry->ok_to_evict)
            continue;

          /* if !accessed, evict */
          if (!entry->accessed)
            {
              /* if dirty, write back first */
              if (entry->dirty)
                {
                  block_write (fs_device, entry->sector_no, entry->data);
                  entry->dirty = false;
                }

              candidate = entry;
              candidate_found = true;
              break;
            }
          else
            entry->accessed = false;
        }

      if (candidate_found)
        {
          /* remove from cache list */
          list_remove (&candidate->elem);

          /* mark the entry as free in cache_free_map */
          bitmap_set (cache_free_map, candidate->cache_entry_no, true);

          ret_val = candidate->cache_entry_no;

          free (candidate);
          break;
        }
    }

  return ret_val;
}

/* finds a sector in cache and returns buffer cache entry. returns NULL if not found */
static struct buffer_cache_entry *
buffer_cache_find_sector (block_sector_t sector)
{
  struct buffer_cache_entry *buff_cache_entry = NULL;
  struct list_elem *e;

  for (e = list_begin (&list_buffer_cache); e != list_end (&list_buffer_cache);
      e = list_next (e))
    {
      struct buffer_cache_entry *entry = list_entry (e, struct buffer_cache_entry, elem);

      if (entry->sector_no == sector)
        {
          buff_cache_entry = entry;
          break;
        }
    }

  return buff_cache_entry;
}

void
spawn_thread_read_ahead (block_sector_t sector)
{
  block_sector_t *arg = (block_sector_t *) malloc (sizeof (block_sector_t));

  if (arg != NULL)
    {
      *arg = sector + 1;
      (void) thread_create ("buff_cache_ra", 0, thread_entry_read_ahead, arg);
    }
}

static void
thread_entry_read_ahead (void *arg)
{
  lock_acquire (&cache_lock);

  block_sector_t sector =  *(block_sector_t *) arg;
  struct buffer_cache_entry *entry = buffer_cache_find_sector (sector);

  /* if entry is not NULL, sector is already present in cache. otherwise bring it in cache */
  if (entry == NULL)
    {
      struct buffer_cache_entry *new_entry = (struct buffer_cache_entry *) malloc (sizeof (struct buffer_cache_entry));

      new_entry->ok_to_evict = false;
      new_entry->sector_no = sector;

      /* get the cache entry no or evict if full */
      size_t index = bitmap_scan_and_flip (cache_free_map, 0, 1, true);

      /* need to evict */
      if (index == BITMAP_ERROR)
        {
          new_entry->cache_entry_no = buffer_cache_evict ();
        }
      /* eviction not required */
      else
        {
          new_entry->cache_entry_no = index;
        }

      block_read (fs_device, sector, new_entry->data);

      new_entry->accessed = false;
      new_entry->dirty = false;

      list_push_back (&list_buffer_cache, &new_entry->elem);
      new_entry->ok_to_evict = true;
    }

  lock_release (&cache_lock);

  free (arg);
}

/* wrapper function for writing back dirty sectors to disk */
void
buffer_cache_write_back (void)
{
  struct list_elem *e;

  for (e = list_begin (&list_buffer_cache); e != list_end (&list_buffer_cache);
      e = list_next (e))
    {
      struct buffer_cache_entry *entry = list_entry (e, struct buffer_cache_entry, elem);

      if (entry->dirty)
        {
          /* write back to disk */
          block_write (fs_device, entry->sector_no, entry->data);

          entry->dirty = false;
        }
    }
}

/* periodically writes all the dirty sectors back to disk and goes to sleep */
static void
thread_entry_write_back (void *arg UNUSED)
{
  while (true)
    {
      buffer_cache_write_back ();

      /* sleep */
      timer_sleep (60 * TIMER_FREQ);
    }
}
