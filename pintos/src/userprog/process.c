#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#ifdef VM
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#endif
static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;
  char * fn;
	/* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  fn = malloc(strlen(file_name)+1);
	strlcpy(fn,file_name,strlen(file_name)+1);
	strlcpy (fn_copy, file_name, PGSIZE);	

	char * rest;
	fn = strtok_r(fn," ",&rest);
	/* Create a new thread to execute FILE_NAME. */
  tid = thread_create (fn, PRI_DEFAULT, start_process, fn_copy);
  
	if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 
  
	free(fn);
	
	//wait for new thread to finish
	sema_down(&thread_current()->child_lock);

	//if current thread failed to load, return -1
	if(!thread_current()->load_success){
		return -1;
	}
		return tid;
	
}

/* A thread function that loads a user process and makes it start
   running. */
static void
start_process (void *f_name)
{
  char *file_name = f_name;
  struct intr_frame if_;
  bool success;	

	#ifdef VM
	struct thread *curr = thread_current();
	page_table_init(&curr->sup_page_table);
	curr->mmap_count= 0;
	list_init(&curr->mmap_list);



	#endif

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
	//In tihs step, load ELF excutable file of user program.
 	
	success = load (file_name, &if_.eip, &if_.esp);
 
  /* If load failed, quit. */
  palloc_free_page (file_name);
  
	/*Always need to indicate the result of load, and unblock the parent thread by sema_up. 
		If fail, exit */
	if (!success)
	{
		thread_current()->parent_process->load_success = false;
		sema_up(&thread_current()->parent_process->child_lock);
    thread_exit ();
	}else{
		thread_current()->parent_process->load_success = true;
		sema_up(&thread_current()->parent_process->child_lock);
	}
		/*
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
		 
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{

	
	struct thread * curr = thread_current();
	struct list_elem *temp_el;
	struct child * cp = NULL;
	struct list_elem * return_elem;

	//For all the child threads of thread, search for child that has pid child_tid.
	for(temp_el=list_begin(&curr->child_list) ; temp_el != list_end(&curr->child_list); temp_el = list_next(temp_el)){
		
		struct child *child_process = list_entry(temp_el,struct child, elem);
		if(child_process->pid == child_tid){
			cp = child_process;		
			return_elem = temp_el;

			}
	}

	//invalid or not a child check
	if(!cp){
			return -1;
	}
	//set the lock_child__id, to identify that which child procsss makes parent to wait.
	curr->lock_child_id = cp->pid;
	
	//if not already_waiting, let's wait!
	if(!cp->is_wait)
		sema_down(&thread_current()->child_lock);

	int return_status = cp->status;
	//remove exit child from the child list of thread, and free child struct that allocated dynamically.
	list_remove(return_elem);
	free(cp);
	return return_status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *curr = thread_current ();
  uint32_t *pd;
	int exit_status = curr->exit_code;

	//if exit status is default setting, exit with error code.
	if(exit_status == -2){
		exit_process(-1);
	}
	
	printf("%s: exit(%d)\n",curr->name,exit_status);

	acquire_file_lock();
	if(!(curr->load ==NULL)){
		file_close(curr->load);//close the load executable file of current thread that exit.
		
	}
	close_all_filelist(&curr->file_list);//close all the files obtained by current thread.

	release_file_lock();

	#ifdef VM
	frame_table_lock_acquire();
	page_table_remove(&curr->sup_page_table);
	frame_table_lock_release();
	#endif

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = curr->pagedir;
  if (pd != NULL) 
    {
     /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      curr->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp,char* file_name);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

	
  /* Allocate and activate page directory. */
  acquire_file_lock();
	t->pagedir = pagedir_create ();//create&return new page table.
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();//load pagedir to CPU register
	char * rest;

	char * file_copy =  malloc(strlen(file_name)+1);

  strlcpy(file_copy,file_name,strlen(file_name)+1);
	file_copy = strtok_r(file_copy," ",&rest);

	/* Open executable file. */
  
	file = filesys_open (file_copy);
	free(file_copy);
  
	if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff; //e_phoff = Program header table's file ofs.
	for (i = 0; i < ehdr.e_phnum; i++)//number of entries in the program header table
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))//error case
        goto done;
      file_seek (file, file_ofs);//change the read point into offset

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)//read the header of file & save it into phdr.
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
								default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;//location of file. p_offset means file offset where segment is located.
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;//where to save the file in memory, by virtual address
              uint32_t page_offset = phdr.p_vaddr & PGMASK;//p_vaddr = Virtual address of beginning of segment.
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);

				       }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp,file_name))
	{  
		goto done;
}
  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;//eip = Address to jump to in order to start program
	thread_current()->load = file;
  success = true;
	file_deny_write(file);//deny the file that is owned by current thread.
	

	done:
  /* We arrive here whether the load is successful or not. */

	release_file_lock();

	return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
	
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  
	off_t cur_ofs = ofs;
	struct thread * curr = thread_current();

	while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Do calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;	//the number of bytes to read from the executable file.
      size_t page_zero_bytes = PGSIZE - page_read_bytes;	//the number of bytes to initialize to zero fillowing the bytes read


#ifdef VM
			frame_table_lock_acquire();
			page_insert(file, cur_ofs, upage, page_read_bytes, page_zero_bytes, writable);//Supplement page table manage
	
			struct sup_pte * spte = get_sup_pte(&curr->sup_page_table,upage);
			ASSERT(spte != NULL);
			/*Lazy loading now. Initially file segment is lot loaded to memory, so spte->loaded is false.*/
			spte->loaded = false;
			frame_table_lock_release();

	/*	
			
			void * kpage = frame_alloc(upage,0);//allocate frame for upage AND manage frame table etc.
			
			struct sup_pte * spte = get_sup_pte(&curr->sup_page_table,upage);//ERROR case management required???????????
			
			ASSERT(spte != NULL);

			if (file_read(file, kpage, page_read_bytes) != (int)page_read_bytes)//write file contents into frame
			{
					frame_free(upage);
					page_remove(&curr->sup_page_table,upage);
					return false;
			}
     memset (kpage + page_read_bytes, 0, page_zero_bytes);//set rest of page into 0. Maybe for alignment of page?

    	// Add the page to the process's address space. 
      if (!install_page (upage, kpage, writable)) 
        {
					frame_free(upage);
					page_remove(&curr->sup_page_table,upage);	
          return false; 
        }
				*/
#else

			/*Before Project3 VM part. Need to Remove later */
			
      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)//raed file & save it into KPAGE
        {
          palloc_free_page (kpage);
          return false; 
       }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);//set rest of page into 0. Maybe for alignment of page?

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }


#endif

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
			cur_ofs += PGSIZE;


   }

  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp,char * file_name) 
{
  void * kpage;
  bool success = false;
	struct thread * curr = thread_current();
#ifdef VM
	frame_table_lock_acquire();
	kpage = frame_alloc(((uint8_t *)PHYS_BASE) - PGSIZE, PAL_ZERO);

	if(kpage != NULL){
			success = install_page(((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
			if (success)
			{
					*esp = PHYS_BASE;
					page_insert(NULL,NULL, ((uint8_t *)PHYS_BASE) - PGSIZE, NULL, NULL,true);
			}
			else{
					frame_free(kpage);
			}
	}
	frame_table_lock_release();


#else
	//Before pj4(VM). NEed to be removed later.

	
  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
			{
							*esp = PHYS_BASE;
			}else
        palloc_free_page (kpage);
    };
#endif

	char *token, *rest;
	int argc = 0;
	char * file_copy = malloc(strlen(file_name)+1);
	
	strlcpy(file_copy,file_name,strlen(file_name)+1);

	for (token = strtok_r(file_copy," ",&rest); token != NULL; token = strtok_r(NULL," ",&rest))
		argc++;//count the argument token numbers
	
	

	int *argv = calloc(argc,sizeof(int));
	int i = 0;
	//STEP1. push command token to stack.
	for(token = strtok_r(file_name," ",&rest); token != NULL; token =strtok_r(NULL," ",&rest)){
		*esp = *esp - (strlen(token)+1);
		memcpy(*esp,token,strlen(token)+1);
		argv[i] = *esp;
		i++;
	}

	//STEP2. makes word_align
	char align = (size_t)*esp % 4;
	if(align != 0){
		*esp = *esp - align;
		memset(*esp, 0,  align);
	}

	//STEP3. NULL pointer push. (Why?)
	*esp -= sizeof(int);
	memset(*esp,0,sizeof(int));
	
	//STEP4. argv array value PUSH
	i--;
	for(i ; i >=0 ; i--){
		*esp = *esp- sizeof(int);
		memcpy(*esp,&argv[i],sizeof(int));
	} 

	//STEP5. remain part. argv,argc,return address push & free malloc if any
	int argv_addr = *esp;
	*esp -=  sizeof(int);
	memcpy(*esp,&argv_addr,sizeof(int));
	*esp -=  sizeof(int);
	memcpy(*esp,&argc,sizeof(int));
	
	int return_addr = 0 ;
	*esp -=  sizeof(int);
	memcpy(*esp,&return_addr, sizeof(int));


	free(argv);
	free(file_copy);

	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable){

				struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL//To check that upage is not already mapped
          && pagedir_set_page (t->pagedir, upage, kpage, writable));//from upage to kpage mapping insert into pagedir
}
