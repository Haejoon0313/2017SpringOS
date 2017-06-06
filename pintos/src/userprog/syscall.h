#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void check_address (void * addr);

typedef int mapid_t;
#define MAP_FAILED ((mapid_t) -1)


void exit_process(int status);
void close_all_filelist(struct list * file_list);
#endif /* userprog/syscall.h */
