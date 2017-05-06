#ifndef VM_SWAP_H
#define VM_SWAP_H
#include "vm/page.h"

void swap_init(void);

void swap_in (struct sup_pte * page, void * kpage);
size_t swap_out(void * kpage );
void swap_set(uint32_t swap_idx, bool value); 
#endif

