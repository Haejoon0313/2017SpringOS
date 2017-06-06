#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "userprog/process.h"
#ifdef FILESYS
#include "filesys/inode.h"

#endif
#include "vm/page.h"

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
	if (!mapping_check)
					mapping_check = pagedir_get_page(thread_current()->pagedir, addr);

	//if (mapping_check==NULL){
	//				exit_process(-1);
	//}	
}




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

	thread_current()->exception_esp = f->esp;
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
						f->eax = filesys_create(new_file, new_initial_size, false);

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
						void * buffer = (void *) *(p+2);
						check_address(p+1);				
						check_address(*(p+2));
						check_address(p+3);
						// check address valid

						int i;
						int read_fd = *(p+1);
						uint32_t* read_buffer = *(p+2);
						unsigned read_size = *(p+3);
						int file_size;

						//Read from keyboard using input_getc()
						if(read_fd == 0){						
										for(i=0; i<read_size ; i++){
												read_buffer[i] = input_getc();
												f->eax = read_size; 
								}
						//Read by open file.
						}else{
								struct open_file * read_file = get_file_by_fd(&thread_current()->file_list,read_fd);
								if(read_file==NULL || read_file->file == NULL){
												f->eax = -1;
								}else{
										//read file and return the bytes actually read.
										if(!is_user_vaddr(buffer)){
														f->eax = -1;
										}else{
														acquire_file_lock();
														file_size = file_read(read_file->file,buffer,read_size);
														f->eax = file_size;
														release_file_lock();
										}
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
#ifdef VM
		case SYS_MMAP:
						{
										
						check_address(p+1);
						check_address(p+2);


						int fd = *(p+1);
						uint8_t * addr = *(p+2);

						/*address == 0 case*/
						if(addr==0)
						{	
							f->eax= MAP_FAILED;
							break;
						}
						/*address is not page-aligned case*/
						if(pg_ofs(addr)!=0){
							f->eax = MAP_FAILED;
							break;
						}
						/*fd==0 or 1 means the console I/O case */
						if((fd==0)||(fd==1)){
							f->eax=MAP_FAILED;
							break;
						}
						
						struct thread * curr = thread_current();
						struct open_file * mmap_open_file = get_file_by_fd(&curr->file_list,fd);
						struct file * mmap_file = mmap_open_file->file;
						/*mmap fiel is null case */
						if(mmap_open_file == NULL)
						{
										f->eax = MAP_FAILED;
										break;
						}

						size_t read_bytes =  file_length(mmap_file);
						off_t cur_ofs = 0;
						size_t remain_read_bytes = read_bytes;
						mapid_t map_id = curr->mmap_count;
						
						if(read_bytes == 0){
										f->eax = MAP_FAILED;
										break;
						};

						frame_table_lock_acquire();
						bool insert_success;
						bool is_fail = false;
						/*insert mmap file into supplemental page table */
						while(remain_read_bytes >0)
						{								
								/*Actual read bytes */
								size_t page_read_bytes = (remain_read_bytes >= PGSIZE ? PGSIZE : remain_read_bytes);

								bool insert_success = page_insert(file_reopen(mmap_file), cur_ofs, addr , page_read_bytes, PGSIZE-page_read_bytes, true);

								/*mmap file insert to supplemental page table is failed*/
								if(!insert_success){
									is_fail = true;
									break;
								}

								struct sup_pte * spte = get_sup_pte(&curr->sup_page_table,addr);
								
								ASSERT(spte);
								spte->mmap_id = map_id;
								spte->loaded = false;

								remain_read_bytes -= page_read_bytes;
								cur_ofs += PGSIZE;
								list_push_back(&curr->mmap_list,&spte->list_elem);
								addr += PGSIZE;
						}
						frame_table_lock_release();
						if(is_fail)
										f->eax = MAP_FAILED;
						else{
										curr->mmap_count++;
										f->eax = map_id;
						}
						break;
						}

		case SYS_MUNMAP:
						{
						check_address(p+1);
						int munmap_id = *(p+1);
						struct thread * curr = thread_current();
						struct list_elem * el = NULL;
						frame_table_lock_acquire();

						for(el = list_front(&curr->mmap_list) ;el != list_end(&curr->mmap_list) ;  el = list_next(el) ){
								struct sup_pte * spte = list_entry(el,struct sup_pte, list_elem);
								if(munmap_id == spte->mmap_id){

										/*now, removing munmap file */
										list_remove(&spte->list_elem);//remove from mmap list
										void * kpage = pagedir_get_page(curr->pagedir,spte->upage);
										if(kpage !=NULL){
												ASSERT(spte->loaded);
												/*If file is dirty, rewrite it to file, not swap disk. */
												if(pagedir_is_dirty(curr->pagedir, spte->upage)){
														acquire_file_lock();
														file_write_at(spte->file,spte->upage,spte->read_bytes,spte->file_ofs);
														release_file_lock();
												}
												pagedir_clear_page(curr->pagedir,spte->upage);
												page_remove(&curr->sup_page_table, spte->upage);
												frame_free(kpage);
								}
										else{
												page_remove(&curr->sup_page_table, spte->upage);
								}
								}
						}
						frame_table_lock_release();
						break;						
						}
#endif
#ifdef FILESYS
		case SYS_CHDIR:
						{
						check_address(p+1);
						char * path = *(p+1);
						struct dir * dir = path_parsing(path);
						struct thread * curr = thread_current();

						if(dir == NULL){
										f->eax = false;
						}else{
										dir_close(curr->dir);
										curr->dir = dir;
										f->eax = true;
						}
						break;
						}
		case SYS_MKDIR:
						{
						check_address(p+1);
						char * new_dir = *(p+1);

						acquire_file_lock();

						if(filesys_create(new_dir, 0, true)){
										release_file_lock();
										f->eax = true;
						}else{
										release_file_lock();
										f->eax = false;
						}

						break;
						}
		case SYS_READDIR:
						{
						check_address(p+1);
						//check_address(*(p+2));
						check_address(p+2);
						struct dir *read_dir;
						bool success;

						int fd = *(p+1);
						char * name = NULL;
						name= *(p+2);

						acquire_file_lock();
						struct open_file *  read_temp = get_file_by_fd(&thread_current()->file_list, fd);
						struct file * read_file = read_temp->file;

						if((read_temp == NULL) || (read_file == NULL))
						{
							exit_process(-1);
							break;
						}
						success = readdir_manager(read_file, name);
						release_file_lock();
						f->eax = success;

						break;
						}
		case SYS_ISDIR:
						{
						check_address(p+1);
						int fd = *(p+1);
						struct open_file *  temp = get_file_by_fd(&thread_current()->file_list, fd);
						struct file * check_file = temp->file;
							
						if (check_file == NULL)
						{
							f->eax = -1;
							break;
						}

						f->eax = inode_dir_check(file_get_inode(check_file));
						break;
						}
		case SYS_INUMBER:
						{
						check_address(p+1);
						int fd = *(p+1);
						struct open_file * temp = get_file_by_fd(&thread_current()->file_list, fd);
						struct file * number_file = temp->file;

						if(number_file == NULL)
						{
							f->eax = -1;
							break;
						}
					
						int inumber = inode_to_inumber(file_get_inode(number_file));
						
						ASSERT(inumber != NULL);

						f->eax = inumber;

						break;
						}
#endif

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

