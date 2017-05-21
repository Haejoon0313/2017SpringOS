#include "vm/frame.h"
#include <list.h>
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include "vm/swap.h"

static struct list frame_table;
static struct lock frame_table_lock;

void frame_table_init(void){
				list_init(&frame_table);
				lock_init(&frame_table_lock);
}


/* allocate frame in physical memory by call "palloc_get_page()". 
	 Then make frame table entry, and push it into Frame Table. */
void * frame_alloc(void * upage, enum palloc_flags flag){
				
				void * kpage = palloc_get_page(PAL_USER | flag);
				struct fte * fte;

				if(kpage == NULL){
								kpage = frame_evict(flag);
				}
				if(kpage != NULL){
					fte = (struct fte *)malloc(sizeof(struct fte));
					fte->origin_thread = thread_current();
					fte->upage = upage;
					fte->kpage = kpage;
					list_push_back(&frame_table, &fte->elem);
				}
				return kpage;//return kernel virtual address, pointing frame
}


/* free the frame(in physical memory) mapped by "upage" virtual address, 
	 and remove it in Frame table, and free the frame table entry struct.
	 Search the frame table, and do step by step.
	 */
void frame_free(void *upage){
				struct fte *temp_frame;
				struct list_elem *e = NULL;
				for(e = list_begin(&frame_table) ; e != list_end(&frame_table) ; e = list_next(e)){
								temp_frame = list_entry(e,struct fte, elem);
								
								if(temp_frame->kpage == upage){
												palloc_free_page(temp_frame->kpage);
												list_remove(e);
												free(temp_frame);
												break;
								}
				}
}

/*Evict the frame in frame table, according to FIFO algorithm.
 */
void * frame_evict(enum palloc_flags flag){
				struct list_elem * el;
				struct fte * evict_fte;
				struct sup_pte * spte;
				bool is_dirty;

				el = list_begin(&frame_table);

				/*Simple FIFO algorithm */
				evict_fte = list_entry(el, struct fte, elem);
				void * upage = evict_fte->upage;
				void * kpage = evict_fte->kpage;

				spte = get_sup_pte(&(evict_fte->origin_thread->sup_page_table), upage);
				
				is_dirty = pagedir_is_dirty(evict_fte->origin_thread->pagedir,upage);
				
				if(spte->mmap_id == -1){
						if(is_dirty){//written, so saved at swap disk
						/*swap out the first entry of Frame table */
										spte->swapped = true;
										ASSERT(spte->loaded);
										spte->swap_index = swap_out(kpage);
						}
						else{//read_only data, so just remove
										spte->loaded = false;
						}
				}
				else{//mmap file case
						if(is_dirty){
								acquire_file_lock();
								file_write_at(spte->file,spte->upage,spte->read_bytes,spte->file_ofs);
								release_file_lock();
						}
						spte->loaded = false;
				}
				pagedir_clear_page(evict_fte->origin_thread->pagedir, upage);
				
				palloc_free_page(kpage);
				list_remove(el);
				free(evict_fte);
			
				return palloc_get_page(PAL_USER | flag);
}


void frame_table_lock_acquire(void){
				lock_acquire(&frame_table_lock);
}

void frame_table_lock_release(void){
				lock_release(&frame_table_lock);
}
