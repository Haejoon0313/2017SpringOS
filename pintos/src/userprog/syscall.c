#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "../threads/vaddr.h"
#include "../filesys/filesys.h"
#include "../filesys/file.h"
#include "userprog/process.h"
#define ARG_MAX = 3;
#define EXIT_ERROR = -1;
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

	switch(sys_call){
		case SYS_HALT:
						power_off();
						break;

		case SYS_EXIT:
						check_address(p+1);
						exit_process(*(p+1));	
						break;

		case SYS_EXEC:{
						check_address(p+1);
						check_address(*(p+1));
						
						//acquire_file_lock()
						char * filename = *(p+1);

						//char * restore_name = malloc(strlen(filename)+1);
						//strlcpy(restore_name,filename,strlen(filename)+1);

						char * rest;

						//struct file * exec_file = filesys_open(strtok_r(restore_name," ",&rest));
						struct file * exec_file = filesys_open(filename);
						
						if(exec_file ==NULL)
								return -1;

						else{
								file_close(exec_file);
								return process_execute(filename);
						}
						//release_file_lock()
						break;
	}

		case SYS_WAIT:{
						check_address(p+1);
						f->eax =  process_wait(*(p+1));
						break;
		}
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
						check_address(p+1);
						check_address(p+3);
						check_address(*(p+2));
						
						//printf("FD : %d \n",*(p+1));
						if(*(p+1) ==1){
										putbuf(*(p+2),*(p+3));

										f->eax = *(p+3);
						}
						else{
										f->eax = *(p+3);

						}
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
		exit_process(-1);
	}

	//user address boundary check
	if (!is_user_vaddr(addr)){
		exit_process(-1);					
	}

	//addreess mapping the virtual memory check
	uint32_t *thread_pd = thread_current()->pagedir;
	void *mapping_check = pagedir_get_page(thread_pd,addr);
	if (mapping_check ==NULL){
		exit_process(-1);	
	}	
}




void exit_process(int status){
	struct list_elem *child_elem;
	struct thread * curr = thread_current();
	struct child * temp_child;

	for (child_elem = list_begin(&curr->parent_process->child_list); child_elem  != list_end(&curr->parent_process->child_list); child_elem = list_next(child_elem)){

					
	temp_child = list_entry(child_elem, struct child, elem);

	
	if(temp_child->pid == curr->tid){
		
		temp_child->is_wait = true;
		temp_child->status = status;
	}
}

	thread_current()->exit_code = status;

	//If parent is waiting on this child,
	if(curr->parent_process->lock_child_id = thread_current()->tid){
			//wake-up parents that is waiting on child to exit.
			sema_up(&thread_current()->parent_process->child_lock);
	}
	thread_exit();


}



//	printf ("system call!\n");
