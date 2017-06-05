#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include <stdio.h>

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define DIRECT_CNT 64
#define INDIRECT_CNT 128
#define DOUBLY_INDIRECT_CNT (INDIRECT_CNT * INDIRECT_CNT)

/*To save previous project code */

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}


/*If we need to read POS position of file, which sector do we need to read?
 byte_to_sector translate the pos to sector. =>Which sector is indicated by byte.*/
/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);

	if(pos >= inode_length(inode)){
					
					//printf("@@@@@@@@@@@@@Byte to sector, wrong pos!\n	length : %d \n pos : %d \n",inode_length(inode),pos);
					return -1;
	}
	disk_sector_t return_sec;
	
	off_t sec_cnt;
	
	sec_cnt = pos/DISK_SECTOR_SIZE;
	
	struct inode_disk * disk_inode = NULL;
	disk_inode = calloc(1,sizeof * disk_inode);/// (struct inode_disk *)malloc(DISK_SECTOR_SIZE);
	ASSERT(disk_inode != NULL);
	
	cache_read(inode->sector, disk_inode,0, DISK_SECTOR_SIZE);

	/*return sector in direct case*/
	if(sec_cnt < DIRECT_CNT)
		return_sec = disk_inode->direct[sec_cnt];

	/*Indirect case*/
	else if (sec_cnt < DIRECT_CNT + INDIRECT_CNT){
		cache_read(disk_inode->indirect, (void *)&return_sec, (sec_cnt - DIRECT_CNT)*sizeof(disk_sector_t),sizeof(disk_sector_t));
	}
	
	else if(sec_cnt < DIRECT_CNT + INDIRECT_CNT + DOUBLY_INDIRECT_CNT){
		off_t sector_cnt = (sec_cnt - DIRECT_CNT - INDIRECT_CNT);//count of doubley indirect
		off_t doubly_indirect_off = sector_cnt / INDIRECT_CNT;
		off_t double_off = sector_cnt % INDIRECT_CNT;
		
		disk_sector_t indirect_sec_di;

		cache_read(disk_inode->doubly_indirect, (void *)&indirect_sec_di, doubly_indirect_off * sizeof(disk_sector_t),sizeof(disk_sector_t));
		cache_read(indirect_sec_di, (void *)&return_sec, double_off * sizeof(disk_sector_t),sizeof(disk_sector_t));
	}
	else
					printf("b_t_s error?\n");

	free(disk_inode);

	return return_sec;

}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}


/*Expand disk inode*/
bool inode_expand(struct inode * extend_inode, off_t extend_length)//LENGTH means real size to expand.
{	
	struct inode_disk * disk_inode = calloc (1, sizeof *disk_inode);

	ASSERT((disk_inode !=NULL)&&(extend_inode !=NULL));

	disk_sector_t new_indirect_in_double;

	char zeros[DISK_SECTOR_SIZE];	
	
	cache_read(extend_inode->sector, disk_inode, 0, DISK_SECTOR_SIZE);
	
	/*Data of current disk_inode length*/
	size_t exist_sectors = bytes_to_sectors(inode_length(extend_inode));//(disk_inode->length);

	/*How many sectors need to make newly*/
	off_t snip_byte = exist_sectors * DISK_SECTOR_SIZE -inode_length(extend_inode);// disk_inode->length;
	
	/*how many sectors are newly allocated*/	
	size_t sectors = bytes_to_sectors(extend_length-snip_byte);/*number of actually write sector*/
	
	/*current sector count*/
	size_t curr_sector = exist_sectors;
	
	/*remaining sector to newly locate. Initially, SECTORS remain.*/
	size_t remain_sectors = sectors;

	
	
	bool free_check;
	disk_sector_t alloc_sec;

	while(remain_sectors >0){
		if(curr_sector < DIRECT_CNT)
		{
						free_check = free_map_allocate(1,&alloc_sec);
					
						disk_inode->direct[curr_sector] = alloc_sec;
		}
		else if(curr_sector < DIRECT_CNT + INDIRECT_CNT)
		{
			if(curr_sector == DIRECT_CNT){
				if(!free_map_allocate(1, &disk_inode->indirect))
					return false;
				cache_write(disk_inode->indirect,zeros,0,DISK_SECTOR_SIZE);
			}
			free_check = free_map_allocate(1,&alloc_sec);
			cache_write(disk_inode->indirect, (void *)&alloc_sec,(curr_sector-DIRECT_CNT)*sizeof(disk_sector_t), sizeof(disk_sector_t));

		}
		else if(curr_sector < DIRECT_CNT +INDIRECT_CNT + DOUBLY_INDIRECT_CNT)
		{
			size_t double_cnt = curr_sector - DIRECT_CNT - INDIRECT_CNT;

			/*makes new doubly indirect blcok*/
			if(curr_sector == DIRECT_CNT + INDIRECT_CNT)
			{
				if(!free_map_allocate(1,&disk_inode->doubly_indirect))
					return false;
				cache_write(disk_inode->doubly_indirect,zeros,0,DISK_SECTOR_SIZE);
			}


			if(double_cnt%INDIRECT_CNT == 0){
				if(!free_map_allocate(1,&new_indirect_in_double))
					return false;
				cache_write(new_indirect_in_double,zeros,0,DISK_SECTOR_SIZE);
				cache_write(disk_inode->doubly_indirect,(void *)&new_indirect_in_double,(double_cnt/INDIRECT_CNT)*sizeof(disk_sector_t) ,sizeof(disk_sector_t));
			}

			free_map_allocate(1,&alloc_sec);
			disk_sector_t double_indirect_sec;

			cache_read(disk_inode->doubly_indirect, (void *)&double_indirect_sec,(double_cnt/INDIRECT_CNT)*sizeof(disk_sector_t)   ,sizeof(disk_sector_t));

			cache_write(double_indirect_sec, (void*)&alloc_sec, double_cnt%INDIRECT_CNT *sizeof(disk_sector_t),sizeof(disk_sector_t));

		}
		else
			printf("what this case is?\n");
	curr_sector++;
	remain_sectors--;

	}
	disk_inode->length += extend_length;
	cache_write(extend_inode->sector,(void *) disk_inode, 0, DISK_SECTOR_SIZE);
	free(disk_inode);
	return true;
}



/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);


  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  disk_inode = calloc (1, sizeof *disk_inode);
	ASSERT(disk_inode != NULL);
	ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

	
	disk_inode->length = 0;/*file initial size is set to 0*/
	disk_inode->magic = INODE_MAGIC;
	disk_inode->is_dir = is_dir;
	
	cache_write(sector,disk_inode,0,DISK_SECTOR_SIZE);

	struct inode* ex_inode = inode_open(sector);	
	lock_acquire(&ex_inode->lock);	
	success = inode_expand(ex_inode,length);
	lock_release(&ex_inode->lock);
	free(disk_inode);
	return success;
	
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) 
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode =calloc(1,sizeof * inode);//(struct slkfjsdfldk; malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
	lock_init(&inode->lock);
	inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;

//	disk_read (filesys_disk, inode->sector, &inode->data);/*read inode_sector and write it into &inode->data*/
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
	int j;
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

	struct inode_disk * disk_inode = NULL;
	disk_inode = calloc(1,sizeof * disk_inode);//(struct inode_disk *) malloc(sizeof (struct inode_disk));

	cache_read(inode->sector, disk_inode, 0, DISK_SECTOR_SIZE);


	uint32_t direct[DIRECT_CNT];
	uint32_t indirect[INDIRECT_CNT];
	uint32_t doubly[INDIRECT_CNT];
	uint32_t double_element[INDIRECT_CNT];

	size_t sector_cnt = bytes_to_sectors(disk_inode->length);
	size_t double_cnt, indirect_cnt, direct_cnt;
	off_t double_indirect_off, double_off;


  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
			lock_acquire(&inode->lock);			
						
			/* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
					/*doubly indirect sector free step*/
					if(sector_cnt > DIRECT_CNT + INDIRECT_CNT){
						double_cnt = sector_cnt - DIRECT_CNT - INDIRECT_CNT;

						cache_read(disk_inode->doubly_indirect, doubly, 0,DISK_SECTOR_SIZE);//now, doubly has T1 table.

						while(double_cnt >0){
							double_indirect_off = double_cnt/INDIRECT_CNT; //double_indirect의 첫번째 table offset										
							cache_read(disk_inode->doubly_indirect, double_element ,double_indirect_off ,DISK_SECTOR_SIZE);

							double_off = double_cnt%INDIRECT_CNT;

							for(j = 0; j<double_off ; j++){
								free_map_release(double_element[j],1);
							}
							double_cnt -= double_off;
							sector_cnt -= double_off;
							disk_inode->length = DISK_SECTOR_SIZE * (DIRECT_CNT + INDIRECT_CNT);					
						}
					}

					/*INDIRECT sector free case*/
					if(sector_cnt > DIRECT_CNT){
						indirect_cnt = sector_cnt - DIRECT_CNT;
						cache_read(disk_inode->indirect, indirect, 0 , DISK_SECTOR_SIZE);

						while(indirect_cnt >0)
						{
							free_map_release(indirect[indirect_cnt], 1);
							indirect_cnt--;
							sector_cnt--;
						}
						disk_inode->length = DISK_SECTOR_SIZE * DIRECT_CNT;
					}

					if(sector_cnt <= DIRECT_CNT){
						direct_cnt = sector_cnt;

						cache_read((disk_sector_t)disk_inode->direct,(void*) direct, 0, DISK_SECTOR_SIZE);
						while(direct_cnt >0){
							free_map_release(direct[direct_cnt],1);
							direct_cnt--;
							sector_cnt--;
						}
						disk_inode->length = 0;
					}
					
					cache_write(inode->sector, (void *)disk_inode, 0, DISK_SECTOR_SIZE);
					free_map_release(inode->sector,1);

        }
	lock_release(&inode->lock);	
	free (disk_inode);
	free (inode); 
    }

}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;//remain sector size(ex : 0.5sector).

      /* Bytes left in inode, bytes left in sector, lesser of the two. 전체 file과, 하나의 sector에서, 남은 부분(앞으로 읽을수도 있음) */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      size_t chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0){
							break;
			}
			
			cache_read(sector_idx, buffer + bytes_read, sector_ofs, chunk_size);
			struct cache * read_cache =  find_cache(sector_idx);
			ASSERT(read_cache != NULL);
			read_cache->open_cnt--;
			/*need read_ahead later??*/
			

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }


  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
//	uint8_t * bounce = NULL;	
	
	if(inode->deny_write_cnt != 0)
					return 0;
	
	off_t exist_length;
	exist_length = inode_length(inode);

	/*File extension case*/
	if(exist_length < offset + size){
					inode_expand(inode, offset + size - exist_length);
	}

	//printf("@@@@@@@@@@@%d %d \n",exist_length, offset);
  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

			
			/* Number of bytes to actually write into this sector. */
      size_t chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0){
	        break;
			}
		
			cache_write(sector_idx, (void *)buffer + bytes_written, sector_ofs  , chunk_size);
			struct cache* write_cache = find_cache(sector_idx);
			write_cache->open_cnt--;




      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }


  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
	off_t length;

//	struct inode_disk * disk_inode;
//	cache_read(inode->sector, disk_inode,0, DISK_SECTOR_SIZE);
//	length = (off_t*)& disk_inode->length;
	cache_read(inode->sector,(void *)&length, sizeof(disk_sector_t) ,sizeof(off_t));
	return length;
}
