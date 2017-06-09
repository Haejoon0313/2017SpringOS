#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "filesys/cache.h"
#include <list.h>
#include "threads/malloc.h"
#include "threads/thread.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  filesys_disk = disk_get (0, 1);
  if (filesys_disk == NULL)
    PANIC ("hd0:1 (hdb) not present, file system initialization failed");

  inode_init ();
  free_map_init ();
	cache_init();
	//list_init(&cache_list);

	thread_current()->dir = dir_open_root();
  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{	
	free_map_close ();
	destroy_cache_list();

}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{
	ASSERT(name != NULL);

	if(strlen(name) ==0)
					return false;

	char * temp_name;
	size_t input_len = strlen(name);

	temp_name = malloc(input_len + 1);
	strlcpy(temp_name, name, input_len + 1);



	char * temp_path = strrchr(temp_name, '/');
	char * open_path = NULL;
	char * target_name = NULL;
	struct dir * current_dir;
	struct inode * current_inode;
	disk_sector_t sector;


	bool result = false;



	if(temp_path == NULL){
					open_path= malloc(2);

					target_name = (char *) malloc(input_len + 1 );

					strlcpy(target_name, temp_name, input_len + 1);

					current_dir = dir_reopen(thread_current()->dir);

	}
	/*
	else if(temp_path == temp_name){
					
					open_path = (char *) malloc(2);

					//strlcpy(open_path, name, 1);
					open_path[0] = '/';
					open_path[1] = 0;


					target_name = (char *) malloc(input_len);

					strlcpy(target_name, temp_name + 1, input_len);

					current_dir = path_parsing(open_path);
	}

*/
	
	
	
	else{

					size_t path_len = temp_path - temp_name;
					open_path = (char *) malloc(path_len + 1);

					strlcpy(open_path, name, path_len + 1);

					size_t name_len = input_len - path_len;
					target_name = (char *) malloc(name_len + 1);

					strlcpy(target_name, temp_path + 1, name_len + 1);

					current_dir = path_parsing(open_path);
	}

	if(current_dir == NULL){
					result = false;
	}else{
					current_inode = dir_get_inode(current_dir);

					if(!dir_lookup(current_dir, target_name, &current_inode)){
									if(free_map_allocate(1, &sector)){
													if(is_dir){
																	result = dir_create(sector, 0);
													}else{
																	result = inode_create(sector, initial_size, false);
													}

													result &= dir_add(current_dir, target_name, sector);
									}
					}else{
									inode_close(current_inode);
					}

					dir_close(current_dir);
	}
	
	free(open_path);
	free(target_name);
	free(temp_name);

	return result;
}
													


/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
	ASSERT(name != NULL);
	char * temp_name;
	size_t input_len = strlen(name);

	if (input_len == 0)
					return NULL;

	temp_name = malloc(input_len + 1);
	strlcpy(temp_name, name, input_len + 1);

	char * temp_path = strrchr(temp_name, '/');
	char * open_path = NULL;
	char * target_name = NULL;
	struct dir * current_dir;
	struct inode * current_inode;


	if(temp_path == NULL){
					target_name = (char *) malloc(input_len + 1);

					strlcpy(target_name, temp_name, input_len + 1);

					current_dir = dir_reopen(thread_current()->dir);
	
	}else{
					size_t path_len = temp_path - temp_name;
					open_path = (char *) malloc(path_len + 1);

					strlcpy(open_path, name, path_len + 1);

					size_t name_len = input_len - path_len;
					target_name = (char *) malloc(name_len + 1);

					strlcpy(target_name, temp_path + 1, name_len + 1);

					current_dir = path_parsing(open_path);
	}

	if(current_dir == NULL){
					free(open_path);
					free(target_name);
					free(temp_name);
					return NULL;
	}else{
					if(target_name[0]==0){
									free(open_path);
									free(target_name);
									free(temp_name);
									return file_open(dir_get_inode(current_dir));
					}

					dir_lookup(current_dir, target_name, &current_inode);
					dir_close(current_dir);
					free(open_path);
					free(target_name);
					free(temp_name);


					return file_open(current_inode);
	}

}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  if(name == NULL){
					return false;
	}
	char * temp_name;
	size_t input_len = strlen(name);
	struct thread * curr = thread_current();

	if(input_len == 0)
					return NULL;

	temp_name = malloc(input_len + 1);
	strlcpy(temp_name, name, input_len + 1);

	char * temp_path = strrchr(temp_name, '/');
	char * open_path = NULL;
	char * target_name = NULL;
	struct dir * current_dir;
	struct inode * current_inode;
	disk_sector_t sector;
	bool result = false;

	if(temp_path == NULL){
					target_name = (char *) malloc(input_len + 1 );

					strlcpy(target_name, temp_name, input_len + 1);

					current_dir = dir_reopen(curr->dir);

	}else{
					size_t path_len = temp_path - temp_name;
					open_path = (char *) malloc(path_len + 1);

					strlcpy(open_path, name, path_len + 1);

					size_t name_len = input_len - path_len;
					target_name = (char *) malloc(name_len + 1);

					strlcpy(target_name, temp_path + 1, name_len + 1);

					current_dir = path_parsing(open_path);
	}

	if(current_dir == NULL){
					result = false;
	}else{
					dir_lookup(current_dir, target_name, &current_inode);

					if(current_inode == dir_get_inode(curr->dir)){
									result = false;
					}else{
									result = dir_remove(current_dir, target_name);
					}

					dir_close(current_dir);
					inode_close(current_inode);
	}

	free(open_path);
	free(target_name);
	free(temp_name);

	return result;

}




/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
