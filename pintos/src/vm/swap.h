#ifndef VM_SWAP_H
#ifndef VM_SWAP_H
#define VM_SWAP_H
#include "vm/page.h"

void swap_init(void);

void swap_in (struct sup_pte * page, void * kpage);
uint32_t swap out(void * kpage );
void swap_destory( );

#endif

