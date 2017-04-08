#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "../threads/vaddr.h"
#include "../filesys/filesys.h"
#include "../filesys/file.h"
#define ARG_MAX = 3;
static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //interrupt frame stack accessing address(ESP) 
	int *p = f->esp;
	int args[3];

	//address validation check
	check_address(p);
 
	/*System call handling according to its system call Number*/
	int sys_call = *p;
	//printf("syscall number : %d\n", sys_call);
	switch(sys_call){
		case SYS_HALT:
						power_off();
						break;
					
		case SYS_EXIT:
						//printf("exit thread! status is %d\n",*(p+1));
						printf("%s: exit(%d)\n",thread_current()->name,*(p+1));
						thread_exit();
						break;

		case SYS_EXEC:
		
						break;
		
		case SYS_WAIT:

						break;

		case SYS_CREATE:
						{
						const char * new_file;
						unsigned new_initial_size;
						//check address validation
						check_address(p+1);
						check_address(p+2);
						new_file = *(p+1);
						new_initial_size = *(p+2);
						//create new file
						bool is_create_success = filesys_create(new_file,new_initial_size);						
						//return true if success the create, else false
						f->eax = is_create_success;
						break;
						}
		case SYS_REMOVE:
						check_address(p+1);
						//Needs to be implemented more about open file removed
						f->eax=filesys_remove((const char*)(p+1));
						break;

		case SYS_OPEN:
						{
						check_address(p+1);
						const char* file_name = *(p+1);
						struct file *my_file = filesys_open(file_name);

						if(!my_file){
							f->eax = -1;
						}
						break;
						}
		case SYS_FILESIZE:
						{
						//check_addrss(p+1);
						//f->eax = filesize(*(p+1));
						break;
						}
		case SYS_READ:
						break;


		case SYS_WRITE:
						check_address(p+3);
						check_address(*(p+2));
						if(*(p+1) ==1){
										putbuf(*(p+2),*(p+3));

										f->eax = *(p+3);
						}
						
						//printf((int)*(p+5), (const void *)*(p+6),(unsigned)*(p+7));			
						break;

		case SYS_SEEK:
						break;

		case SYS_TELL:
						break;

		case SYS_CLOSE:
						break;

	}
				
}				

/*
CHECK address validation in 3 case, and thread_exit if invalid ptr
	 */
void check_address(void *addr){

	/*PART1. Address validation part*/
	//NULL pointer check
	if(!addr){
		printf("%s: exit(%d)",thread_current()->name,-1);
		thread_exit();
	}
	//user address boundary check
	if (!is_user_vaddr(addr)){
		printf("%s: exit(%d)",thread_current()->name,-1);	
		thread_exit();
	}
	//addreess mapping the virtual memory check
	uint32_t *thread_pd = thread_current()->pagedir;
	void *mapping_check = pagedir_get_page(thread_pd,addr);
	if (mapping_check ==NULL){
		printf("%s: exit(%d)",thread_current()->name,-1);		
		thread_exit();
		
	}	
}


//Until now, NOT use this function
/*
Function that pop arguments from intr_frame stack, and save it
on args[] array. Number of arguments is same with argc.
In Fact, its not pop, just for convenience.(진짜 팝하는건 아님ㅎ)
	 */
void pop_args(struct intr_frame *f,int *args, int argc){
	int i = 0;
	int * argument_ptr;

	for(i=0; i < argc ; i++){
		argument_ptr = (int *)(f->esp + 1+i);
		check_address((void *)argument_ptr);//validate the ptr.
		args[i] = *argument_ptr;
	}



}
//	printf ("system call!\n");

