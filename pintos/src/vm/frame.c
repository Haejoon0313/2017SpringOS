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

struct list frame_table;
struct lock frame_table_lock;

void frame_table_init(void){
				list_init(&frame_table);
				lock_init(&frame_table_lock);
}


/* allocate frame in physical memory by call "palloc_get_page()". 
	 Then make frame table entry, and push it into Frame Table. */
void * frame_alloc(void * upage, enum palloc_flags flag){
				
				void * kpage = palloc_get_page(PAL_USER | flag);

				frame_table_lock_acquire();

				if(kpage == NULL){
								kpage = frame_evict(flag);

								frame_table_lock_release();
				}else{
								struct fte * fte = malloc(sizeof(fte));
								fte->thread = thread_current();
								fte->upage = upage;
								fte->kpage = kpage;
								list_push_back(&frame_table, &fte->elem);

								frame_table_lock_release();
				}

				return kpage;//return kernel virtual address, pointing frame
}


/* free the frame(in physical memory) mapped by "upage" virtual address, 
	 and remove it in Frame table, and free the frame table entry struct.
	 Search the frame table, and do step by step.
	 */
void frame_free(void *upage){
				struct fte *temp_frame;

				for(struct list_elem *e = list_begin(&frame_table) ; e != list_end(&frame_table) ; e = list_next(el)){
								temp_frame = list_entry(e,struct fte, elem);
								
								if(fte->upage == upage){
												palloc_free_page(fte->kpage);
												list_remove(e);
												free(temp_frame);
												break;
								}
				}
}


void * frame_evict(enum palloc_flags flag){
				return NULL;
}

void frame_table_lock_acquire(void){
				lock_acquire(&frame_table_lock);
}

void frame_table_lock_release(void){
				lock_release(&frame_table_lock);
}
