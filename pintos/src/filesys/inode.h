#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/disk.h"
#include <list.h>
#include "threads/synch.h"
struct bitmap;


struct inode_disk
{
				disk_sector_t start;
				off_t length;
				unsigned magic;
				
				disk_sector_t direct[64];
				disk_sector_t indirect;
				disk_sector_t doubly_indirect;

				uint32_t unused[59];
};

struct inode{
				struct list_elem elem;	/*element in inode list*/
				disk_sector_t sector;		/*sector # of disk location*/
				int open_cnt;						/*Number of openers*/
				bool removed;						/*True if deleted*/
				int deny_write_cnt;			/*0:writes ok, >0 deny write*/
				struct inode_disk data; /*Inode contents*/
				struct lock lock;
};




void inode_init (void);
bool inode_create (disk_sector_t, off_t);
struct inode *inode_open (disk_sector_t);
struct inode *inode_reopen (struct inode *);
disk_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

bool inode_expand(struct inode * extend_inode, off_t extend_length);
#endif /* filesys/inode.h */
