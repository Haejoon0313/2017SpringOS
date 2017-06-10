#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <list.h>
#include "threads/synch.h"
#include "devices/disk.h"
#include <stdint.h>
#include <stdbool.h>
#define CACHE_MAX_SECTOR_NO 64
//#define frequency_of_writeback 500

//static struct list cache_list;
struct cache{
				uint8_t buffer[DISK_SECTOR_SIZE]; /*data of cache */
				disk_sector_t sector_idx;					/*Sector No. */
				bool dirty;												/*is dirty cache? */
				int open_cnt;											/*#of open count*/
//				bool loaded;											/*is this block used?(uploaded?)*/
				struct lock lock;									/*lock for each cache entry*/
				struct list_elem el;							/*list element */	
				
};

struct ahead_entry{
				disk_sector_t sector_idx;
				struct list_elem el;							
};


void cache_empty(void);


void cache_init(void);

struct cache* make_cache(disk_sector_t sec_no, bool dirty);

struct cache* find_cache(disk_sector_t sec_no);

bool write_behind(struct cache* cache);/*if cache is dirty, write it back to disk */

struct cache* find_cache(disk_sector_t sec_no);/*search and return cache*/

bool evict_cache(void);/*evict cache */

//void thread_function_wb_frequently(void);/*frequntly refresh the whole cache*/

void destroy_cache_list(void);											/*remove the whole cache list*/

void cache_read(disk_sector_t sec_no, void * read_buffer, int sector_offset, int size);

void cache_write(disk_sector_t sec_no, void *read_buffer, int  sector_offset, int size);

void cache_read_ahead(disk_sector_t sec);

void read_ahead_manager(void);



void all_cache_write_behind(void);

void cache_thread_init(void);
void cache_lock_acquire(void);
void cache_lock_release(void);
#endif
