#ifndef CACHE_FILESYS_H
#define CACHE_FILESYS_H

#include <stdio.h>
#include "devices/block.h"
#include <list.h>

#define BUFFER_CACHE_SIZE   64 /* in terms of sectors */

struct buffer_cache_entry
  {
    struct list_elem elem;
    bool ok_to_evict;
    char data[BLOCK_SECTOR_SIZE];
    block_sector_t sector_no;         /* sector no which this cache entry corresponds to */
    size_t cache_entry_no;            /* entry no in cache block */
    bool accessed;                    /* accessed bit */
    bool dirty;                       /* dirty bit */
  };

void buffer_cache_init (void);
void buffer_cache_write (block_sector_t , const void *, int , int , bool );
void buffer_cache_read (block_sector_t , void *, int , int );
size_t buffer_cache_evict (void);
void spawn_thread_read_ahead (block_sector_t );
void buffer_cache_write_back (void);

#endif /* CACHE_FILESYS_H */
