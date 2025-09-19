#include <linux/syscalls.h> //
// #include <linux/printk.h>
// #include <linux/types.h>
// #include <linux/mm.h>
// #include <linux/rwlock.h> // 
#include <linux/spinlock_types.h>
#include <linux/export.h>
#include <linux/delay.h> //
#include <linux/kthread.h> //
#include <linux/preempt.h> //
#include <linux/pid.h>
#include <linux/hugetlb.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <linux/oom.h>
#include <linux/random.h>
#include <linux/rcupdate.h>
#include <linux/page_ref.h>
#include <asm/tlbflush.h>
// #include <linux/list.h> //
// #include <linux/slab.h> //
#include <linux/err.h> //
#include <asm/current.h> 
#include "ds.h" //
// #include "ds_struct.h" //
#include <asm/page.h>
// #include <asm/pgtable.h> //
// #include <asm/pgalloc.h>
// #include <asm/paravirt.h> //

LIST_HEAD(user_head);
// LIST_HEAD(kern_head);
LIST_HEAD(count_head);
// DEFINE_SPINLOCK(user_lock);

static int make_recovery_thread(struct m_head_struct *mhead, struct page *pte_page);
static int recover_broken_pgtable(struct mm_struct *mm, struct m_head_struct *mhead, struct page *pte_page);

static int __make_pgd_m_log(pgd_t *pgd, pid_t pid)
{
	struct page *pgd_page;
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == pid) {
			pgd_page = virt_to_page((pgd_t *)(((unsigned long)pgd) & PAGE_MASK));	/* get pgd page */
			if (!pgd_page->m_log) 
				if (init_page_for_pt_surgery(pgd_page) < 0)
					return -1;

			init_m_log(pgd_page->m_log, PGD_FLAG_MASK);	/* update m_log in pgd page */
			// printk(KERN_INFO "make m pgd alloc %lx, %lx, %d, %d\n", (((unsigned long)pgd) & PAGE_MASK), PGD_FLAG_MASK, current->pid, current->tgid);
		}
	}
	return 0;
}

int make_pgd_m_log(pgd_t *pgd)
{
	return __make_pgd_m_log(pgd, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pgd_m_log);

static int __make_pud_m_log(p4d_t *p4d, pud_t *pud, pid_t pid)
{
	struct page *p4d_page;
	struct page *pud_page;
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == pid) {
			p4d_page = virt_to_page((p4d_t *)(((unsigned long)p4d) & PAGE_MASK)); /* get p4d page */
			pud_page = virt_to_page((pud_t *)(((unsigned long)pud) & PAGE_MASK)); /* get pud page */
			if(!p4d_page->m_log || !(p4d_page->m_log->base & PGD_FLAG_MASK))
				break;

			if (!pud_page->m_log) 
				if (init_page_for_pt_surgery(pud_page) < 0)
					return -1;

			init_m_log(pud_page->m_log, make_ds_base_from_p4d((unsigned long)p4d));	/* update m_log in pud page */
			// printk(KERN_INFO "make m pud alloc %lx, %lx, %d, %d\n", (((unsigned long)pud) & PAGE_MASK), pud_page->m_log->base, current->pid, current->tgid);
		}
	}
	return 0;
}

int make_pud_m_log(p4d_t *p4d, pud_t *pud)
{
	return __make_pud_m_log(p4d, pud, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pud_m_log);

static int __make_pmd_m_log(pud_t *pud, pmd_t *pmd, pid_t pid)
{
	struct page *pud_page;
	struct page *pmd_page;
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == pid) {
			pud_page = virt_to_page((pud_t *)(((unsigned long)pud) & PAGE_MASK)); /* get pud page */
			pmd_page = virt_to_page((pmd_t *)(((unsigned long)pmd) & PAGE_MASK)); /* get pmd page */
			if (!pud_page->m_log || !(pud_page->m_log->base & PUD_FLAG_MASK))
				break;

			if (!pmd_page->m_log) 
				if (init_page_for_pt_surgery(pmd_page) < 0)
					return -1;

			init_m_log(pmd_page->m_log, make_ds_base_from_pud((unsigned long)pud, pud_page->m_log->base));	/* update m_log in pmd page */
			// printk(KERN_INFO "make m pmd alloc %lx, %lx, %d, %d\n", (((unsigned long)pmd) & PAGE_MASK), pmd_page->m_log->base, current->pid, current->tgid);
		}
	}
	return 0;
}

int make_pmd_m_log(pud_t *pud, pmd_t *pmd)
{
	return __make_pmd_m_log(pud, pmd, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pmd_m_log);

static int __make_pte_m_log(pmd_t *pmd, pte_t *pte, pid_t pid)
{
	struct page *pmd_page;
	struct page *pte_page;
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == pid) {
			pmd_page = virt_to_page((pmd_t *)(((unsigned long)pmd) & PAGE_MASK)); /* get pmd page */
			pte_page = virt_to_page((pte_t *)(((unsigned long)pte) & PAGE_MASK)); /* get pte page */
			if (!pmd_page->m_log || !(pmd_page->m_log->base & PMD_FLAG_MASK))
				break;

			if (!pte_page->m_log) 
				if (init_page_for_pt_surgery(pte_page) < 0)
					return -1;

			init_m_log(pte_page->m_log, make_ds_base_from_pmd((unsigned long)pmd, pmd_page->m_log->base));	/* update m_log in pte page */
			// printk(KERN_INFO "make m pte alloc %lx, %lx, %d, %d\n", (((unsigned long)pte) & PAGE_MASK), pte_page->m_log->base, current->pid, current->tgid);
		}
	}
	return 0;
}

int make_pte_m_log(pmd_t *pmd, pte_t *pte) 
{
	return __make_pte_m_log(pmd, pte, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pte_m_log);

static void modify_ds_flag(struct ds_log *ds_node, struct ds_log *new, struct page *page)
{
	struct ds_log *next, *prev;

	if(ds_node->base == new->base && ds_node->limit == new->limit) {
		ds_node->flag = new->flag;
		if(list_is_first(&ds_node->list, &page->ds_head)) {
			next = list_next_entry(ds_node, list);
			ds_node_merge(ds_node, next);
		}
		else if(list_is_last(&ds_node->list, &page->ds_head)) {
			prev = list_prev_entry(ds_node, list);
			ds_node_merge(prev, ds_node);
		}
		else {
			next = list_next_entry(ds_node, list);
			prev = list_prev_entry(ds_node, list);
			ds_node_merge(ds_node, next);
			ds_node_merge(prev, ds_node);
		}	
	}
	else if(ds_node->base == new->base) {
		ds_node->base++;
		list_add_tail(&new->list, &ds_node->list);
		if(!list_is_first(&new->list, &page->ds_head)) {
			prev = list_prev_entry(new, list);
			ds_node_merge(prev, new);
		}
	}
	else if(ds_node->limit == new->limit) {
		ds_node->limit--;
		list_add(&new->list, &ds_node->list);
		if(!list_is_last(&new->list, &page->ds_head)) {
			next = list_next_entry(new, list);
			ds_node_merge(new, next);
		}
	}
	else {
		if((next = make_ds_log_node(new->limit, ds_node->limit, ds_node->offset, ds_node->flag)) == NULL)
			return;
		ds_node->limit = new->base;
		list_add(&new->list, &ds_node->list);
		list_add(&next->list, &new->list);
	}
	return;
}

static void modify_ds_offset(struct ds_log *ds_node, struct ds_log *new, struct page *page)
{
	struct ds_log *next, *prev;

	if(ds_node->base == new->base && ds_node->limit == new->limit) {
		ds_node->offset = new->offset;
		ds_node->flag = new->flag;
		if(list_is_first(&ds_node->list, &page->ds_head)) {
			next = list_next_entry(ds_node, list);
			ds_node_merge(ds_node, next);
		}
		else if(list_is_last(&ds_node->list, &page->ds_head)) {
			prev = list_prev_entry(ds_node, list);
			ds_node_merge(prev, ds_node);
		}
		else {
			next = list_next_entry(ds_node, list);
			prev = list_prev_entry(ds_node, list);
			ds_node_merge(ds_node, next);
			ds_node_merge(prev, ds_node);
		}	
	}
	else if(ds_node->base == new->base) {
		ds_node->base++;
		list_add_tail(&new->list, &ds_node->list);
		if(!list_is_first(&new->list, &page->ds_head)) {
			prev = list_prev_entry(new, list);
			ds_node_merge(prev, new);
		}
	}
	else if(ds_node->limit == new->limit) {
		ds_node->limit--;
		list_add(&new->list, &ds_node->list);
		if(!list_is_last(&new->list, &page->ds_head)) {
			next = list_next_entry(new, list);
			ds_node_merge(new, next);
		}
	}
	else {
		if((next = make_ds_log_node(new->limit, ds_node->limit, ds_node->offset, ds_node->flag)) == NULL)
			return;

		ds_node->limit = new->base;
		list_add(&new->list, &ds_node->list);
		list_add(&next->list, &new->list);
	}
	return;
}

static void delete_ds(struct ds_log *ds_node, struct ds_log *new)
{
	if(ds_node->base == new->base && ds_node->limit == new->limit) {
		list_del(&ds_node->list);
		kfree(ds_node);
	}
	else if(ds_node->base == new->base) {
		ds_node->base++;
	}
	else if(ds_node->limit == new->limit) {
		ds_node->limit--;
	}
	else {
		unsigned long ds_limit = ds_node->limit;
		ds_node->limit = new->base;
		new->base = new->limit;
		new->limit = ds_limit;
		new->offset = ds_node->offset;
		new->flag = ds_node->flag;
		list_add(&new->list, &ds_node->list);
	}
	return;
}


static void update_pte_replica(unsigned long addr, pte_t *ptep, pte_t pte)
{
	pte_t *target_pte = ptep + (addr & PT_PGTABLE_MASK);
	set_pte_recover(target_pte, pte);
	// printk(KERN_INFO "update replica %lx %lx %lx", addr, pte_pfn(*target_pte), pte_flags(*target_pte));
}

static int __make_pte_ds_log_usr(pte_t *ptep, pte_t pte, pid_t pid)
{
	struct ds_log *dnode, *next, *prev;
	struct m_head_struct *mhead;
	struct page *pte_page;
	unsigned long pte_value = pte_pfn(pte);
	unsigned long pte_flag = pte_flags(pte);
	unsigned long base = 0;
	// int flag = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == pid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK)); /* get pte page */
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK)) 
				return 0;

			base = make_ds_base_from_pte((unsigned long)ptep, pte_page->m_log->base);
			if ((dnode = make_ds_log_node(base, base+1, make_ds_offset(base, pte_value), pte_flag)) == NULL) 
				return -1;

			spin_lock(&pte_page->m_log->recovery_lock);
			if (pte_page->m_log->replica)
				update_pte_replica(base, pte_page->m_log->replica, pte);
			spin_unlock(&pte_page->m_log->recovery_lock);

			spin_lock(&pte_page->ds_lock);
			if (list_empty(&pte_page->ds_head)) {
				list_add(&dnode->list, &pte_page->ds_head);
				// printk(KERN_INFO "make ds %lx %lx %lx %d\n", base, pte_value, pte_flag, pid);
			}
			else {
				list_for_each_entry_reverse(prev, &pte_page->ds_head, list) {
// #ifdef CONFIG_RECOVERY_COUNT
// 						unsigned int emes;
// 						if (pte_page->dup_pt && flag == 0) {
// 							get_random_bytes(&emes, sizeof(emes));
// 							if (emes % DIVISION_NUM == 0) {
// 								printk(KERN_INFO "RECOVERY ERROR: detect EMEs at ds_log %lx in pte update %lx-%lx\n", (unsigned long)prev, pte_page->base, (unsigned long)pte_page->dup_pt);
// 								flag = 1;
// 							}
// 						}
// #endif
					if (prev->base <= dnode->base && dnode->limit <= prev->limit) {
						if (pte_value == 0 && pte_flag == 0) { /* delete ds_log */
							delete_ds(prev, dnode);
							// printk(KERN_INFO "delete ds %lx-%lx\n", dnode->base, dnode->limit);
						}
						else if (dnode->offset != prev->offset) { /* modify ds_log offset */
							modify_ds_offset(prev, dnode, pte_page);
							// printk(KERN_INFO "modify ds offset %lx %lx %lx %d\n", base, pte_value, pte_flag, pid);
						}
						else if(ds_log_flag_diff(dnode->flag, prev->flag)) { /* modify ds_log flag */
							modify_ds_flag(prev, dnode, pte_page);
							// printk(KERN_INFO "modify ds flag %lx %lx %lx %d\n", base, pte_value, pte_flag, pid);

							// if(!ds_log_rw_diff(prev) && ds_log_rw_diff(dnode)) { /* ds_log mkwrite */
							// 	// printk(KERN_INFO "make write %lx %lx-%lx", base, prev->base, prev->limit);
							// 	modify_ds_flag(prev, dnode, pte_page);
							// 	// printk(KERN_INFO "make write %lx %lx %d\n", base, pte_flag, pid);
							// }
							// else if(ds_log_rw_diff(prev) && !ds_log_rw_diff(dnode)) {	/* ds_log wrprotect */
							// 	// printk(KERN_INFO "make wrprotect %lx %lx-%lx", base, prev->base, prev->limit);
							// 	modify_ds_flag(prev, dnode, pte_page);
							// 	// printk(KERN_INFO "make wrprotect %lx %lx %d\n", base, pte_flag, pid);
							// }
							// else {
							// 	// printk(KERN_INFO "not modify ds flag %lx  %lx->%lx %d\n", base, prev->flag, pte_flag, pid);
							// }
						}
						spin_unlock(&pte_page->ds_lock);
						goto end;
					}
					else if(dnode->base >= prev->limit) { /* create new pte */
						list_add(&dnode->list, &prev->list);
						// printk(KERN_INFO "make ds %lx %lx %lx %d\n", base, pte_value, pte_flag, pid);
						if(list_is_last(&dnode->list, &pte_page->ds_head)) {
							ds_node_merge(prev, dnode);
							spin_unlock(&pte_page->ds_lock);
							goto end;
						}
						next = list_next_entry(dnode, list);
						ds_node_merge(dnode, next);
						ds_node_merge(prev, dnode);
						spin_unlock(&pte_page->ds_lock);
						goto end;
					}
				}
				list_add(&dnode->list, &pte_page->ds_head); /* create new pte that is top of pt */
				// printk(KERN_INFO "make ds %lx %lx %lx %d\n", base, pte_value, pte_flag, pid);
				next = list_next_entry(dnode, list);
				ds_node_merge(dnode, next);
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

static void clear_wrbit_ds_flag(struct ds_log *ds_node, unsigned long base, unsigned long limit, struct page *page)
{
	struct ds_log *next, *prev;
	if(ds_node->base == base && ds_node->limit == limit) {
		ds_node->flag &= _PAGE_RW_NOT;
		// printk(KERN_INFO "make wrprotect %lx %lx %d\n", base, ds_node->flag, current->tgid);
		if(list_is_first(&ds_node->list, &page->ds_head)) {
			next = list_next_entry(ds_node, list);
			ds_node_merge(ds_node, next);
		}
		else if(list_is_last(&ds_node->list, &page->ds_head)) {
			prev = list_prev_entry(ds_node, list);
			ds_node_merge(prev, ds_node);
		}
		else {
			next = list_next_entry(ds_node, list);
			prev = list_prev_entry(ds_node, list);
			ds_node_merge(ds_node, next);
			ds_node_merge(prev, ds_node);
		}	
	}
	else if(ds_node->base == base) {
		ds_node->base++;
		if((next = make_ds_log_node(base, limit, ds_node->offset, ds_node->flag & _PAGE_RW_NOT)) == NULL)
			return;
		// printk(KERN_INFO "make wrprotect %lx %lx %d\n", base, next->flag, current->tgid);
		list_add_tail(&next->list, &ds_node->list);
		if(!list_is_first(&next->list, &page->ds_head)) {
			prev = list_prev_entry(next, list);
			ds_node_merge(prev, next);
		}
	}
	else if(ds_node->limit == limit) {
		ds_node->limit--;
		if((prev = make_ds_log_node(base, limit, ds_node->offset, ds_node->flag & _PAGE_RW_NOT)) == NULL)
			return;
		// printk(KERN_INFO "make wrprotect %lx %lx %d\n", base, prev->flag, current->tgid);
		list_add(&prev->list, &ds_node->list);
		if(!list_is_last(&prev->list, &page->ds_head)) {
			next = list_next_entry(prev, list);
			ds_node_merge(prev, next);
		}
	}
	else {
		if((prev = make_ds_log_node(base, limit, ds_node->offset, ds_node->flag & _PAGE_RW_NOT)) == NULL)
			return;
		if((next = make_ds_log_node(limit, ds_node->limit, ds_node->offset, ds_node->flag)) == NULL)
			return;
		// printk(KERN_INFO "make wrprotect %lx %lx %d\n", base, prev->flag, current->tgid);
		ds_node->limit = base;
		list_add(&prev->list, &ds_node->list);
		list_add(&next->list, &prev->list);
	}
	return;
}

static void clear_wrbit_pte_replica(unsigned long base, pte_t *ptep)
{
	pte_t *target_pte = ptep + (base & PT_PGTABLE_MASK);
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
				clear_wrbit_pte_replica(base, pte_page->m_log->replica);
			spin_unlock(&pte_page->m_log->recovery_lock);

			spin_lock(&pte_page->ds_lock);
			list_for_each_entry_reverse(prev, &pte_page->ds_head, list) {
				if (prev->base <= base && limit <= prev->limit) {
					if (ds_log_rw_diff(prev)) {
						clear_wrbit_ds_flag(prev, base, limit, pte_page);
					}
					break;
				}
			}
			spin_unlock(&pte_page->ds_lock);
			// printk(KERN_INFO "  pte pfn %lx %lx\n", pte_pfn(*ptep), pte_flags(*ptep));
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(clear_wrbit_ds_log);

static int get_pmdp_for_recover_pgtable(struct mm_struct *mm, unsigned long base, pmd_t **pmdpp)
{
	p4d_t *p4dp;
	pud_t *pudp;

	unsigned long pgd = (base >> 27) & PT_PGTABLE_MASK;
	unsigned long pud = (base >> 18) & PT_PGTABLE_MASK;
	unsigned long pmd = (base >> 9) & PT_PGTABLE_MASK;

  	if (get_pgdp(mm, pgd, &p4dp) == 0) {
		if (get_pudp(p4dp, pud, &pudp) == 0) {
			if (get_pmdp(pudp, pmd, pmdpp) == 0) {
				return 0;
			}
		}
	}
	return -1;
}

static struct page *get_page_from_addr(struct mm_struct *mm, unsigned long base) 
{
	pmd_t *pmdp;

	if(get_pmdp_for_recover_pgtable(mm, base, &pmdp) < 0)
		return NULL;

	return pmd_page(*pmdp);
}


// int register_broken_pte_and_recover_broken_pgtable(unsigned long pte_va)
// {
// 	struct page *pte_page;
// 	struct m_head_struct *mhead;
// 	struct broken_pte_log *bnode;
// 	unsigned long target_base = 0;
// 	pte_t *ptep_new;
// 	int ret = 0;

// 	list_for_each_entry(mhead, &user_head, list) {
// 		if (mhead->pid == current->tgid) {
// 			pte_page = virt_to_page((pte_t *)(pte_va & PAGE_MASK));
// 			if (pte_page->base & PTE_FLAG_MASK)
// 				target_base = make_ds_base((pte_page->base >> 27) & PT_PGTABLE_MASK, (pte_page->base >> 18) & PT_PGTABLE_MASK, (pte_page->base >> 9) & PT_PGTABLE_MASK, ((pte_va & OFFSET_MASK) / 0x8) & PT_PGTABLE_MASK);

// 			if (pte_page->base != 0 && target_base != 0) {
// 				list_for_each_entry(bnode, &mhead->head, list) {
// 					if (target_base == bnode->base) {
// 						printk(KERN_INFO "Have already registered the broken pte %lx\n", target_base);
// 						goto end;
// 					}
// 				}

// 				if (add_broken_pte_node(target_base, mhead) < 0) {
// 					ret = -1;
// 					goto end;
// 				}
// 				// printk(KERN_INFO "register broken pte %lx %lx %d\n", pte_page->base, target_base, mhead->pid);
				
// 				if (!pte_page->dup_pt) {
// 					ptep_new = pte_realloc(mhead->mm);
// 					if (!ptep_new) {
// 						printk(KERN_INFO "out of memory\n");
// 						ret = -1;
// 						goto end;
// 					}
// 					// printk(KERN_INFO "duplicate pte %lx", (unsigned long)__pa(ptep_new));
// 					if (restore_replica(target_base & PT_PGTABLE_MASK_NOT, ptep_new, pte_page) < 0) {
// 						list_for_each_entry(bnode, &mhead->head, list) {
// 							if (target_base == bnode->base) {
// 								pte_free(mhead->mm, virt_to_page(ptep_new));
// 								list_del(&bnode->list);
// 								kfree(bnode);
// 								ret = -1;
// 								goto end;
// 							}
// 						}
// 					}
// 					pte_page->dup_pt = ptep_new;

// 					// check ref count 
// 					if (recover_broken_pgtable(mhead->mm, mhead, pte_page) < 0)
// 						ret = -1;
// 					goto end;
// 				}
// 			}
// 			break;
// 		}
// 	}
// end:
// 	return ret;
// }
// EXPORT_SYMBOL_GPL(register_broken_pte_and_recover_broken_pgtable);

int register_broken_pte_and_make_recovery_thread(unsigned long pte_va)
{
	struct page *pte_page;
	struct m_head_struct *mhead;
	unsigned long target_base = 0;
	pte_t *ptep_new;
	int ret = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(pte_va & PAGE_MASK));
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
				return 0;

			target_base = make_ds_base_from_pte(pte_va, pte_page->m_log->base);
			if (is_broken_pte_node_registered(target_base, pte_page->m_log) > 0)
				return 0;
			
			if (add_broken_pte_node(target_base, pte_page->m_log) < 0) 
				return 0;
			printk(KERN_INFO "register broken pte %lx %lx %d\n", pte_page->m_log->base, target_base, mhead->pid);
			
			spin_lock(&pte_page->m_log->recovery_lock);
			if (!pte_page->m_log->replica) {
				ptep_new = pte_realloc(mhead->mm);
				if (!ptep_new) {
					spin_unlock(&pte_page->m_log->recovery_lock);	
					printk(KERN_INFO "REPLICA ERROR: kmalloc failed\n");
					ret = -1;
					goto end;
				}
				printk(KERN_INFO "replica %lx", (unsigned long)__pa(ptep_new));
				if (restore_replica(target_base & PT_PGTABLE_MASK_NOT, ptep_new, pte_page) < 0) {
					spin_unlock(&pte_page->m_log->recovery_lock);
					delete_broken_pte_all(pte_page->m_log);
					pte_free(mhead->mm, virt_to_page(ptep_new));
					ret = -1;
					goto end;
				} 
				
				pte_page->m_log->replica = ptep_new;
				spin_unlock(&pte_page->m_log->recovery_lock);

				// check ref count 
				// if (make_recovery_thread(mhead, pte_page) < 0) {
				// 	delete_broken_pte_all(pte_page->m_log);
				// 	spin_lock(&pte_page->m_log->recovery_lock);
				// 	pte_page->m_log->replica = NULL;
				// 	spin_unlock(&pte_page->m_log->recovery_lock);
				// 	pte_free(mhead->mm, virt_to_page(ptep_new));
				// 	ret = -1;
				// }
				goto end;
			}
			spin_unlock(&pte_page->m_log->recovery_lock);
			break;
		}
	}
end:
	return ret;
}
EXPORT_SYMBOL_GPL(register_broken_pte_and_make_recovery_thread);


static long register_broken_pte_from_user(unsigned long user_va)
{
	struct m_head_struct *mhead;
	unsigned long target_base = user_va >> OFFSET_SHIFT;
	struct page *pte_page;
	pte_t *ptep_new; 
	int ret = 0;
#ifdef CONFIG_RECOVERY_COUNT
	struct recovery_count *rcount;
#endif

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			/* get struct page */
			if ((pte_page = get_page_from_addr(mhead->mm, target_base)) == NULL)
				goto end;

			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
				goto end;

			if (is_broken_pte_node_registered(target_base, pte_page->m_log) > 0)
				goto end;

			if (add_broken_pte_node(target_base, pte_page->m_log) < 0)
				goto end;
			printk(KERN_INFO "register broken pte %lx %lx %d\n", pte_page->m_log->base, target_base, mhead->pid);

#ifdef CONFIG_RECOVERY_COUNT
			list_for_each_entry(rcount, &count_head, list) {
				if (rcount->pid == current->tgid) {
					rcount->ucount++;		
					break;
				}
			}
#endif

			spin_lock(&pte_page->m_log->recovery_lock);
			if (!pte_page->m_log->replica) {
				ptep_new = pte_realloc(mhead->mm);
				if (!ptep_new) {
					spin_unlock(&pte_page->m_log->recovery_lock);
					printk(KERN_INFO "REPLICA ERROR: kmalloc failed\n");
					ret = -1;
					goto end;
				}
				printk(KERN_INFO "replica %lx", (unsigned long)__pa(ptep_new));
				if (restore_replica(target_base & PT_PGTABLE_MASK_NOT, ptep_new, pte_page) < 0) {
					spin_unlock(&pte_page->m_log->recovery_lock);
					delete_broken_pte_all(pte_page->m_log);
					pte_free(mhead->mm, virt_to_page(ptep_new));
					ret = -1;
					goto end;
				}
					
				pte_page->m_log->replica = ptep_new;
				spin_unlock(&pte_page->m_log->recovery_lock);
				
				// check ref count 
				if (recover_broken_pgtable(mhead->mm, mhead, pte_page) < 0){
					delete_broken_pte_all(pte_page->m_log);
					spin_lock(&pte_page->m_log->recovery_lock);
					pte_page->m_log->replica = NULL;
					spin_unlock(&pte_page->m_log->recovery_lock);
					pte_free(mhead->mm, virt_to_page(ptep_new));
					ret = -1;
				}
				goto end;
			}
			spin_unlock(&pte_page->m_log->recovery_lock);
			break;
		}
	}
end:
	return ret;
}

SYSCALL_DEFINE1(mycall_register_broken_pte, unsigned long, va)
{
	return register_broken_pte_from_user(va);
}

int check_pte_is_broken_for_pte_write(pte_t *ptep)
{
	struct page *pte_page;
	struct broken_pte_log *bnode;
	struct m_head_struct *mhead;
	unsigned long target_base;
	pte_t entry;
#ifdef CONFIG_RECOVERY_COUNT
	unsigned int count;
	struct recovery_count *rcount;
#endif
	
	if (!ptep) { // NULL pointer
		return -1;
	}

	entry = *ptep; // check EMEs and register broken pte

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
#ifdef CONFIG_RECOVERY_COUNT
			list_for_each_entry(rcount, &count_head, list) {
				if (rcount->pid == current->tgid) {
					get_random_bytes(&count, sizeof(count));
					if (count % DIVISION_NUM == 0) {
						rcount->kcount++;
						printk(KERN_INFO "detect EMEs at write %lx and start recovery\n", (unsigned long)ptep);
						register_broken_pte_and_make_recovery_thread((unsigned long)ptep);
					}
					break;
				}
			}
#endif
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
				break;

			spin_lock(&pte_page->m_log->recovery_lock);
			if (pte_page->m_log->replica) {
				target_base = make_ds_base_from_pte((unsigned long)ptep, pte_page->m_log->base);
				spin_lock(&pte_page->m_log->broken_lock);
				list_for_each_entry(bnode, &pte_page->m_log->head, list) {
					if (target_base == bnode->base) {
						// printk(KERN_INFO "Hit broken pte write %lx %d\n", target_base, current->tgid);
						// printk(KERN_INFO "get broken    pte %lx %lx\n", pte_pfn(*ptep), pte_flags(*ptep));
						spin_unlock(&pte_page->m_log->broken_lock);
						spin_unlock(&pte_page->m_log->recovery_lock);
						return 1; // pte is broken
					}
				}
				spin_unlock(&pte_page->m_log->broken_lock);
				spin_unlock(&pte_page->m_log->recovery_lock);
				return 0; // pte is healthy
			}
			spin_unlock(&pte_page->m_log->recovery_lock);
			return 0; // PT is healthy
		}
	}
	return -1; // pte is not managed by ds_log
}
EXPORT_SYMBOL_GPL(check_pte_is_broken_for_pte_write);

pte_t check_pte_is_broken_for_pte_read(pte_t *ptep)
{
	struct page *pte_page;
	struct broken_pte_log *bnode;
	struct m_head_struct *mhead;
	unsigned long target_base;
	unsigned long offset;
	pte_t *pte;
	pte_t entry;
#ifdef CONFIG_RECOVERY_COUNT
	unsigned int count;
	struct recovery_count *rcount;
#endif

	if(!ptep) { // NULL pointer
		return native_make_pte(0);
	}

	entry = *ptep; // check EMEs and register broken pte

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
#ifdef CONFIG_RECOVERY_COUNT
			list_for_each_entry(rcount, &count_head, list) {
				if (rcount->pid == current->tgid) {
					get_random_bytes(&count, sizeof(count));
					if (count % DIVISION_NUM == 0) {
						rcount->kcount++;
						printk(KERN_INFO "detect EMEs at read %lx and start recovery\n", (unsigned long)ptep);
						register_broken_pte_and_make_recovery_thread((unsigned long)ptep);
					}
					break;
				}
			}
#endif
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
				break;

			spin_lock(&pte_page->m_log->recovery_lock);
			if (pte_page->m_log->replica) {
				target_base = make_ds_base_from_pte((unsigned long)ptep, pte_page->m_log->base);
				spin_lock(&pte_page->m_log->broken_lock);
				list_for_each_entry(bnode, &pte_page->m_log->head, list) {
					if (target_base == bnode->base) {
						// printk(KERN_INFO "Hit broken pte read %lx %d\n", target_base, current->tgid);
						offset = target_base & PT_PGTABLE_MASK;
						pte = (pte_t *)pte_page->m_log->replica + offset;
						entry = *pte;
						// printk(KERN_INFO "get broken    pte %lx %lx\n", pte_pfn(*ptep), pte_flags(*ptep));
						// printk(KERN_INFO "get recovered pte %lx %lx\n", pte_pfn(*pte), pte_flags(*pte));
						break; // pte is broken
					}
				}
				spin_unlock(&pte_page->m_log->broken_lock);
			}
			spin_unlock(&pte_page->m_log->recovery_lock);
			break;
		}
	}
	return entry; // pte is safety or not managed by ds_log
}
EXPORT_SYMBOL_GPL(check_pte_is_broken_for_pte_read);

static void print_replica(pte_t *ptep, unsigned long base)
{
	unsigned long pgd = (base >> 27) & PT_PGTABLE_MASK;
	unsigned long pud = (base >> 18) & PT_PGTABLE_MASK;
	unsigned long pmd = (base >> 9) & PT_PGTABLE_MASK;

	printk(KERN_INFO "print dup pte\n");
	for(unsigned long pte=0; pte<MAX; pte++, ptep++) {
		if(!pte_none(*ptep) && pte_present(*ptep)) {
			printk(KERN_INFO "  %lx  %lx %lx  %lx\n", make_ds_base(pgd, pud, pmd, pte), pte_pfn(*ptep), pte_flags(*ptep), (unsigned long)ptep);
		}
	}
}

static int update_pmdp(struct mm_struct *mm, struct page *pte_page, pmd_t *pmdp)
{
	printk(KERN_INFO "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
	// print_replica(pte_offset_index(pmdp, 0), pte_page->m_log->base);

	spin_lock(&pte_page->m_log->recovery_lock);
	if (restore_page(pte_page, virt_to_page(pte_page->m_log->replica)) < 0) {
		spin_unlock(&pte_page->m_log->recovery_lock);
		return -1;	
	}
	// print_replica(pte_page->m_log->replica, pte_page->m_log->base);
	pmd_reinstall(mm, pmdp, pte_page->m_log->replica);
	spin_unlock(&pte_page->m_log->recovery_lock);

	printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
	return 0;
}

static int recover_broken_pgtable(struct mm_struct *mm, struct m_head_struct *mhead, struct page *pte_page)
{
	pmd_t *pmdp;
	// spinlock_t *ptl;
	unsigned long base = pte_page->m_log->base & PT_PGTABLE_MASK_NOT;
	unsigned long addr = 0;
	int count = 0;
	int ret = 0;
#ifdef CONFIG_RECOVERY_COUNT
	struct recovery_count *rcount;
#endif
	
	if(get_pmdp_for_recover_pgtable(mm, base, &pmdp) < 0) {
		printk(KERN_INFO "USER RECOVER ERROR: pmdp is NULL\n");
		ret = -1;
		goto end;
	}

	// ptl = ptlock_ptr(pmd_to_page(pmdp));
	// spin_lock(ptl);
	for(;;) {
		if (page_count(pte_page) == 1) {
			if (update_pmdp(mm, pte_page, pmdp) < 0) {
				// spin_unlock(ptl);			
				printk(KERN_INFO "USER RECOVER ERROR: failed to update pmdp\n");
				ret = -1;
				goto end;
			}
			count = delete_broken_pte_all(pte_page->m_log);
			break;
		}
		schedule();
	}
	// spin_unlock(ptl);

	// want to add TLB flush operation
	addr = base << OFFSET_SHIFT;
	flush_tlb_mm_range(mm, addr, addr + PMD_SIZE, OFFSET_SHIFT, false);

#ifdef CONFIG_RECOVERY_COUNT
	list_for_each_entry(rcount, &count_head, list) {
		if (rcount->pid == current->tgid) {
			rcount->usuccount += count;
			break;
		}
	}
#endif
	// printk(KERN_INFO "finish recovery\n");
	pte_page->m_log->base = 0;
	spin_lock(&pte_page->m_log->recovery_lock);
	pte_page->m_log->replica = NULL;
	spin_unlock(&pte_page->m_log->recovery_lock);
end:
	return ret;
}

// static int recovery_thread(void *data)
// {
// 	struct krecoverd_info *kinfo = (struct krecoverd_info *)data;
// 	struct m_head_struct *mhead = kinfo->mhead;
// 	struct page *pte_page = kinfo->page;
// 	pmd_t *pmdp;
// 	// spinlock_t *ptl;
// 	unsigned long base = pte_page->m_log->base & PT_PGTABLE_MASK_NOT;
// 	unsigned long addr = 0;
// 	int preempt = 0;
// 	int count = 0;
// #ifdef CONFIG_RECOVERY_COUNT
// 	struct recovery_count *rcount;
// #endif

// 	printk(KERN_INFO "start recovery thread\n");
// 	if (get_pmdp_for_recover_pgtable(mhead->mm, base, &pmdp) < 0) {
// 		printk(KERN_INFO "KRECOVERD ERROR: pmdp is NULL\n");
// 		goto failed;
// 	}
	
// 	// ptl = ptlock_ptr(pmd_to_page(pmdp));
// 	// spin_lock(ptl);

// 	printk(KERN_INFO "kthread should stop\n");

// 	preempt = dec_preempt_before_schedule();
// 	while (!kthread_should_stop())
// 		schedule();
// 	inc_preempt_after_schedule(preempt);

// 	printk(KERN_INFO "fin kthread should stop\n");

// 	if (update_pmdp(mhead->mm, pte_page, pmdp) < 0) {
// 		// spin_unlock(ptl);
// 		printk(KERN_INFO "KRECOVERD ERROR: failed to update pmdp\n");
// 		goto failed;
// 	}

// 	count = delete_broken_pte_all(pte_page->m_log);
// 	// spin_unlock(ptl);

// 	// want to add TLB flush operation
// 	addr = base << OFFSET_SHIFT;
// 	flush_tlb_mm_range(mhead->mm, addr, addr + PMD_SIZE, OFFSET_SHIFT, false);

// #ifdef CONFIG_RECOVERY_COUNT
// 	list_for_each_entry(rcount, &count_head, list) {
// 		if (rcount->pid == mhead->pid) {
// 			rcount->ksuccount += count;
// 			break;
// 		}
// 	}
// #endif
// 	goto end;

// failed:
// 	delete_broken_pte_all(pte_page->m_log);
// 	pte_free(mhead->mm, virt_to_page(pte_page->m_log->replica));

// end:
// 	pte_page->m_log->base = 0;
// 	spin_lock(&pte_page->m_log->recovery_lock);
// 	pte_page->m_log->replica = NULL;
// 	spin_unlock(&pte_page->m_log->recovery_lock);

// 	// spin_lock(&mhead->krecoverd_lock);
// 	// destroy_kinfo_node(kinfo);
// 	// mhead->kinfo = NULL;
// 	// spin_unlock(&mhead->krecoverd_lock);
// 	// print_pgtable(mm, mhead->pid);
// 	// printk(KERN_INFO "finish kthread\n");
// 	return 0;
// }

// static int make_recovery_thread(struct m_head_struct *mhead, struct page *pte_page)
// {
// 	int count = dec_preempt_before_schedule();
// 	int ret = 0;
// 	struct krecoverd_info *create;
	
// 	if ((create = make_kinfo_node(mhead, pte_page)) == NULL) {
// 		ret = -1;
// 		goto end;
// 	}

// 	for(;;) {
// 		spin_lock(&mhead->krecoverd_lock);
// 		if (!mhead->kinfo){
// 			create->krecoverd_task = kthread_run(recovery_thread, create, "krecoverd");
// 			if (IS_ERR(create->krecoverd_task)) {
// 				destroy_kinfo_node(create);
// 				printk(KERN_INFO "KRECOVERD ERROR: Failed to start krecoverd\n");
// 				ret = -1;
// 				goto end;
// 			}
			
// 			mhead->kinfo = create;
// 			spin_unlock(&mhead->krecoverd_lock);
// 			printk(KERN_INFO "recovery thread pid %d comm %s\n", create->krecoverd_task->pid, create->krecoverd_task->comm);
// 			goto end;
// 		}
// 		spin_unlock(&mhead->krecoverd_lock);
// 		schedule();
// 	}
// end:
// 	inc_preempt_after_schedule(count);
// 	return ret;
// }

int wait_to_recover_broken_pgtable(pmd_t *pmdp) 
{
	struct page *pte_page;
	struct m_head_struct *mhead;
	// int count = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			pte_page = pmd_page(*pmdp);
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
				return 0;
			
			// count = dec_preempt_before_schedule();
			for (;;) {
				if (!pte_page->m_log->replica) {
					// printk(KERN_INFO "Replaced replica\n");
					// inc_preempt_after_schedule(count);
					break;
				}
				// printk(KERN_INFO "wait to recover broken pgtable %lx\n", pte_page->m_log->base);
				schedule();
			}
			// inc_preempt_after_schedule(count);
			break;
		}
	}
	return 0;		
}
EXPORT_SYMBOL_GPL(wait_to_recover_broken_pgtable);

static void fix_krecoverd_failure(struct mm_struct *mm, struct m_head_struct *mhead, struct page *pte_page)
{
	pmd_t *pmdp;
	// spinlock_t *ptl;
	unsigned long base = pte_page->m_log->base & PT_PGTABLE_MASK_NOT;
	unsigned long addr = 0;
	int count = 0;
#ifdef CONFIG_RECOVERY_COUNT
	struct recovery_count *rcount;
#endif
	
	if (get_pmdp_for_recover_pgtable(mm, base, &pmdp) < 0) {
		printk(KERN_INFO "KRECOVERD ERROR: pmdp is NULL\n");
		goto failed;
	}

	// ptl = ptlock_ptr(pmd_to_page(pmdp));
	// spin_lock(ptl);
	for(;;) {
		if (page_count(pte_page) == 1) {
			if (update_pmdp(mm, pte_page, pmdp) < 0) {
				// spin_unlock(ptl);
				printk(KERN_INFO "KRECOVERD ERROR: failed to update pmdp\n");
				goto failed;
			}
			count = delete_broken_pte_all(pte_page->m_log);
			break;
		}
		schedule();
	}
	// spin_unlock(ptl);

	// want to add TLB flush operation
	addr = base << OFFSET_SHIFT;
	flush_tlb_mm_range(mm, addr, addr + PMD_SIZE, OFFSET_SHIFT, false);

#ifdef CONFIG_RECOVERY_COUNT
	list_for_each_entry(rcount, &count_head, list) {
		if (rcount->pid == current->tgid) {
			rcount->ksuccount += count;
			break;
		}
	}
#endif
	goto end;

failed:
	delete_broken_pte_all(pte_page->m_log);
	pte_free(mhead->mm, virt_to_page(pte_page->m_log->replica));

end:
	pte_page->m_log->base = 0;
	spin_lock(&pte_page->m_log->recovery_lock);
	pte_page->m_log->replica = NULL;
	spin_unlock(&pte_page->m_log->recovery_lock);

	// spin_lock(&mhead->krecoverd_lock);
	// destroy_kinfo_node(mhead->kinfo);
	// mhead->kinfo = NULL;
	// spin_unlock(&mhead->krecoverd_lock);
	return;
}

int inc_page_ref_count(pte_t *ptep)
{
	struct page *pte_page;
	struct m_head_struct *mhead;
	int ref_count = 0;
	int count = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
				goto end;

			count = dec_preempt_before_schedule();
			for (;;) {
				if (!pte_page->m_log->replica) {
					break;
				}
				schedule();
			}
			inc_preempt_after_schedule(count);

			ref_count = page_ref_inc_return(pte_page);
			// printk(KERN_INFO "m %lx ref count %d %d\n", pte_page->m_log->base, ref_count, current->tgid);
			break;
		}
	}
end:
	return 0;
}
EXPORT_SYMBOL_GPL(inc_page_ref_count);

int dec_page_ref_count(pte_t *ptep)
{
	struct page *pte_page; 
	struct m_head_struct *mhead;
	int ref_count = 0;
	int count = 0;
	int ret = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK)) 
				goto end;

			ref_count = page_ref_dec_return(pte_page);
			// printk(KERN_INFO "m %lx ref count %d %d\n", pte_page->m_log->base, ref_count, current->tgid);

			count = dec_preempt_before_schedule();
			// if (mhead->kinfo) {
				if (pte_page->m_log->replica && ref_count == 1) {
					// printk(KERN_INFO "kthread stop\n");
					// if ((ret = kthread_stop(mhead->kinfo->krecoverd_task)) < 0) {
					// 	printk(KERN_INFO "kthread failure %d and start recovery\n",ret);
					// 	fix_krecoverd_failure(mhead->mm, mhead, pte_page);
					// }
					fix_krecoverd_failure(mhead->mm, mhead, pte_page);
					// printk(KERN_INFO "fin kthread stop\n");
				}
			// }
			inc_preempt_after_schedule(count);
			break;
		}
	}
end:
	return 0;
}
EXPORT_SYMBOL_GPL(dec_page_ref_count);

static int get_ptep_and_make_m_log(pmd_t *pmdp, pid_t pid, unsigned long pte, pte_t **ptepp)
{
  	pte_t *ptep = pte_offset_index(pmdp, pte);
	*(ptepp) = ptep;

  	if(pte_none(*ptep) || !pte_present(*ptep)) {
    	// printk(KERN_INFO "pte %lu is not present.\n", pte);
    	return -1;
  	}

  	return 0;
}

static int get_pmdp_and_make_m_log(pud_t *pudp, pid_t pid, unsigned long pmd, pmd_t **pmdpp)
{
  	pmd_t *pmdp = pmd_offset_index(pudp, pmd);
	*(pmdpp) = pmdp;

  	if(pmd_none(*pmdp) || !pmd_present(*pmdp) || pmd_large(*pmdp)){
    	// printk(KERN_INFO "pmd %lu is not present.\n", pmd);
    	return -1;
  	}

	__make_pte_m_log(pmdp, (pte_t *)pmd_page_vaddr(*pmdp), pid);
	
  	return 0;
}

static int get_pudp_and_make_m_log(p4d_t *p4dp, pid_t pid, unsigned long pud, pud_t **pudpp)
{
  	pud_t *pudp = pud_offset_index(p4dp, pud);
	*(pudpp) = pudp;
	
  	if(pud_none(*pudp) || !pud_present(*pudp) || pud_large(*pudp)){
	    // printk(KERN_INFO "pud %lu is not present", pud);
	    return -1;
  	}

	__make_pmd_m_log(pudp, (pmd_t *)pud_pgtable(*pudp), pid);
	
  	return 0;  
}

static int get_p4dp_and_make_m_log(pgd_t *pgdp, pid_t pid, unsigned long p4d, p4d_t **p4dpp)
{
  	p4d_t *p4dp = p4d_offset_index(pgdp, p4d);
	*(p4dpp) = p4dp;
	
	if(p4d_none(*p4dp) || !p4d_present(*p4dp)){
	    // printk(KERN_INFO "p4d %lu is not present", pgd);
    	return -1;
  	}
	
	__make_pud_m_log(p4dp, (pud_t *)p4d_pgtable(*p4dp), pid);
	
	return 0;
}

static int get_pgdp_and_make_m_log(struct mm_struct *mm, pid_t pid, unsigned long pgd, p4d_t **p4dpp)
{
  	pgd_t *pgdp = pgd_offset_index(mm, pgd);

  	if(pgd_none(*pgdp) || !pgd_present(*pgdp)){
	    // printk(KERN_INFO "pgd %lu is not present.\n", pgd);
    	return -1;
  	}

	if(get_p4dp_and_make_m_log(pgdp, pid, pgd, p4dpp) < 0){
		return -1;
	}

  	return 0;
}

static long make_pte_ds_log_usr_from_pgtable(struct task_struct *p)
{
	struct m_head_struct *mhead;
	struct mm_struct *mm;
	pid_t pid = p->tgid;
	// pgd_t *pgdp;
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep;
	
	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == pid) {
			mm = mhead->mm;
			break;
		}
	}

	if (!mm) {
		printk(KERN_INFO "Please execute register_pid\n");
		goto err;
	}

	// if p have already registered, retur -1
	if(__make_pgd_m_log(mm->pgd, pid) < 0) {
		printk(KERN_INFO "Have already made m list\n");
		return 0;
	}

	for(unsigned long pgd=0; pgd < USER_MAX; pgd++) {
		if(get_pgdp_and_make_m_log(mm, pid, pgd, &p4dp) == 0) {
			for(unsigned long pud=0; pud<MAX; pud++) {
				if(get_pudp_and_make_m_log(p4dp, pid, pud, &pudp) == 0) {
					for(unsigned long pmd=0; pmd<MAX; pmd++) {
						if(get_pmdp_and_make_m_log(pudp, pid, pmd, &pmdp) == 0) {
							for(unsigned long pte=0; pte<MAX; pte++) {
			                	if(get_ptep_and_make_m_log(pmdp, pid, pte, &ptep) == 0) {
									// make_ds from ptep
									if(__make_pte_ds_log_usr(ptep, *ptep, pid) < 0) {
										printk(KERN_INFO "pte ds list failure at from_pgtable\n");
										// goto end;
									}
			                    }
			            	}
						}
		        	}
				}
	    	}
		}
    }
	return 0;
err:
	return -1;
}

SYSCALL_DEFINE0(mycall_make_ds_usr_from_pgtable)
{
	long ret;
	ktime_t start, end;

	start = ktime_get();
	ret = make_pte_ds_log_usr_from_pgtable(current);
	end = ktime_get();

	printk(KERN_INFO "make_ds_usr time: %lld\n", ktime_sub(end, start));
	
	return ret;
}

static long register_pid(struct mm_struct *mm, pid_t pid)
{
	struct m_head_struct *mhead;
	int ret = 0;

	// target_task = current;
	
	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == pid) {
			// printk(KERN_INFO "Have already registered the pid %d\n", pid);
			ret = -1;
			goto end;
		}
	}

	mhead = kmalloc(sizeof(struct m_head_struct), GFP_ATOMIC);
	if (!mhead) {
		ret = -1;
		goto end;
	}
	mhead->pid = pid;
	mhead->mm = mm;
	// mhead->kinfo = NULL;
	// spin_lock_init(&mhead->krecoverd_lock);
	list_add(&mhead->list, &user_head);

	printk(KERN_INFO "init pid %d\n",pid);
end:
	return ret;
}

static void register_recovery_count(pid_t pid)
{
	struct recovery_count *rcount;
	
	rcount = kmalloc(sizeof(struct recovery_count), GFP_KERNEL);
	if (!rcount) {
		return;
	}
	rcount->pid = pid;
	rcount->kcount = 0;
	rcount->ksuccount = 0;
	rcount->ucount = 0;
	rcount->usuccount = 0;
	list_add(&rcount->list, &count_head);
}

SYSCALL_DEFINE0(mycall_ds_register_pid)
{
	long ret = register_pid(current->mm, current->tgid); 
	// if (init_ds_log_mempool()) {
	// 	printk(KERN_INFO "ds_log mempool init error\n");
	// }

	if (ret == 0) {
#ifdef CONFIG_RECOVERY_COUNT
		register_recovery_count(current->tgid);
#endif
		ret = make_pte_ds_log_usr_from_pgtable(current);
	}
	return ret;
	// return register_pid(current->mm, current->tgid);
}

bool check_parent_is_target(pid_t ppid, pid_t pid)
{
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == ppid) {
			// printk(KERN_INFO "parent pid %d, child pid %d\n", ppid, pid);
			return true;
		}
	}
	return false;	
}
EXPORT_SYMBOL_GPL(check_parent_is_target);

void register_child(struct task_struct *p)
{
	// register pid & make ds_log, m_list
	// printk(KERN_INFO "child pid %d, current tid %d, current pid %d\n", p->tgid, current->pid, current->tgid);
	if (register_pid(p->mm, p->tgid) == 0) {
		// print_user_pgtable(p);
#ifdef CONFIG_RECOVERY_COUNT
		register_recovery_count(p->tgid);
#endif
		make_pte_ds_log_usr_from_pgtable(p);
	}
}
EXPORT_SYMBOL_GPL(register_child);
