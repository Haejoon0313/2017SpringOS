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
  
	int *p = f->esp;

	int sys_call = *p;
	printf("syscall number : %d\n", sys_call);
	switch(sys_call){
		case SYS_HALT:

						break;
					
		case SYS_EXIT:
						//printf("exit thread! \n");
						thread_exit();
						break;

		case SYS_EXEC:
		
						break;
		
		case SYS_WAIT:

						break;

		case SYS_CREATE:

						break;

		case SYS_REMOVE:
						break;

		case SYS_OPEN:

						break;

		case SYS_FILESIZE:
						break;

		case SYS_READ:
						break;


		case SYS_WRITE:
						//printf("write to file!\n");
						printf((int)*(p+1), (const void *)*(p+2),(unsigned)*(p+3));			
						break;

		case SYS_SEEK:
						break;

		case SYS_TELL:
						break;

		case SYS_CLOSE:
						break;

	}
				
				
				
				
				
	printf ("system call!\n");
}
