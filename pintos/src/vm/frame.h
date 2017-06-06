#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"
#include "threads/thread.h"

struct fte
{
				struct thread * origin_thread;
				void * upage;
				void * kpage;
				struct list_elem elem;
};

void frame_table_init(void);
void * frame_alloc(void * upage, enum palloc_flags);
void frame_free(void * upage);
void * frame_evict(enum palloc_flags);
void frame_table_lock_acquire(void);
void frame_table_lock_release(void);

#endif
