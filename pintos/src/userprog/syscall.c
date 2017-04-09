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

struct open_file{
	struct file *file;
	int fd;
	struct list_elem elem;
};

struct open_file* get_file_by_fd(struct list* file_list, int fd);
void file_close_inlist(struct list* file_list, int fd);
void check_address(void *addr);

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

						char * restore_name = malloc(strlen(filename)+1);
						strlcpy(restore_name,filename,strlen(filename)+1);

						char * rest;
						restore_name = strtok_r(restore_name," ",&rest);

						struct file * exec_file = filesys_open(restore_name);
					//	struct file * exec_file = filesys_open(filename);
						
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

						acquire_file_lock();

						new_file = *(p+1);
						new_initial_size = *(p+2);
						//create new file, return true if success the create, else false
						f->eax = filesys_create(new_file, new_initial_size);

						release_file_lock();

						break;
						}
		case SYS_REMOVE:
						check_address(p+1);
						//Needs to be implemented more about open file removed

						acquire_file_lock();

						if(filesys_remove(*(p+1)) == NULL){
							f->eax = false;
						}else{
							f->eax = true;
						}

						release_file_lock();

						break;

		case SYS_OPEN:
						{
						check_address(p+1);

						acquire_file_lock();

						const char* file_name = *(p+1);
						struct file *my_file = filesys_open(file_name);
						
						release_file_lock();

						if(my_file == NULL){
							f->eax = -1;
						}else{
							struct open_file *open_f = malloc(sizeof(struct open_file));
							open_f->file = my_file;
							open_f->fd = thread_current()->fd_count;
							thread_current()->fd_count++;
							list_push_back(&thread_current()->file_list, &open_f->elem);
							f->eax = open_f->fd;
						}
						break;
						}
		case SYS_FILESIZE:
						{
						check_address(p+1);

						acquire_file_lock();

						f->eax = file_length(get_file_by_fd(&thread_current()->file_list,*(p+1))->file);

						release_file_lock();

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
						check_address(p+1);

						acquire_file_lock();

						f->eax = file_tell(get_file_by_fd(&thread_current()->file_list,*(p+1))->file);

						release_file_lock();

						break;

		case SYS_CLOSE:
						check_address(p+1);
						
						acquire_file_lock();
						
						file_close_inlist(&thread_current()->file_list, *(p+1));

						release_file_lock();

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

struct open_file* get_file_by_fd(struct list* file_list, int fd){
		struct list_elem *temp;
		struct open_file *open_f;

		for (temp = list_begin(file_list); temp != list_end(file_list); temp = list_next(temp)){
				open_f = list_entry(temp, struct open_file, elem);
				if (open_f->fd == fd){
					return open_f;
				}
		}
		return NULL;
}

void file_close_inlist(struct list* file_list, int fd){
	struct list_elem *temp;
	struct open_file *open_f;

	for (temp = list_begin(file_list); temp != list_end(file_list); temp = list_next(temp)){
			open_f = list_entry(temp, struct open_file, elem);
			if (open_f->fd == fd){
				file_close(open_f->file);
				list_remove(&open_f->elem);
			}
	}
	free(open_f);
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
