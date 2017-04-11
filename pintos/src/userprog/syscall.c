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

// struct open_file needs to save file opened and file descriptor

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
						
						acquire_file_lock();
						char * filename = *(p+1);

						char * restore_name = malloc(strlen(filename)+1);
						strlcpy(restore_name,filename,strlen(filename)+1);

						char * rest;
						restore_name = strtok_r(restore_name," ",&rest);
						//to check whether this file is openable or not.
						struct file * exec_file = filesys_open(restore_name);
						
						if(exec_file == NULL){
								free(restore_name);
								release_file_lock();
								f->eax = -1;
						}else{//If openable, now handled by process_execute() function in userprog/process.c
								free(restore_name);
								file_close(exec_file);
								release_file_lock();
								f->eax = process_execute(filename);//execute start
						}
						break;
	}

		case SYS_WAIT:{
						check_address(p+1);
						int wait_pid =*(p+1);
						f->eax =  process_wait(wait_pid);//handled by process_wait() function in userprog/process.c
						break;
		}

		case SYS_CREATE:
						{
						check_address(*(p+1));
						check_address(p+2);

						const char * new_file;
						unsigned new_initial_size;
						//check address validation
						acquire_file_lock();

						new_file = *(p+1);
						new_initial_size = *(p+2);
						//create new file, return true if success the create, else false
						f->eax = filesys_create(new_file, new_initial_size);

						release_file_lock();

						break;
						}

		case SYS_REMOVE:
						check_address(p+1); // check address valid
				
						acquire_file_lock();

						if(filesys_remove(*(p+1)) == NULL){
							f->eax = false; // no file return false
						}else{
							f->eax = true; // success return true
						}
						release_file_lock();

						break;

		case SYS_OPEN:
						{
						check_address(*(p+1)); // check address valid

						acquire_file_lock();

						const char* file_name = *(p+1);
						struct file *my_file = filesys_open(file_name);
						
						release_file_lock();

						if(my_file == NULL){
							f->eax = -1; // no file return -1
						}else{
							struct open_file *open_f = malloc(sizeof(struct open_file)); // malloc for open_file
							open_f->file = my_file;
							open_f->fd = thread_current()->fd_count; // set fd as current thread fd_count
							thread_current()->fd_count++; // set fd_count plus 1
							list_push_back(&thread_current()->file_list, &open_f->elem); // add to file list
							f->eax = open_f->fd; // return open_file fd
						}
						break;
						}

		case SYS_FILESIZE:
						{
						check_address(p+1); //check address valid

						acquire_file_lock();

						f->eax = file_length(get_file_by_fd(&thread_current()->file_list,*(p+1))->file); // find file in file_list by using fd, then get file_length

						release_file_lock();

						break;
						}
		case SYS_READ:{
						check_address(p+1);
						check_address(*(p+2));
						check_address(p+3);
						// check address valid

						int i;
						int read_fd = *(p+1);
						uint8_t* read_buffer = *(p+2);
						unsigned read_size = *(p+3);
						
						//Read from keyboard using input_getc()
						if(read_fd == 0){						
										for(i=0; i<read_size ; i++){
												read_buffer[i] = input_getc();
												f->eax = read_size; 
								}
						//Read by open file.
						}else{
								struct open_file * read_file = get_file_by_fd(&thread_current()->file_list,read_fd);
								if(read_file==NULL){
												f->eax = -1;
								}else{
										acquire_file_lock();
										//read file and return the bytes actually read.
										f->eax= file_read(read_file->file,read_buffer,read_size);
										release_file_lock();
								}
						}
						break;
						}

		case SYS_WRITE:
						check_address(p+1);
						check_address(p+3);
						check_address(*(p+2));
						// check address valid
						unsigned write_size = *(p+3);
						//writes to console case
						if(*(p+1) ==1){
								putbuf(*(p+2),*(p+3));

								f->eax = write_size;//return the sizes actually write.
						}
						//writes to file case
						else{
								struct open_file * write_file = get_file_by_fd(&thread_current()->file_list,*(p+1));
								if(write_file==NULL){
										f->eax=-1;
								}else{
								acquire_file_lock();
								f->eax = file_write(write_file->file,*(p+2),write_size);//return the sizes actually write.
								release_file_lock();
						}}
						break;

		case SYS_SEEK:
						check_address(p+1);
						check_address(p+2);
						// check address valid
						acquire_file_lock();

						file_seek(get_file_by_fd(&thread_current()->file_list,*(p+1))->file,*(p+2)); // find file in file_list by using fd, then do file_seek for (p+2) position
						release_file_lock();
						break;

		case SYS_TELL:
						check_address(p+1); // check address valid

						acquire_file_lock();

						f->eax = file_tell(get_file_by_fd(&thread_current()->file_list,*(p+1))->file); // find file in file_list by using fd, then get file_tell

						release_file_lock();

						break;

		case SYS_CLOSE:
						check_address(p+1); // check address valid
						
						acquire_file_lock();
						
						file_close_inlist(&thread_current()->file_list, *(p+1)); // remove file in file_list, then close file

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
	void *mapping_check = pagedir_get_page(thread_current()->pagedir, addr);
	if (!mapping_check){
		exit_process(-1);
	}	
}

struct open_file* get_file_by_fd(struct list* file_list, int fd){
		struct list_elem *temp;
		struct open_file *open_f;

		for (temp = list_begin(file_list); temp != list_end(file_list); temp = list_next(temp)){
				open_f = list_entry(temp, struct open_file, elem); // search in file_list
				if (open_f->fd == fd){
					return open_f; // if find right fd, return open_file struct
				}
		}
		return NULL; // if cannot find fd, return NULL
}

void file_close_inlist(struct list* file_list, int fd){
	struct list_elem *temp;
	struct open_file *open_f;

	for (temp = list_begin(file_list); temp != list_end(file_list); temp = list_next(temp)){
			open_f = list_entry(temp, struct open_file, elem); // search in file_list
			if (open_f->fd == fd){
				file_close(open_f->file); // if find right fd, file close
				list_remove(temp); // remove element in list
			}
	}
} // if cannot find fd, do nothing


/*
EXIT syscall management function. Search all the child list of parent, and if there is child  that thread wait, 
save the status of child thread and exit.
	 */
void exit_process(int status){
	struct list_elem *child_elem;
	struct thread * curr = thread_current();

	//search all the childlist of parent threads, and setting all the status, wait_identifier of child.
	for (child_elem = list_begin(&curr->parent_process->child_list); child_elem  != list_end(&curr->parent_process->child_list); child_elem = list_next(child_elem)){			
		struct child * temp_child = list_entry(child_elem, struct child, elem);

		//setting child struct status and wait identifier.
		if(temp_child->pid == curr->tid){
			temp_child->is_wait = true;
			temp_child->status = status;
		}
	}
	//setting exit status
	thread_current()->exit_code = status;

	//If parent is waiting for me to exit, then
	if(curr->parent_process->lock_child_id == thread_current()->tid){
			//wake-up parents that is waiting on child to exit.
			sema_up(&thread_current()->parent_process->child_lock);
	}
	thread_exit();
}


/*
When thread exit, needs to free all the files that is owned by thread.
This function is used for manage that case.
	 */
void close_all_filelist(struct list * file_list){
	struct list_elem *temp;
	struct open_file *open_f;

	//for all the open_file structure on file_list
	while(!list_empty(file_list)){
		temp = list_pop_front(file_list);

		open_f = list_entry(temp, struct open_file, elem);
		list_remove(temp);//remove from file_list
		free(open_f);//free all the open_file struct
	}
}

