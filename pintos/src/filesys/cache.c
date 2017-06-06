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
static struct list read_ahead_list;
static struct condition read_ahead_condition;
static struct lock read_ahead_lock;

static void thread_function_wb_frequently(void);

void cache_block_lock_acquire(struct cache* lock_cache);

void cache_block_lock_release(struct cache* lock_cache);

void cache_init(void){
				list_init(&cache_list);
				lock_init(&cache_lock);
				list_init(&read_ahead_list);
				cond_init(&read_ahead_condition);
				lock_init(&read_ahead_lock);
		};


void cache_thread_init(void){
				/*make thread for frequently write behind*/
			thread_create("thread_function_wb_frequently",0,(thread_func* ) thread_function_wb_frequently,NULL);
			
		//	thread_create("thread_function_read_ahead",0,(thread_func *) read_ahead_manager,NULL);
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
				lock_init(&new_cache->lock);
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
												cache_block_lock_acquire(tmp_cache);
												list_remove(elem);
												list_push_back(&cache_list, &tmp_cache->el);
												cache_block_lock_release(tmp_cache);
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
												cache_block_lock_acquire(evicted_cache);
												check = true;
												break;
								}
				}

				if(!check){
					return check;
				}

				bool wb_check = write_behind(evicted_cache);

				ASSERT(wb_check);

				list_remove(elem);
				
				cache_block_lock_release(evicted_cache);
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

//				printf("buffer address to cache write : %x \n",read_buffer);
				ASSERT(read_buffer != NULL);
				if(read_cache == NULL){
								read_cache = make_cache(sec_no,false);
								cache_block_lock_acquire(read_cache);
								disk_read(filesys_disk,sec_no,read_cache->buffer);
								cache_block_lock_release(read_cache);
				}

				cache_block_lock_acquire(read_cache);
				read_cache->open_cnt++;
				ASSERT(read_buffer!=NULL);
				ASSERT(read_cache !=NULL);
				memcpy(read_buffer,(uint8_t *)&read_cache->buffer + sector_offset, size);
				cache_block_lock_release(read_cache);
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
												cache_block_lock_acquire(write_cache);
												disk_read(filesys_disk,sec_no,write_cache->buffer);
												cache_block_lock_release(write_cache);
								}
				}
				cache_block_lock_acquire(write_cache);
				write_cache->dirty = true;
				memcpy((uint8_t*)&write_cache->buffer + sector_offset, write_buffer, size);
				cache_block_lock_release(write_cache);
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
								cache_lock_acquire();
								for(elem = list_begin(&cache_list) ; elem != list_end(&cache_list) ; elem = list_next(elem)){
										tmp_cache = list_entry(elem, struct cache, el);
										bool check = write_behind(tmp_cache);
										ASSERT(check);
								}
				}			
								cache_lock_release();
				
}



void destroy_cache_list(void){

				struct list_elem * elem;
				struct cache * removed;

				cache_lock_acquire();
				while(!list_empty(&cache_list)){//for(elem = list_begin(&cache_list) ; elem != list_end(&cache_list) ; elem = list_next(elem)){
								elem = list_begin(&cache_list);

								removed = list_entry(elem, struct cache, el);
								cache_block_lock_acquire(removed);
								bool wb_result = write_behind(removed);
								ASSERT(wb_result);
								cache_block_lock_release(removed);
								list_remove(elem);
								free(removed);
				}
				cache_lock_release();

}


void cache_read_ahead(disk_sector_t ahead_sec){
	struct cache * ahead_cache = NULL;
	struct ahead_entry * new_rae = NULL;
	ahead_cache = find_cache(ahead_sec);

	/*next sector is already in cache system. return*/
	if(ahead_cache !=NULL){
					return;
	}

	lock_acquire(&read_ahead_lock);

	new_rae = calloc(1, sizeof * new_rae);

	list_push_back(&read_ahead_list, &new_rae->el);
	
	cond_signal(&read_ahead_condition, &read_ahead_lock);

	lock_release(&read_ahead_lock);

}


void read_ahead_manager(){
	struct cache * read_ahead_cache = NULL;
	struct ahead_entry * new_rae = NULL;
	struct list_elem * elem;
	disk_sector_t ra_sec;
	while(1)
	{
		lock_acquire(&read_ahead_lock);

		cond_wait(&read_ahead_condition, &read_ahead_lock);
		
		elem = list_begin(&read_ahead_list);

		new_rae = list_entry(elem, struct ahead_entry, el);

		ra_sec = new_rae->sector_idx;

		read_ahead_cache = find_cache(ra_sec);
		
		list_remove(elem);
		/*next sector is already in cache*/
		if(read_ahead_cache != NULL)
		{
			free(new_rae);
			return;	
		}
		cache_lock_acquire();

		read_ahead_cache = make_cache(ra_sec, false);

		disk_read(filesys_disk, ra_sec, read_ahead_cache->buffer);
		
		cache_lock_release();
		read_ahead_cache->open_cnt = 1;

		free(new_rae);
		return;
	}
	
}




void cache_lock_acquire(void){
				lock_acquire(&cache_lock);
}

void cache_lock_release(void){
				lock_release(&cache_lock);
}


void cache_block_lock_acquire(struct cache* lock_cache){
	ASSERT(lock_cache != NULL);
				//lock_acquire(&lock_cache->lock);
}


void cache_block_lock_release(struct cache* lock_cache){
	ASSERT(lock_cache !=NULL);
				//lock_release(&lock_cache->lock);
}
