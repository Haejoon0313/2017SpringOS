#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void check_address (void * addr);
void pop_args(struct intr_frame *f,int *args, int argc);
void exit_process(int status);
#endif /* userprog/syscall.h */
