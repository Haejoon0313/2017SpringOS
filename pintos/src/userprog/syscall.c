#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  
	int * p = f->esp;

	int sys_call = *p;

	switch(sys_call){
		case SYS_EXIT:
						printf("exit thread! \n");
						thread_exit();
						break;

		case SYS_WRITE:
						printf("write to file!\n");
						break;


	}
				
				
				
				
				
	printf ("system call!\n");
  //thread_exit ();
}
