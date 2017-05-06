#include "vm/frame.h"
#include <list.h>
#include <hash.h>
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/swap.h"

static unsigned page_hash(const struct hash_elem * e,void * aux UNUSED);
static bool page_less(const struct hash_elem * a, const struct hash_elem * b, void * aux UNUSED);
static void page_table_free(struct hash_elem * e, void * aux UNUSED);

void page_table_init(struct hash * page_table){
				hash_init(page_table, page_hash, page_less, NULL);
}

void page_table_remove(struct hash * page_table){
				hash_destroy(page_table, page_table_free);
}

bool page_insert(struct file * file, off_t file_ofs, uint8_t * upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable){
				struct sup_pte * sup_pte = (struct sup_pte *)malloc(sizeof(struct sup_pte));
//				struct thread * t = thread_current();

				sup_pte->file = file;
				sup_pte->upage = upage;
				sup_pte->read_bytes = read_bytes;
				sup_pte->zero_bytes = zero_bytes;
				sup_pte->writable=writable;
				sup_pte->file_ofs = file_ofs;
				sup_pte->swapped = false;
				sup_pte->loaded = true;

				if(hash_insert(&thread_current()->sup_page_table, &sup_pte->elem)==NULL){
								return true;
				}else{
								free(sup_pte);
								return false;
				}
}

struct sup_pte * get_sup_pte(struct hash * page_table, void * upage){
				struct sup_pte temp_sup_pte;
				struct hash_elem * e;

				temp_sup_pte.upage = upage;
				e = hash_find(page_table, &temp_sup_pte.elem);

				if(e == NULL){
								return NULL;
				}else{
								return hash_entry(e, struct sup_pte, elem);
				}
}

bool page_remove(struct hash * page_table, void * upage){
				struct sup_pte temp_sup_pte;
				struct hash_elem * e;

				temp_sup_pte.upage = upage;
				e = hash_delete(page_table, &temp_sup_pte.elem);

				if(e == NULL){
								return false;
				}else{
								return true;
				}
}

unsigned page_hash(const struct hash_elem * e, void * aux UNUSED){
				const struct sup_pte * sup_pte = hash_entry(e, struct sup_pte, elem);
				return hash_bytes(&sup_pte->upage, sizeof sup_pte->upage);
}

bool page_less(const struct hash_elem * a, const struct hash_elem * b, void * aux UNUSED){
				struct sup_pte * pte_a = hash_entry(a, struct sup_pte, elem);
				struct sup_pte * pte_b = hash_entry(b, struct sup_pte, elem);

				return pte_a->upage < pte_b->upage;
}

static void page_table_free(struct hash_elem * e, void * aux UNUSED){
				struct sup_pte * sup_pte = hash_entry(e, struct sup_pte, elem);
				struct thread * t = thread_current();

				if(sup_pte->loaded == true){
								frame_free(pagedir_get_page(t->pagedir, sup_pte->upage));
								pagedir_clear_page(t->pagedir, sup_pte->upage);
				}
				free(sup_pte);
}
