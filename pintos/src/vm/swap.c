#include "vm/swap.h"
#include <debug.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.c>
#include <round.h>
#include <string.h>
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <bitmap.h>
#include "devices/disk.h"
#include "userprog/exception.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"

/*data struct for swap disk */
static struct bitmap * swap_table;
static struct lock swap_lock;

void
swap_init(){
				
		lock_init(&swap_lock);
		struct disk *swap_disk = disk_get(1,1);
		swap_table = bitmap_create(disk_size(swap_disk) * 512 / 4096);//disk_size() may return the # of sector.
		
		ASSERT(!swap_table);

}


/* When memory is full, Find victim page and Write it into disk
	 by call the function swap out(kpage).
	 victim page is pointed by KPAGE.
	 Evict page is up to frame_evict()'s policy(FIFO, LRU etc...)
	 swap_out() is called by frame_evict(). And frame_evict() 
	 is called by frame_alloc() when palloc_get_page() failed.
	 */
uint32_t swap_out(void * kpage){
		struct disk *swap_disk = disk_get(1,1);
		disk_sector_t sector_cnt = NULL;//to count sector for 1 PAGE.

		lock_acquire(&swap_lock);
		uint32_t swap_idx = bitmap_scan_and_filp(swap_table,0,1,false);
		
		if (swap_idx == BITMAP_ERROR){
						printf("DISK FULLED \n");
						exit(-1);
		}

		for (sector_cnt = 0; sector_cnt <PGSIZE / DISK_SECTOR_SIZE ; sector_cnt++){
						disk_write(swap_disk, swap_idx* PGSIZE /DISK_SECTOR_SIZE + sector_cnt, kpage+ sector_cnt*DISK_SECTOR_SIZE);

		}
		lock_release(&swap_lock);
		return swap_idx //return the index of swap_table
}

/*Read disk and swap in the frame in swap disk.  */
void swap_in (struct sup_pte * spte, void * kpage){
		uint32_t swap_idx;
		disk_sector_t sector_cnt;
		struct disk * swap_disk = disk_get(1,1);

		lock_acquire(&swap_lock);
		swap_idx = spte->swap_index;		
	
		swap_set(swap_idx, false);

		for(sector_cnt =0 ; sector_cnt < PGSIZE/DISK_SECTOR_SIZE ; sector_cnt++){
				disk_read(swap_disk, (swap_idx*PGSIZE/DISK_SECTOR_SIZE) + sector_cnt , kpage);
		}
		lock_release(&swap_lock);

}

/*ASSERT that current swap_idx is !value. Then, flip it into VALUE boolean. */
void swap_set(uint32_t swap_idx, bool value){
		ASSERT(bitmap_test(swap_table, !value));

		lock_acquire(&swap_lock);
		bitmap_set(swap_table, swap_idx, value);
		lock_release(&swap_lock);

}




