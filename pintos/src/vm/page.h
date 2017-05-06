#include <list.h>
#include <hash.h>
#include "threads/palloc.h"
#include "threads/thread.h"

struct sup_pte
{
				struct file * file;
				uint8_t * upage;
				uint32_t read_bytes;
				uint32_t zero_bytes;
				bool writable;
				struct hash_elem elem;
	
				off_t file_ofs;
				bool swapped; //initially false
				bool loaded;	//initially true
			  uint32_t swap_index;
};

void page_table_init(struct hash * page_table);
void page_table_remove(struct hash * page_table);

bool page_insert(struct file * file, off_t file_ofs, uint8_t * upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
struct sup_pte * get_sup_pte(struct hash * page_table, void * upage);
bool page_remove(struct hash * page_table, void * upage);
