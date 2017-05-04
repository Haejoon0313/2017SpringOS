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


void
swap_init(){
		static struct lock swap_lock;
				
		lock_init(&swap_lock);
		struct disk *swap_disk = disk_get(1,1);//to make&calculate swap_table bitmap.
		static struct bitmap * swap_table = bitmap_create(disk_size(swap_disk) * 512 / 4096);//disk_size() may return the # of sector.
		
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
		struct disk *new_disk = disk_get(1,1);
		disk_sector_t sector_cnt = NULL;//to count sector for 1 PAGE.

		lock_acquire(&swap_disk);
		uint32_t swap_idx = bitmap_scan_and_filp(swap_table,0,1,false);
		
		if (swap_index == BITMAP_ERROR){
						printf("DISK FULLED \n");
						exit(-1);
		}

		for (sector_cnt = 0; sector_cnt <4096/512 ; sector_cnt++){
						disk_write(new_disk, swap_idx*4096/512 + sector_cnt, kpage+ sector_cnt*512);

		}
		lock_release(&swap_lock);
		return swap_idx //return the index of swap_table
}

/*re allocate frame KAPGE into memory, for spte */
void swap_in (struct sup_pte * spte, void * kpage){

}

/*reset the swap table to false(0) for given swap_index */
void swap_reset(uint32_t swap_idx){
		

}




