#include "filesys/cache.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "filesys/inode.h"
#include <debug.h>
#include "devices/disk.h"
#include "devices/timer.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include <list.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#define CACHE_MAX_SECTOR_NO 64
#define frequency_of_writeback 500

static struct list cache_list;
static struct lock cache_lock;

static void thread_function_wb_frequently(void);


void cache_init(void){
				list_init(&cache_list);
				lock_init(&cache_lock);
					
		};


void cache_thread_init(void){
				/*make thread for frequently write behind*/
			//	thread_create("wb_frequently",PRI_DEFAULT,thread_function_wb_frequently,NULL);
}

/*make new cache entry.*/
struct cache* make_cache(disk_sector_t sec_no, bool dirty){
				struct cache * new_cache = NULL;
//				printf("[c]Make new cache entry.\n");	
				size_t cachelist_size = list_size(&cache_list);
				/*Cache full case*/
				if(cachelist_size >= CACHE_MAX_SECTOR_NO){
	//							printf("[c]Evict!\n");
								bool evict_success = evict_cache();
								ASSERT(evict_success);
				}
				new_cache = (struct cache *) malloc(sizeof(struct cache));

				new_cache->sector_idx = sec_no;
				new_cache->dirty = dirty;
				new_cache->open_cnt = 0;
				list_push_back(&cache_list,&new_cache->el);

				return new_cache;
}


/*Search cache_list, and get cache having sec_no.
 If not exist, return NULL*/
struct cache* find_cache(disk_sector_t sec_no){
				struct cache* tmp_cache = NULL;
				struct list_elem* elem;
				
				//if(list_size(&cache_list)>=2){
				for(elem = list_begin(&cache_list) ; elem != list_end(&cache_list) ; elem = list_next(elem)){
								tmp_cache = list_entry(elem, struct cache, el);
								if (tmp_cache->sector_idx == sec_no){
												/*pop finding cache and re_insert it, doto LRU alogoritm for eviction */
												list_remove(elem);
												list_push_back(&cache_list, &tmp_cache->el);
												return tmp_cache;
								}
				}
				//}
				return NULL;
}

/*.If cache is dirty, write it into disk.*/
bool write_behind(struct cache* wb_cache){
				
				if(wb_cache == NULL){
								//printf("That cache is not exist!\n");
								return false;
				}
				if(wb_cache->dirty == true){
								disk_write(filesys_disk,wb_cache->sector_idx,wb_cache->buffer);
								wb_cache->dirty = false;
				}
				return true;
}

/*evict one cache by LRU algorithm, and return new cache structure. */
bool evict_cache(void){

				struct cache * evicted_cache = NULL;

				struct list_elem * elem = NULL;

				bool check = false;

				for (elem =list_begin(&cache_list); elem != list_end(&cache_list) ; elem = list_next(elem)){
								evicted_cache = list_entry(elem, struct cache, el);								
								if(evicted_cache->open_cnt == 0){
												check = true;
												break;
								}
				}

				bool wb_check = write_behind(evicted_cache);

				ASSERT(wb_check);

				list_remove(elem);

				free(evicted_cache);

				ASSERT(check);
				return check;
}




/*Read sec_no sector(by cache) and write it into read_buffer.*/
void cache_read(disk_sector_t sec_no, void *read_buffer, int sector_offset, int size){
				struct cache * read_cache = NULL;
				//printf("[c]cache read\n");
				cache_lock_acquire();

				read_cache = find_cache(sec_no);

				ASSERT(read_buffer != NULL);
				if(read_cache == NULL){
								read_cache = make_cache(sec_no,false);
								disk_read(filesys_disk,sec_no,read_cache->buffer);
				}
				read_cache->open_cnt++;
				memcpy(read_buffer,(uint8_t *)&read_cache->buffer + sector_offset, size);
				cache_lock_release();

}
/*Read write_buffer and write it into sector (into cache).*/
void cache_write(disk_sector_t sec_no, void* write_buffer, int sector_offset, int size){
				//printf("[c]cache write\n");
				struct cache* write_cache = NULL;
				cache_lock_acquire();
				write_cache = find_cache(sec_no);

				if(write_cache == NULL){
								write_cache = make_cache(sec_no, true);

								if(sector_offset>0 || size<DISK_SECTOR_SIZE){
												disk_read(filesys_disk,sec_no,write_cache->buffer);
								}
				}
				write_cache->dirty = true;
				memcpy((uint8_t*)&write_cache->buffer + sector_offset, write_buffer, size);
				write_cache->open_cnt++;
				cache_lock_release();

}

/*write behind all dirty cache in evry 500 ticks*/
static void thread_function_wb_frequently(void){
				struct cache* tmp_cache =NULL;
				struct list_elem * elem;
				
				while(true){
								timer_sleep(500);
								
								/*Traverse cache list and write behind the dirty cache*/
								//cache_lock_acquire();
								for(elem = list_begin(&cache_list) ; elem != list_end(&cache_list) ; elem = list_next(elem)){
										tmp_cache = list_entry(elem, struct cache, el);
										bool check = write_behind(tmp_cache);
								}
				}			
								//cache_lock_release();
				
}



void destroy_cache_list(void){

				struct list_elem * elem;
				struct cache * removed;

				cache_lock_acquire();
				for(elem = list_begin(&cache_list) ; elem != list_end(&cache_list) ; elem = list_next(elem)){
								removed = list_entry(elem, struct cache, el);
								bool wb_result = write_behind(removed);
								ASSERT(wb_result);
								list_remove(elem);
								//free(removed);
				}
				cache_lock_release();

}

void cache_lock_acquire(void){
				lock_acquire(&cache_lock);
}

void cache_lock_release(void){
				lock_release(&cache_lock);
}
