#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

#ifdef VM
#include <debug.h>
#include <stdint.h>
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "vm/frame.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#endif

#define STACK_LIMIT  (1<<23)//8MB size
/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);
static bool install_page(void *upage, void * kapge, bool writable);
/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
 	{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

	/* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;//page fault is because of not present?????
  write = (f->error_code & PF_W) != 0;			//us it only for read??????
  user = (f->error_code & PF_U) != 0;				//is it not user range?????

  /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */
	//Tips about pj4, page fault handler!


#ifdef VM
	struct thread * curr = thread_current();
	struct sup_pte * spte = NULL;
	void * kpage;
	void * upage=NULL;
	
	//printf("[#]fault addr : %x \n",fault_addr);
 	//printf("[#]pf cnt : %d \n",page_fault_cnt);

	if(not_present &&is_user_vaddr(fault_addr)){
					frame_table_lock_acquire();
  				upage = pg_round_down(fault_addr);
					spte = get_sup_pte(&curr->sup_page_table,upage);

					

					/*page fault is caused by STACK GROWTH*/
					if(spte==NULL){

									if(!user){
										f->esp = curr->exception_esp;
									}
									/* MAX stack size =8MB */                      /*For PUSHA instruction */				
									if(fault_addr >= PHYS_BASE - (1<<23) && (uint8_t *)f->esp -32<= (uint8_t *)fault_addr){
										kpage = frame_alloc(upage, PAL_ZERO);
								
										bool stack_check = pagedir_get_page(curr->pagedir,upage);
										bool stack_set = pagedir_set_page(curr->pagedir, upage, kpage, true);
										
										if((stack_check !=NULL) || !stack_set){
														frame_free(kpage);
										}else{
														page_insert(NULL,NULL,upage,NULL,NULL,true);
														frame_table_lock_release();
														return;
										}
							}
					}

					/*page fault is caused by swap or lazy loading */					
					else{ 
									/*page fault is caused by SWAP */									
									if(spte->swapped&& spte->loaded && spte->swapped && !(pagedir_get_page(curr->pagedir,spte->upage))) {
											kpage = frame_alloc(upage, PAL_ZERO);
											swap_in(spte,kpage);

											bool swap_check = pagedir_get_page(curr->pagedir,spte->upage);						
											bool swap_set = pagedir_set_page(curr->pagedir, upage, kpage, true);

											if ( (swap_check !=NULL) || !swap_set){
														frame_free(kpage);													
														
										 }else{
														pagedir_set_dirty(curr->pagedir, upage,true);
														pagedir_set_accessed(curr->pagedir, upage, true);
														spte->swapped = false;
														ASSERT(spte->loaded);
														frame_table_lock_release();
														return;
										 }}
									/*page fault is caused by LAZY LOADING*/
									else if(!spte->loaded){
									
											/*Check the correctness of load page size */
											ASSERT(spte->read_bytes + spte->zero_bytes == PGSIZE);

													/*CASE 1. All non-zero page  */
											if (spte->read_bytes == PGSIZE){
														void *kpage = frame_alloc(upage,0);
														
														struct sup_pte * spte = get_sup_pte(&curr->sup_page_table,upage);
														ASSERT(spte != NULL);

														if(file_read_at(spte->file, kpage, spte->read_bytes,spte->file_ofs) != (int) spte->read_bytes){
																		frame_free(upage);
																		frame_table_lock_release();
																		release_file_lock();
																		return false;
														}
											
														bool null_check = pagedir_get_page(curr->pagedir,spte->upage);
														bool set_success = pagedir_set_page(curr->pagedir, upage, kpage, spte->writable);
												
														if((null_check!=NULL) || !set_success){
																		frame_free(upage);
																		frame_table_lock_release();
																		page_remove(&curr->sup_page_table, upage);
																		return false;
														}

														spte->loaded = true;
														pagedir_set_accessed(curr->pagedir, upage, true);
														frame_table_lock_release();
														return;
											}

											/*CASE 2. All zeroed page */
											else if(spte->zero_bytes == PGSIZE){
														void *kpage = frame_alloc(upage,PAL_ZERO);
														
														struct sup_pte * spte = get_sup_pte(&curr->sup_page_table,upage);
														ASSERT(spte != NULL);
									
														bool null_check = pagedir_get_page(curr->pagedir,spte->upage);
														bool set_success = pagedir_set_page(curr->pagedir, upage, kpage, spte->writable);
												
														if((null_check!=NULL) || !set_success){
																		frame_free(upage);
																		frame_table_lock_release();
																		page_remove(&curr->sup_page_table, upage);
																		return false;
														}
														
														spte->loaded = true;
														pagedir_set_accessed(curr->pagedir, upage, true);
														frame_table_lock_release();
														return;
											}

											/*CASE 3. Partial page */
											else {
														void * kpage = frame_alloc(upage,0);
														
														struct sup_pte * spte = get_sup_pte(&curr->sup_page_table, upage);

														ASSERT(spte->file != NULL);
														ASSERT(kpage != NULL);

														if (file_read_at(spte->file, kpage, spte->read_bytes,spte->file_ofs) != (int) spte->read_bytes){
																		frame_free(upage);
																		frame_table_lock_release();
																		return false;
														}
														
														memset(kpage + spte->read_bytes, 0 ,spte->zero_bytes);
														
														bool null_check = pagedir_get_page(curr->pagedir,spte->upage);
														bool set_success = pagedir_set_page(curr->pagedir, upage, kpage, spte->writable);
												
														if((null_check!=NULL) || !set_success){															
																		frame_free(upage);
																		frame_table_lock_release();
																		return false;
														}

														spte->loaded = true;
														pagedir_set_accessed(curr->pagedir, upage, true);
														frame_table_lock_release();
														return;
											}
									}
					}
					frame_table_lock_release();
	}


	exit_process(-1);




#else
	printf ("Page fault at %p: %s error %s page in %s context.\n",
          fault_addr,
          not_present ? "not present" : "rights violation",
          write ? "writing" : "reading",
          user ? "user" : "kernel");
  kill (f);
#endif

}

