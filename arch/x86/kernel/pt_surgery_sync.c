#include <linux/syscalls.h>
#include <linux/export.h>
#include <asm/current.h> 
#include <asm/page.h>
#include "pt_surgery.h"

#ifdef EMULATE_EMES_FOR_PTE
#include <linux/random.h>
#endif

static void modify_ds_log_flag(struct ds_log *dnode, struct ds_log *new, struct page *page)
{
	struct ds_log *next;

	if(dnode->base == new->base && dnode->limit == new->limit) {
		dnode->flag = new->flag;
		if(list_is_first(&dnode->list, &page->ds_head)) {
			ds_log_node_merge_next(dnode);
		}
		else if(list_is_last(&dnode->list, &page->ds_head)) {
			ds_log_node_merge_prev(dnode);
		}
		else {
			ds_log_node_merge_both(dnode);
		}	
	}
	else if(dnode->base == new->base) {
		ds_log_node_split_head(dnode, new);
		if(!list_is_first(&new->list, &page->ds_head))
			ds_log_node_merge_prev(new);
	}
	else if(dnode->limit == new->limit) {
		ds_log_node_split_tail(dnode, new);
		if(!list_is_last(&new->list, &page->ds_head))
			ds_log_node_merge_next(new);
	}
	else {
		if((next = init_ds_log_node(new->limit, dnode->limit, dnode->offset, dnode->flag)) == NULL)
			return;
		ds_log_node_split_middle(dnode, new, next);
	}
	return;
}

static void modify_ds_log_offset(struct ds_log *dnode, struct ds_log *new, struct page *page)
{
	struct ds_log *next;

	if(dnode->base == new->base && dnode->limit == new->limit) {
		dnode->offset = new->offset;
		dnode->flag = new->flag;
		if(list_is_first(&dnode->list, &page->ds_head)) {
			ds_log_node_merge_next(dnode);
		}
		else if(list_is_last(&dnode->list, &page->ds_head)) {
			ds_log_node_merge_prev(dnode);
		}
		else {
			ds_log_node_merge_both(dnode);
		}	
	}
	else if(dnode->base == new->base) {
		ds_log_node_split_head(dnode, new);
		if(!list_is_first(&new->list, &page->ds_head))
			ds_log_node_merge_prev(new);
	}
	else if(dnode->limit == new->limit) {
		ds_log_node_split_tail(dnode, new);
		if(!list_is_last(&new->list, &page->ds_head))
			ds_log_node_merge_next(new);
	}
	else {
		if((next = init_ds_log_node(new->limit, dnode->limit, dnode->offset, dnode->flag)) == NULL)
			return;
		ds_log_node_split_middle(dnode, new, next);
	}
	return;
}

static void delete_ds_log(struct ds_log *dnode, struct ds_log *new)
{
	if(dnode->base == new->base && dnode->limit == new->limit) {
		list_del(&dnode->list);
		kfree(dnode);
	}
	else if(dnode->base == new->base) {
		dnode->base++;
	}
	else if(dnode->limit == new->limit) {
		dnode->limit--;
	}
	else {
		unsigned long ds_limit = dnode->limit;
		dnode->limit = new->base;
		new->base = new->limit;
		new->limit = ds_limit;
		new->offset = dnode->offset;
		new->flag = dnode->flag;
		list_add(&new->list, &dnode->list);
	}
	return;
}


static void sync_pte_replica(unsigned long addr, pte_t *ptep, pte_t pte)
{
	pte_t *target_pte = ptep + (addr & PT_OFFSET_MASK);
	set_pte_recovery(target_pte, pte);
}

static int __make_pte_ds_log_usr(pte_t *ptep, pte_t pte, pid_t pid)
{
	struct ds_log *dnode, *prev;
	struct m_head_struct *mhead;
	struct page *pte_page;
	unsigned long pte_value = pte_pfn(pte);
	unsigned long pte_flag = pte_flags(pte);
	unsigned long base = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == pid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK)); /* get pte page */
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK)) 
				return 0;

			base = make_ds_base_from_pte((unsigned long)ptep, pte_page->m_log->base);
			if ((dnode = init_ds_log_node(base, base+1, make_ds_offset(base, pte_value), pte_flag)) == NULL) 
				return -1;

			spin_lock(&pte_page->m_log->recovery_lock);
			if (pte_page->m_log->replica)
				sync_pte_replica(base, pte_page->m_log->replica, pte);
			spin_unlock(&pte_page->m_log->recovery_lock);

			spin_lock(&pte_page->ds_lock);
			if (list_empty(&pte_page->ds_head)) {
				list_add(&dnode->list, &pte_page->ds_head);
			}
			else {
				list_for_each_entry_reverse(prev, &pte_page->ds_head, list) {
#ifdef EMULATE_EMES_FOR_PTE
					unsigned int emes;
					spin_lock(&pte_page->m_log->recovery_lock);
					if (pte_page->m_log->replica) {
						get_random_bytes(&emes, sizeof(emes));
						if (emes % FAULT_RATIO == 0) {
							printk(KERN_INFO "KERN RECOVERY ERROR: EMEs in ds_log\n");
							delete_broken_pte_log_all(pte_page->m_log);
							pte_free(mhead->mm, virt_to_page(pte_page->m_log->replica));
							pte_page->m_log->replica = NULL;
						}
					}
					spin_unlock(&pte_page->m_log->recovery_lock);
#endif
					if (prev->base <= dnode->base && dnode->limit <= prev->limit) {
						if (pte_value == 0 && pte_flag == 0) { /* delete ds_log */
							delete_ds_log(prev, dnode);
						}
						else if (dnode->offset != prev->offset) { /* modify ds_log offset */
							modify_ds_log_offset(prev, dnode, pte_page);
						}
						else if(ds_log_flag_diff(dnode->flag, prev->flag)) { /* modify ds_log flag */
							modify_ds_log_flag(prev, dnode, pte_page);
						}
						spin_unlock(&pte_page->ds_lock);
						goto end;
					}
					else if(dnode->base >= prev->limit) { /* create new pte */
						list_add(&dnode->list, &prev->list);
						if(list_is_last(&dnode->list, &pte_page->ds_head)) {
							ds_log_node_merge_prev(dnode);
							spin_unlock(&pte_page->ds_lock);
							goto end;
						}
						ds_log_node_merge_both(dnode);
						spin_unlock(&pte_page->ds_lock);
						goto end;
					}
				}
				list_add(&dnode->list, &pte_page->ds_head); /* create new pte that is top of pt */
				ds_log_node_merge_next(dnode);
			}
			spin_unlock(&pte_page->ds_lock);
			break;
		}
	}
end:
	return 0;
}

int make_pte_ds_log_usr(pte_t *ptep, pte_t pte)
{
	return __make_pte_ds_log_usr(ptep, pte, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pte_ds_log_usr);

int make_pte_ds_log_usr_pid(pte_t *ptep, pte_t pte, pid_t pid)
{
	return __make_pte_ds_log_usr(ptep, pte, pid);
}
EXPORT_SYMBOL_GPL(make_pte_ds_log_usr_pid);

static void clear_wrbit_ds_flag(struct ds_log *dnode, unsigned long base, unsigned long limit, struct page *page)
{
	struct ds_log *next, *prev;
	if(dnode->base == base && dnode->limit == limit) {
		dnode->flag &= _PAGE_RW_NOT;
		if(list_is_first(&dnode->list, &page->ds_head)) {
			ds_log_node_merge_next(dnode);
		}
		else if(list_is_last(&dnode->list, &page->ds_head)) {
			ds_log_node_merge_prev(dnode);
		}
		else {
			ds_log_node_merge_both(dnode);
		}	
	}
	else if(dnode->base == base) {
		if((next = init_ds_log_node(base, limit, dnode->offset, dnode->flag & _PAGE_RW_NOT)) == NULL)
			return;
		ds_log_node_split_head(dnode, next);
		if(!list_is_first(&next->list, &page->ds_head))
			ds_log_node_merge_prev(next);
	}
	else if(dnode->limit == limit) {
		if((prev = init_ds_log_node(base, limit, dnode->offset, dnode->flag & _PAGE_RW_NOT)) == NULL)
			return;
		ds_log_node_split_tail(dnode, prev);
		if(!list_is_last(&prev->list, &page->ds_head))
			ds_log_node_merge_next(prev);
	}
	else {
		if((prev = init_ds_log_node(base, limit, dnode->offset, dnode->flag & _PAGE_RW_NOT)) == NULL)
			return;
		if((next = init_ds_log_node(limit, dnode->limit, dnode->offset, dnode->flag)) == NULL)
			return;
		ds_log_node_split_middle(dnode, prev, next);
	}
	return;
}

static void sync_wrbit_pte_replica(unsigned long base, pte_t *ptep)
{
	pte_t *target_pte = ptep + (base & PT_OFFSET_MASK);
	clear_bit(_PAGE_BIT_RW, (unsigned long *)&target_pte->pte);
}

int clear_wrbit_ds_log(pte_t *ptep)
{
	struct page *pte_page;
	struct ds_log *prev;
	struct m_head_struct *mhead;
	unsigned long base = 0;
	unsigned long limit = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
				return 0;

			base = make_ds_base_from_pte((unsigned long)ptep, pte_page->m_log->base);
			limit = base + 1;

			spin_lock(&pte_page->m_log->recovery_lock);
			if (pte_page->m_log->replica)
				sync_wrbit_pte_replica(base, pte_page->m_log->replica);
			spin_unlock(&pte_page->m_log->recovery_lock);

			spin_lock(&pte_page->ds_lock);
			list_for_each_entry_reverse(prev, &pte_page->ds_head, list) {
#ifdef EMULATE_EMES_FOR_PTE
				unsigned int emes;
				spin_lock(&pte_page->m_log->recovery_lock);
				if (pte_page->m_log->replica) {
					get_random_bytes(&emes, sizeof(emes));
					if (emes % FAULT_RATIO == 0) {
						printk(KERN_INFO "KERN RECOVERY ERROR: EMEs in ds_log\n");
						delete_broken_pte_log_all(pte_page->m_log);
						pte_free(mhead->mm, virt_to_page(pte_page->m_log->replica));
						pte_page->m_log->replica = NULL;
					}
				}
				spin_unlock(&pte_page->m_log->recovery_lock);
#endif
				if (prev->base <= base && limit <= prev->limit) {
					if (ds_log_rw_diff(prev)) {
						clear_wrbit_ds_flag(prev, base, limit, pte_page);
					}
					break;
				}
			}
			spin_unlock(&pte_page->ds_lock);
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(clear_wrbit_ds_log);
