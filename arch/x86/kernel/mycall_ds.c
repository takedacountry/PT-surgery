#include <linux/syscalls.h> //
// #include <linux/printk.h>
// #include <linux/types.h>
// #include <linux/mm.h>
// #include <linux/rwlock.h> // 
// #include <linux/spinlock.h> //
#include <linux/export.h>
#include <linux/delay.h> // for msleep
#include <linux/kthread.h> //
#include <linux/preempt.h> //
#include <linux/pid.h>
#include <linux/mm_types.h>
#include <linux/hugetlb.h>
#include <asm-generic/tlb.h>
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
LIST_HEAD(kern_head);

// DEFINE_RWLOCK(user_head_lock);

// struct task_struct *target_task;

static int make_recovery_thread(struct m_list *mnode);
static int recover_broken_pgtable(struct m_list *mnode);
static void print_dup_pte(pte_t *ptep, unsigned long base);

static struct task_struct *k_thread;
static struct task_struct *current_kproc;
// static struct mm_struct *current_mm;
// static pgd_t *current_pgd;
static int recover_count = 0;

static int __make_pgd_m_list(unsigned long pgd_va, pid_t pid)
{
	struct m_head_list *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == pid) {
			m_list_write_lock(mhead);
			if(add_first_m_node(pgd_va & PAGE_MASK, PGD_FLAG_MASK, mhead) < 0) {
				m_list_write_unlock(mhead);
				return -1;
			}
			m_list_write_unlock(mhead);
			// printk(KERN_INFO "make m pgd alloc %lx, %lx, %d\n", pgd_va & PAGE_MASK, PGD_FLAG_MASK, pid);
			printk(KERN_INFO "make m pgd alloc %lx, %lx, %d, %d\n", pgd_va & PAGE_MASK, PGD_FLAG_MASK, current->pid, current->tgid);
			break;
		}
	}
	return 0;
}

int make_pgd_m_list(unsigned long pgd_va)
{
	return __make_pgd_m_list(pgd_va, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pgd_m_list);

static unsigned long get_p4d_base(unsigned long p4d_va, unsigned long pud_va, struct m_head_list *mhead, pid_t pid)
{
	struct m_list *itr;
	unsigned long base;

	list_for_each_entry(itr, &mhead->head, list) {
		if(itr->base & PGD_FLAG_MASK && itr->va <= p4d_va && p4d_va < itr->va + OFFSET_SIZE) {
			base = make_ds_va(((p4d_va - itr->va) / 0x8) & PT_PGTABLE_MASK, 0, 0, PUD_FLAG_MASK & PT_PGTABLE_MASK);
			goto pud_va;
		}
	}
	goto err;
	
pud_va:
	pud_va &= PAGE_MASK;
	while(itr->base <= base) {
		if(itr->va != pud_va && itr->base == base) {
			itr->va = pud_va;
			printk(KERN_INFO "modify m pud %lx %lx\n", itr->va, itr->base);
			goto ret;
		}
		if(list_is_last(&itr->list, &mhead->head)) {
			if(add_m_node(pud_va, base, itr) < 0) {
				goto err;
			}
			goto end;
		}
		itr = list_next_entry(itr, list);
	}
	if(add_tail_m_node(pud_va, base, itr) < 0)
		goto err;
end:
	// printk(KERN_INFO "make m pud alloc %lx, %lx, %d\n", pud_va, base, pid);
	printk(KERN_INFO "make m pud alloc %lx, %lx, %d, %d\n", pud_va, base, current->pid, current->tgid);
ret:
	return base;
err:
	return MAX_NUM;
}

static int __make_pud_m_list(unsigned long p4d_va, unsigned long pud_va, pid_t pid)
{
	struct m_head_list *mhead;
	unsigned long base;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == pid) {
			m_list_write_lock(mhead);
			if((base = get_p4d_base(p4d_va, pud_va, mhead, pid)) >= MAX_NUM) {
				m_list_write_unlock(mhead);
				return -1;
			}
			m_list_write_unlock(mhead);
			break;
		}
	}
	return 0;
}

int make_pud_m_list(unsigned long p4d_va, unsigned long pud_va)
{
	return __make_pud_m_list(p4d_va, pud_va, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pud_m_list);

static unsigned long get_pud_base(unsigned long pud_va, unsigned long pmd_va, struct m_head_list *mhead, pid_t pid)
{
	struct m_list *itr;
	unsigned long base = 0;

	list_for_each_entry(itr, &mhead->head, list) {
		if(itr->base & PUD_FLAG_MASK && itr->va <= pud_va && pud_va < itr->va + OFFSET_SIZE) {
			base = make_ds_va((itr->base >> 27) & PT_PGTABLE_MASK, ((pud_va - itr->va) / 0x8) & PT_PGTABLE_MASK, 0, PMD_FLAG_MASK & PT_PGTABLE_MASK);
			goto pmd_va;
		}
	}
	goto err;

pmd_va:
	pmd_va &= PAGE_MASK;
	while(itr->base <= base) {
		if(itr->va != pmd_va && itr->base == base) {
			itr->va = pmd_va;
			printk(KERN_INFO "modify m pmd %lx %lx\n", itr->va, itr->base);
			goto ret;
		}
		if(list_is_last(&itr->list, &mhead->head)) {
			if(add_m_node(pmd_va, base, itr) < 0) {
				goto err;
			}
			goto end;
		}
		itr = list_next_entry(itr, list);
	}
	if(add_tail_m_node(pmd_va, base, itr) < 0)
		goto err;
end:
	// printk(KERN_INFO "make m pmd alloc %lx, %lx, %d\n", pmd_va, base, pid);
	printk(KERN_INFO "make m pmd alloc %lx, %lx, %d, %d\n", pmd_va, base, current->pid, current->tgid);
ret:
	return base;
err:
	return MAX_NUM;
}

static int __make_pmd_m_list(unsigned long pud_va, unsigned long pmd_va, pid_t pid)
{
	struct m_head_list *mhead;
	unsigned long base;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == pid) {
			m_list_write_lock(mhead);
			if((base = get_pud_base(pud_va, pmd_va, mhead, pid)) >= MAX_NUM) {
				m_list_write_unlock(mhead);
				return -1;
			}
			m_list_write_unlock(mhead);
			break;
		}
	}
	return 0;
}

int make_pmd_m_list(unsigned long pud_va, unsigned long pmd_va)
{
	return __make_pmd_m_list(pud_va, pmd_va, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pmd_m_list);

static unsigned long get_pmd_base(unsigned long pmd_va, unsigned long pte_va, struct m_head_list *mhead, pid_t pid)
{
	struct m_list *itr;
	unsigned long base = 0;

	list_for_each_entry(itr, &mhead->head, list) {
		if(itr->base & PMD_FLAG_MASK && itr->va <= pmd_va && pmd_va < itr->va + OFFSET_SIZE) {
			base = make_ds_va((itr->base >> 27) & PT_PGTABLE_MASK, (itr->base >> 18) & PT_PGTABLE_MASK,  ((pmd_va - itr->va) / 0x8) & PT_PGTABLE_MASK, PTE_FLAG_MASK & PT_PGTABLE_MASK);
			goto pte_va;
		}
	}
	goto err;

pte_va:
	pte_va &= PAGE_MASK;
	while(itr->base <= base) {
		if(itr->va != pte_va && itr->base == base) {
			itr->va = pte_va;
			printk(KERN_INFO "modify m pte %lx %lx\n", itr->va, itr->base);
			goto ret;
		}
		if(list_is_last(&itr->list, &mhead->head)) {
			if(add_m_node(pte_va, base, itr) < 0){
				goto err;
			}
			goto end;
		}
		itr = list_next_entry(itr, list);
	}
	if(add_tail_m_node(pte_va, base, itr) < 0)
		goto err;
end:		
	// printk(KERN_INFO "make m pte alloc %lx, %lx, %d\n", pte_va, base, pid);
	printk(KERN_INFO "make m pte alloc %lx, %lx, %d, %d\n", pte_va, base, current->pid, current->tgid);
ret:
	return base;
err:
	return MAX_NUM;
}

static int __make_pte_m_list(unsigned long pmd_va, unsigned long pte_va, pid_t pid)
{
	struct m_head_list *mhead;
	unsigned long base;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == pid) {
			m_list_write_lock(mhead);
			if((base = get_pmd_base(pmd_va, pte_va, mhead, pid)) >= MAX_NUM) {
				m_list_write_unlock(mhead);
				return -1;
			}
			m_list_write_unlock(mhead);
			break;
		}
	}
	return 0;
}

int make_pte_m_list(unsigned long pmd_va, unsigned long pte_va)
{
	return __make_pte_m_list(pmd_va, pte_va, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pte_m_list);

// static unsigned long get_pte_base(unsigned long va, struct m_head_list *m_head)
// {
// 	struct m_list *itr;

// 	list_for_each_entry(itr, &m_head->head, list) {
// 		if(itr->base & PTE_FLAG_MASK && itr->va <= va && va < itr->va + OFFSET_SIZE) {
// 			return make_ds_va((itr->base >> 27) & PT_PGTABLE_MASK, (itr->base >> 18) & PT_PGTABLE_MASK, (itr->base >> 9) & PT_PGTABLE_MASK, ((va - itr->va) / 0x8) & PT_PGTABLE_MASK);
// 		}
// 	}
// 	return MAX_NUM;
// }

static void modify_ds_flag(struct ds_list *ds_node, struct ds_list *new, struct m_list *m_node)
{
	struct ds_list *next, *prev;

	if(ds_node->base == new->base && ds_node->limit == new->limit) {
		ds_node->flag = new->flag;
		if(list_is_first(&ds_node->list, &m_node->ds_head)) {
			next = list_next_entry(ds_node, list);
			ds_node_merge(ds_node, next);
		}
		else if(list_is_last(&ds_node->list, &m_node->ds_head)) {
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
		if(!list_is_first(&new->list, &m_node->ds_head)) {
			prev = list_prev_entry(new, list);
			ds_node_merge(prev, new);
		}
	}
	else if(ds_node->limit == new->limit) {
		ds_node->limit--;
		list_add(&new->list, &ds_node->list);
		if(!list_is_last(&new->list, &m_node->ds_head)) {
			next = list_next_entry(new, list);
			ds_node_merge(new, next);
		}
	}
	else {
		if((next = make_ds_node(new->limit, ds_node->limit, ds_node->offset, ds_node->flag)) == NULL)
			return;
		ds_node->limit = new->base;
		list_add(&new->list, &ds_node->list);
		list_add(&next->list, &new->list);
	}
	return;
}

static void modify_ds_offset(struct ds_list *ds_node, struct ds_list *new, struct m_list *m_node)
{
	struct ds_list *next, *prev;

	if(ds_node->base == new->base && ds_node->limit == new->limit) {
		ds_node->offset = new->offset;
		ds_node->flag = new->flag;
		if(list_is_first(&ds_node->list, &m_node->ds_head)) {
			next = list_next_entry(ds_node, list);
			ds_node_merge(ds_node, next);
		}
		else if(list_is_last(&ds_node->list, &m_node->ds_head)) {
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
		if(!list_is_first(&new->list, &m_node->ds_head)) {
			prev = list_prev_entry(new, list);
			ds_node_merge(prev, new);
		}
	}
	else if(ds_node->limit == new->limit) {
		ds_node->limit--;
		list_add(&new->list, &ds_node->list);
		if(!list_is_last(&new->list, &m_node->ds_head)) {
			next = list_next_entry(new, list);
			ds_node_merge(new, next);
		}
	}
	else {
		if((next = make_ds_node(new->limit, ds_node->limit, ds_node->offset, ds_node->flag)) == NULL)
			return;
		ds_node->limit = new->base;
		list_add(&new->list, &ds_node->list);
		list_add(&next->list, &new->list);
	}
	return;
}

static void delete_ds(struct ds_list *ds_node, struct ds_list *new, struct m_list *m_node)
{
	struct ds_list *next;

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
		if((next = make_ds_node(new->limit, ds_node->limit, ds_node->offset, ds_node->flag)) == NULL)
			return;
		ds_node->limit = new->base;
		list_add(&next->list, &ds_node->list);
	}
	return;
}

static bool is_ds_write(struct ds_list *ds_node)
{
	if(ds_node->flag & FLAG_RW)
		return true;
	else
		return false;
}

static void dup_pte_update(unsigned long addr, pte_t *ptep, pte_t pte)
{
	pte_t *target_pte = ptep + (addr & PT_PGTABLE_MASK);
	set_pte_recover(target_pte, pte);
}

static int __make_ds_list_usr(unsigned long va, pte_t pte, pid_t pid)
{
	struct m_head_list *mhead;
	struct m_list *mnode;
	struct ds_list *dnode, *next, *prev;
	unsigned long pte_value = pte_pfn(pte);
	unsigned long pte_flag = pte_flags(pte);
	unsigned long base = MAX_NUM;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == pid) {
			// if((base = get_pte_base(va, mhead)) >= MAX_NUM)
			// 	return -1;
			// break;
			m_list_read_lock(mhead);
			list_for_each_entry(mnode, &mhead->head, list) {
				member_read_lock(mnode);
				if(mnode->base & PTE_FLAG_MASK && mnode->va <= va && va < mnode->va + OFFSET_SIZE) {
					base = make_ds_va((mnode->base >> 27) & PT_PGTABLE_MASK, (mnode->base >> 18) & PT_PGTABLE_MASK, (mnode->base >> 9) & PT_PGTABLE_MASK, ((va - mnode->va) / 0x8) & PT_PGTABLE_MASK);
					member_read_unlock(mnode);
					break;
				}
				member_read_unlock(mnode);
			}
			m_list_read_unlock(mhead);
			
			if(base == MAX_NUM)
				goto err;

			member_write_lock(mnode);
			if(mnode->dup_pte)
				dup_pte_update(base, mnode->dup_pte, pte);
			member_write_unlock(mnode);

			if((dnode = make_ds_node(base, base+1, make_ds_offset(base, pte_value), pte_flag)) == NULL)
				goto err;

			ds_list_write_lock(mnode);
			if(list_empty(&mnode->ds_head)) {
				list_add(&dnode->list, &mnode->ds_head);
				ds_list_write_unlock(mnode);
				goto end;
			}
			else {
				list_for_each_entry_reverse(prev, &mnode->ds_head, list) {
					if(prev->base <= dnode->base && dnode->limit <= prev->limit) {
						// printk(KERN_INFO "make ds hit ds %lx %lx %lx %lx %d\n", base, pte_value, pte_flag, va, pid);
						if(pte_value == 0 && pte_flag == 0) {
							delete_ds(prev, dnode, mnode);
							printk(KERN_INFO "delete ds %lx-%lx\n", dnode->base, dnode->limit);
						}
						else if(dnode->offset != prev->offset) {
							// modify pte value
							modify_ds_offset(prev, dnode, mnode);
							printk(KERN_INFO "modify ds offset %lx %lx %lx %d\n", base, pte_value, pte_flag, pid);
						}
						else if(dnode->flag != prev->flag) {
							// modify pte flag 
							if(!is_ds_write(prev) && is_ds_write(dnode)) {
								// ds_mkwrite
								// printk(KERN_INFO "make write %lx %lx-%lx", base, prev->base, prev->limit);
								modify_ds_flag(prev, dnode, mnode);
								printk(KERN_INFO "make write %lx %lx %d\n", base, pte_flag, pid);
							}
							else if(is_ds_write(prev) && !is_ds_write(dnode)) {
								// ds_wrprotect
								// printk(KERN_INFO "make wrprotect %lx %lx-%lx", base, prev->base, prev->limit);
								modify_ds_flag(prev, dnode, mnode);
								printk(KERN_INFO "make wrprotect %lx %lx %d\n", base, pte_flag, pid);
							}
							else {
								printk(KERN_INFO "not modify ds flag %lx  %lx->%lx %d\n", base, prev->flag, pte_flag, pid);
							}
						}
						ds_list_write_unlock(mnode);
						goto end;
					}
					else if(dnode->base >= prev->limit) {
						list_add(&dnode->list, &prev->list);
						// printk(KERN_INFO "make ds %lx %lx %lx %lx %d\n", base, pte_value, pte_flag, va, pid);
						if(list_is_last(&dnode->list, &mnode->ds_head)) {
							ds_node_merge(prev, dnode);
							ds_list_write_unlock(mnode);
							goto end;
						}
						next = list_next_entry(dnode, list);
						ds_node_merge(dnode, next);
						ds_node_merge(prev, dnode);
						ds_list_write_unlock(mnode);
						goto end;
					}
				}
				list_add(&dnode->list, &mnode->ds_head);
				// printk(KERN_INFO "make ds %lx %lx %lx %lx %d\n", base, pte_value, pte_flag, va, pid);
				next = list_next_entry(dnode, list);
				ds_node_merge(dnode, next);
				ds_list_write_unlock(mnode);
				goto end;
			}
		}
	}
	
end:
	return 0;
err:
	return -1;
}

int make_ds_list_usr(unsigned long va, pte_t pte)
{
	return __make_ds_list_usr(va, pte, current->tgid);
}
EXPORT_SYMBOL_GPL(make_ds_list_usr);

static void clear_wrbit_ds_flag(struct ds_list *ds_node, unsigned long base, unsigned long limit, struct m_list *m_node)
{
	struct ds_list *next, *prev;

	if(ds_node->base == base && ds_node->limit == limit) {
		ds_node->flag &= FLAG_RW_NOT;
		printk(KERN_INFO "make wrprotect %lx %lx %d\n", base, ds_node->flag, current->tgid);
		if(list_is_first(&ds_node->list, &m_node->ds_head)) {
			next = list_next_entry(ds_node, list);
			ds_node_merge(ds_node, next);
		}
		else if(list_is_last(&ds_node->list, &m_node->ds_head)) {
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
		if((next = make_ds_node(base, limit, ds_node->offset, ds_node->flag & FLAG_RW_NOT)) == NULL)
			return;
		printk(KERN_INFO "make wrprotect %lx %lx %d\n", base, next->flag, current->tgid);
		list_add_tail(&next->list, &ds_node->list);
		if(!list_is_first(&next->list, &m_node->ds_head)) {
			prev = list_prev_entry(next, list);
			ds_node_merge(prev, next);
		}
	}
	else if(ds_node->limit == limit) {
		ds_node->limit--;
		if((prev = make_ds_node(base, limit, ds_node->offset, ds_node->flag & FLAG_RW_NOT)) == NULL)
			return;
		printk(KERN_INFO "make wrprotect %lx %lx %d\n", base, prev->flag, current->tgid);
		list_add(&prev->list, &ds_node->list);
		if(!list_is_last(&prev->list, &m_node->ds_head)) {
			next = list_next_entry(prev, list);
			ds_node_merge(prev, next);
		}
	}
	else {
		if((prev = make_ds_node(base, limit, ds_node->offset, ds_node->flag & FLAG_RW_NOT)) == NULL)
			return;
		if((next = make_ds_node(limit, ds_node->limit, ds_node->offset, ds_node->flag)) == NULL)
			return;
		printk(KERN_INFO "make wrprotect %lx %lx %d\n", base, prev->flag, current->tgid);
		ds_node->limit = base;
		list_add(&prev->list, &ds_node->list);
		list_add(&next->list, &prev->list);
	}
	return;
}

static void dup_pte_clear_bit(unsigned long addr, pte_t *ptep)
{
	pte_t *target_pte = ptep + (addr & PT_PGTABLE_MASK);
	clear_bit(_PAGE_BIT_RW, (unsigned long *)&target_pte->pte);
}

int clear_wrbit_ds_list(unsigned long va)
{
	struct m_head_list *mhead;
	struct m_list *mnode;
	struct ds_list *prev;
	unsigned long base = MAX_NUM;
	unsigned long limit;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			m_list_read_lock(mhead);
			list_for_each_entry(mnode, &mhead->head, list) {
				member_read_lock(mnode);
				if(mnode->base & PTE_FLAG_MASK && mnode->va <= va && va < mnode->va + OFFSET_SIZE) {
					base = make_ds_va((mnode->base >> 27) & PT_PGTABLE_MASK, (mnode->base >> 18) & PT_PGTABLE_MASK, (mnode->base >> 9) & PT_PGTABLE_MASK, ((va - mnode->va) / 0x8) & PT_PGTABLE_MASK);
					limit = base + 1;
					member_read_unlock(mnode);
					break;
				}
				member_read_unlock(mnode);
			}
			m_list_read_unlock(mhead);
			
			if(base == MAX_NUM) 
				goto err;

			member_write_lock(mnode);
			if(mnode->dup_pte)
				dup_pte_clear_bit(base, mnode->dup_pte);
			member_write_unlock(mnode);

			ds_list_write_lock(mnode);
			list_for_each_entry_reverse(prev, &mnode->ds_head, list) {
				if(prev->base <= base && limit <= prev->limit) {
					if(is_ds_write(prev)) {
						clear_wrbit_ds_flag(prev, base, limit, mnode);
					}
					break;
				}
			}
			ds_list_write_unlock(mnode);
			break;
		}
	}

	return 0;
err:
	return -1;
}
EXPORT_SYMBOL_GPL(clear_wrbit_ds_list);

int register_broken_pte_and_recover_broken_pgtable(unsigned long va)
{
	struct m_head_list *mhead;
	struct m_list *mnode;
	struct broken_pte_list *bnode;
	unsigned int offset;
	unsigned long va_start = MAX_NUM;
	pte_t *ptep_new;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			m_list_read_lock(mhead);
			list_for_each_entry(mnode, &mhead->head, list) {
				member_read_lock(mnode);
				if(mnode->base & PTE_FLAG_MASK && mnode->va <= va && va < mnode->va + OFFSET_SIZE) {
					offset = ((va - mnode->va) / 0x8) & PT_PGTABLE_MASK;
					va_start = mnode->base & PT_PGTABLE_MASK_NOT;
					member_read_unlock(mnode);

					broken_list_read_lock(mnode);
					list_for_each_entry(bnode, &mnode->broken_head, list) {
						if(offset == bnode->offset) {
							printk(KERN_INFO "Have already registered the broken pte %lx\n", make_ds_va((va_start >> 27) & PT_PGTABLE_MASK, (va_start >> 18) & PT_PGTABLE_MASK, (va_start >> 9) & PT_PGTABLE_MASK, offset));
							broken_list_read_unlock(mnode);
							m_list_read_unlock(mhead);
							return 0;
						}
					}
					broken_list_read_unlock(mnode);
					break;
				}
				member_read_unlock(mnode);
			}
			m_list_read_unlock(mhead);

			if(va_start == MAX_NUM)
				goto err;

			broken_list_write_lock(mnode);
			if(add_broken_pte_node(offset, mnode) < 0) {
				broken_list_write_unlock(mnode);
				goto err;
			}
			broken_list_write_unlock(mnode);
			printk(KERN_INFO "register broken pte %lx %d\n", make_ds_va((va_start >> 27) & PT_PGTABLE_MASK, (va_start >> 18) & PT_PGTABLE_MASK, (va_start >> 9) & PT_PGTABLE_MASK, offset), offset);
			
			member_write_lock(mnode);
			if(!mnode->dup_pte) {
				ptep_new = pte_realloc(current->mm);
				if(!ptep_new) {
					printk(KERN_INFO "out of memory\n");
					member_write_unlock(mnode);
					goto err;
				}
				printk(KERN_INFO "duplicate pte %lx", (unsigned long)__pa(ptep_new));
				update_dup_pgtable(va_start, ptep_new, mnode);
				mnode->dup_pte = ptep_new;

				member_write_unlock(mnode);
				// check ref count 
				if(recover_broken_pgtable(mnode) < 0)
					goto err;
				goto end;
			}
			member_write_unlock(mnode);
			break;
		}
	}
end:
	return 0;
err:
	return -1;
}
EXPORT_SYMBOL_GPL(register_broken_pte_and_recover_broken_pgtable);

int register_broken_pte_and_make_recovery_thread(unsigned long va)
{
	struct m_head_list *mhead;
	struct m_list *mnode;
	struct broken_pte_list *bnode;
	unsigned int offset;
	unsigned long va_start = MAX_NUM;
	pte_t *ptep_new;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			m_list_read_lock(mhead);
			list_for_each_entry(mnode, &mhead->head, list) {
				member_read_lock(mnode);
				if(mnode->base & PTE_FLAG_MASK && mnode->va <= va && va < mnode->va + OFFSET_SIZE) {
					offset = ((va - mnode->va) / 0x8) & PT_PGTABLE_MASK;
					va_start = mnode->base & PT_PGTABLE_MASK_NOT;
					member_read_unlock(mnode);

					broken_list_read_lock(mnode);
					list_for_each_entry(bnode, &mnode->broken_head, list) {
						if(offset == bnode->offset) {
							printk(KERN_INFO "Have already registered the broken pte %lx\n", make_ds_va((va_start >> 27) & PT_PGTABLE_MASK, (va_start >> 18) & PT_PGTABLE_MASK, (va_start >> 9) & PT_PGTABLE_MASK, offset));
							broken_list_read_unlock(mnode);
							m_list_read_unlock(mhead);
							return 0;
						}
					}
					broken_list_read_unlock(mnode);
					break;
				}
				member_read_unlock(mnode);
			}
			m_list_read_unlock(mhead);

			if(va_start == MAX_NUM)
				goto err;

			broken_list_write_lock(mnode);
			if(add_broken_pte_node(offset, mnode) < 0) {
				broken_list_write_unlock(mnode);
				goto err;
			}
			broken_list_write_unlock(mnode);
			printk(KERN_INFO "register broken pte %lx %d\n", make_ds_va((va_start >> 27) & PT_PGTABLE_MASK, (va_start >> 18) & PT_PGTABLE_MASK, (va_start >> 9) & PT_PGTABLE_MASK, offset), offset);
			
			member_write_lock(mnode);
			if(!mnode->dup_pte) {
				ptep_new = pte_realloc(current->mm);
				if(!ptep_new) {
					printk(KERN_INFO "out of memory\n");
					member_write_unlock(mnode);
					goto err;
				}
				printk(KERN_INFO "duplicate pte %lx", (unsigned long)__pa(ptep_new));
				update_dup_pgtable(va_start, ptep_new, mnode);
				mnode->dup_pte = ptep_new;

				member_write_unlock(mnode);
				// check ref count 
				if(make_recovery_thread(mnode) < 0)
					goto err;
				goto end;
			}
			member_write_unlock(mnode);
			break;
		}
	}
end:
	return 0;
err:
	return -1;
}
EXPORT_SYMBOL_GPL(register_broken_pte_and_make_recovery_thread);

static long register_broken_pte_from_user(unsigned long va)
{
	struct m_head_list *mhead;
	struct m_list *mnode;
	struct broken_pte_list *bnode;
	unsigned int offset;
	unsigned long base = va >> OFFSET_SHIFT;
	unsigned long va_start = MAX_NUM;
	pte_t *ptep_new; 

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			m_list_read_lock(mhead);
			list_for_each_entry(mnode, &mhead->head, list) {
				member_read_lock(mnode);
				if(mnode->base & PTE_FLAG_MASK && (mnode->base & PT_PGTABLE_MASK_NOT) == (base & PT_PGTABLE_MASK_NOT)) {
					offset = (base - (mnode->base & PT_PGTABLE_MASK_NOT)) & PT_PGTABLE_MASK;
					va_start = mnode->base & PT_PGTABLE_MASK_NOT;
					member_read_unlock(mnode);

					broken_list_read_lock(mnode);
					list_for_each_entry(bnode, &mnode->broken_head, list) {
						if(offset == bnode->offset) {
							printk(KERN_INFO "Have already registered the broken pte %lx\n", make_ds_va((va_start >> 27) & PT_PGTABLE_MASK, (va_start >> 18) & PT_PGTABLE_MASK, (va_start >> 9) & PT_PGTABLE_MASK, offset));
							broken_list_read_unlock(mnode);
							m_list_read_unlock(mhead);
							return 0;
						}
					}
					broken_list_read_unlock(mnode);
					break;
				}
				member_read_unlock(mnode);
			}
			m_list_read_unlock(mhead);

			if(va_start == MAX_NUM)
				goto err;

			broken_list_write_lock(mnode);
			if(add_broken_pte_node(offset, mnode) < 0) {
				broken_list_write_unlock(mnode);
				goto err;
			}
			broken_list_write_unlock(mnode);
			printk(KERN_INFO "register broken pte %lx %d\n", make_ds_va((va_start >> 27) & PT_PGTABLE_MASK, (va_start >> 18) & PT_PGTABLE_MASK, (va_start >> 9) & PT_PGTABLE_MASK, offset), offset);
			
			member_write_lock(mnode);
			if(!mnode->dup_pte) {
				ptep_new = pte_realloc(current->mm);
				if(!ptep_new) {
					printk(KERN_INFO "out of memory\n");
					member_write_unlock(mnode);
					goto err;
				}
				printk(KERN_INFO "duplicate pte %lx", (unsigned long)__pa(ptep_new));
				update_dup_pgtable(va_start, ptep_new, mnode);
				mnode->dup_pte = ptep_new;
				
				member_write_unlock(mnode);
				// check ref count 
				if(recover_broken_pgtable(mnode) < 0) 
					goto err;
				goto end;
			}
			member_write_unlock(mnode);
			break;
		}
	}
end:
	return 0;
err:
	return -1;
}

SYSCALL_DEFINE1(mycall_register_broken_pte, unsigned long, va)
{
	return register_broken_pte_from_user(va);
}

int check_pte_is_broken_for_pte_write(pte_t *ptep)
{
	struct m_head_list *mhead;
	struct m_list *mnode;
	struct broken_pte_list *bnode;
	unsigned int offset;
	unsigned long va;
	pte_t entry;
	
	if(!ptep) { // NULL pointer
		return -1;
	}

	va = (unsigned long)ptep;

	entry = *ptep; // check EMEs and register broken pte

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			// register broken pte
			// if(recover_count == 200) {
			// 	register_broken_pte_and_make_recovery_thread(va);
			// 	recover_count++;
			// } else {
			// 	recover_count++;
			// }

			m_list_read_lock(mhead);
			list_for_each_entry(mnode, &mhead->head, list) {
				member_read_lock(mnode);
				if(mnode->base & PTE_FLAG_MASK && mnode->va <= va && va < mnode->va + OFFSET_SIZE) {
					offset = ((va - mnode->va) / 0x8) & PT_PGTABLE_MASK;
					broken_list_read_lock(mnode);
					list_for_each_entry(bnode, &mnode->broken_head, list) {
						if(offset == bnode->offset) {
							printk(KERN_INFO "Hit broken pte %lx %lx\n", va, make_ds_va((mnode->base >> 27) & PT_PGTABLE_MASK, (mnode->base >> 18) & PT_PGTABLE_MASK, (mnode->base >> 9) & PT_PGTABLE_MASK, offset));
							broken_list_read_unlock(mnode);
							member_read_unlock(mnode);
							m_list_read_unlock(mhead);
							return 1; // pte is broken
						}
					}
					broken_list_read_unlock(mnode);
					member_read_unlock(mnode);
					m_list_read_unlock(mhead);
					return 0; // pte is safety
				}
				member_read_unlock(mnode);
			}
			m_list_read_unlock(mhead);
			break;
		}
	}
	return -1; // pte is not managed by ds_list
}
EXPORT_SYMBOL_GPL(check_pte_is_broken_for_pte_write);

pte_t check_pte_is_broken_for_pte_read(pte_t *ptep)
{
	struct m_head_list *mhead;
	struct m_list *mnode;
	struct broken_pte_list *bnode;
	unsigned int offset;
	unsigned long va;
	pte_t *pte;
	pte_t entry;

	if(!ptep) { // NULL pointer
		return native_make_pte(0);
	}

	va = (unsigned long)ptep;

	entry = *ptep; // check EMEs and register broken pte

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			// register broken pte
			if(recover_count == 50) {
				register_broken_pte_and_make_recovery_thread(va);
				recover_count++;
			} else {
				recover_count++;
			}

			m_list_read_lock(mhead);
			list_for_each_entry(mnode, &mhead->head, list) {
				member_read_lock(mnode);
				if(mnode->base & PTE_FLAG_MASK && mnode->va <= va && va < mnode->va + OFFSET_SIZE) {
					offset = ((va - mnode->va) / 0x8) & PT_PGTABLE_MASK;
					broken_list_read_lock(mnode);
					list_for_each_entry(bnode, &mnode->broken_head, list) {
						if(offset == bnode->offset) {
							printk(KERN_INFO "Hit broken pte %lx %lx\n", va, make_ds_va((mnode->base >> 27) & PT_PGTABLE_MASK, (mnode->base >> 18) & PT_PGTABLE_MASK, (mnode->base >> 9) & PT_PGTABLE_MASK, offset));
							pte = mnode->dup_pte + offset;
							printk(KERN_INFO "get broken    pte %lx %lx\n", pte_pfn(*ptep), pte_flags(*ptep));
							printk(KERN_INFO "get recovered pte %lx %lx\n", pte_pfn(*pte), pte_flags(*pte));
							broken_list_read_unlock(mnode);
							member_read_unlock(mnode);
							m_list_read_unlock(mhead);
							return *pte; // pte is broken
						}
					}
					broken_list_read_unlock(mnode);
					member_read_unlock(mnode);
					break;
				}
				member_read_unlock(mnode);
			}
			m_list_read_unlock(mhead);
			break;
		}
	}
	return entry; // pte is safety or not managed by ds_list
}
EXPORT_SYMBOL_GPL(check_pte_is_broken_for_pte_read);

static int get_pmdp(pud_t *pudp, unsigned long pmd, pmd_t **pmdpp)
{
  	pmd_t *pmdp = pmd_offset_index(pudp, pmd);
	*(pmdpp) = pmdp;

  	if(pmd_none(*pmdp) || !pmd_present(*pmdp) || pmd_large(*pmdp)){
    	// printk(KERN_INFO "pmd %lu is not present.\n", pmd);
    	return -1;
  	}
  	return 0;
}

static int get_pudp(p4d_t *p4dp, unsigned long pud, pud_t **pudpp)
{
  	pud_t *pudp = pud_offset_index(p4dp, pud);
	*(pudpp) = pudp;
	
  	if(pud_none(*pudp) || !pud_present(*pudp) || pud_large(*pudp)){
	    // printk(KERN_INFO "pud %lu is not present", pud);
	    return -1;
  	}
  	return 0;  
}

static int get_p4dp(pgd_t *pgdp, unsigned long p4d, p4d_t **p4dpp)
{
  	p4d_t *p4dp = p4d_offset_index(pgdp, p4d);
	*(p4dpp) = p4dp;
	
	if(p4d_none(*p4dp) || !p4d_present(*p4dp)){
	    // printk(KERN_INFO "p4d %lu is not present", pgd);
    	return -1;
  	}
	return 0;
}

static int get_pgdp(struct mm_struct *mm, unsigned long pgd, p4d_t **p4dpp)
{
	pgd_t *pgdp = pgd_offset_index(mm, pgd);

  	if(pgd_none(*pgdp) || !pgd_present(*pgdp)){
	    // printk(KERN_INFO "pgd %lu is not present.\n", pgd);
    	return -1;
  	}

	if(get_p4dp(pgdp, pgd, p4dpp) < 0){
		return -1;
	}
  	return 0;
}

static int get_pgdp_for_kthread(unsigned long pgd, p4d_t **p4dpp)
{
	pgd_t *pgdp;

	printk(KERN_INFO "start get pgdp\n");

	pgdp = current_kproc->mm->pgd + pgd;

	printk(KERN_INFO "before: success to get pgdp\n");

  	if(pgd_none(*pgdp) || !pgd_present(*pgdp)){
	    // printk(KERN_INFO "pgd %lu is not present.\n", pgd);
    	return -1;
  	}
	printk(KERN_INFO "after: success to get pgdp %lx\n", (unsigned long)pgd_val(*pgdp));

	if(get_p4dp(pgdp, pgd, p4dpp) < 0){
		return -1;
	}
  	return 0;
}

static int get_pmdp_from_pgtable(unsigned long base, pmd_t **pmdpp)
{
	p4d_t *p4dp;
	pud_t *pudp;

	unsigned long pgd = (base >> 27) & PT_PGTABLE_MASK;
	unsigned long pud = (base >> 18) & PT_PGTABLE_MASK;
	unsigned long pmd = (base >> 9) & PT_PGTABLE_MASK;

  	if(get_pgdp(current->mm, pgd, &p4dp) == 0) {
		if(get_pudp(p4dp, pud, &pudp) == 0) {
			if(get_pmdp(pudp, pmd, pmdpp) == 0) {
				return 0;
			}
		}
	}
	return -1;
}

static int update_pmdp(struct m_list *mnode, pmd_t *pmdp)
{
	struct broken_pte_list *bnode;

	broken_list_read_lock(mnode);
	if(list_empty(&mnode->broken_head)) {
		broken_list_read_unlock(mnode);
		return -1;
	}
	
	printk(KERN_INFO "recover broken pte %lx\n",mnode->base);

	list_for_each_entry(bnode, &mnode->broken_head, list) {
		printk(KERN_INFO "        broken pte %d\n", bnode->offset);
	}
	broken_list_read_unlock(mnode);

	printk(KERN_INFO "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));

	member_write_lock(mnode);
	print_dup_pte(mnode->dup_pte, mnode->base);
	pmd_reinstall(current->mm, pmdp, mnode->dup_pte);
	modify_m_va(mnode, (unsigned long)mnode->dup_pte);
	mnode->dup_pte = NULL;
	member_write_unlock(mnode);
	delete_broken_pte_all(mnode);

	printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
	return 0;
}

static int recover_broken_pgtable(struct m_list *mnode)
{
	struct mmu_gather *tlb;
	pmd_t *pmdp;
	spinlock_t *ptl;
	unsigned long va = mnode->base & PT_PGTABLE_MASK_NOT;

	printk(KERN_INFO "start recovery %lx\n",va);
	// here
	if(get_pmdp_from_pgtable(va, &pmdp) < 0)
		return -1;

	ptl = ptlock_ptr(pmd_to_page(pmdp));
	spin_lock(ptl);
	printk(KERN_INFO "get pmd lock\n");

	for(;;) {
		ref_count_lock(mnode);
		printk(KERN_INFO "pgtable ref count %d\n", mnode->ref_count);
		if(mnode->ref_count == 0) {
			if(update_pmdp(mnode, pmdp) < 0) {
				ref_count_unlock(mnode);
				spin_unlock(ptl);
				printk(KERN_INFO "cannot recovery\n");
				return -1;
			}
			ref_count_unlock(mnode);
			break;
		}
		ref_count_unlock(mnode);
		// wait some time msec
		// msleep(100);
	}

	spin_unlock(ptl);

	// want to add TLB flush operation
	// tlb_gather_mmu(tlb, current->mm);
	// pte_free_tlb(tlb, pmd_pgtable(*pmdp), va << OFFSET_SHIFT);
	// tlb_finish_mmu(tlb);

	printk(KERN_INFO "finish recovery\n");

	return 0;
}

// int recover_broken_pte_from_pgtable_va(unsigned long va)
// {
// 	struct m_head_list *mhead;
// 	struct m_list *mnode;
// 	struct broken_pte_list *bnode;
// 	unsigned long va_start = MAX_NUM;
// 	pmd_t *pmdp;

// 	list_for_each_entry(mhead, &user_head, list) {
// 		if(mhead->pid == current->tgid) {
// 			m_list_read_lock(mhead);
// 			list_for_each_entry(mnode, &mhead->head, list) {
// 				member_read_lock(mnode);
// 				if(mnode->base & PTE_FLAG_MASK && mnode->va <= va && va < mnode->va + OFFSET_SIZE) {
// 					va_start = mnode->base & PT_PGTABLE_MASK_NOT;
// 					if(!mnode->dup_pte) {
// 						member_read_unlock(mnode);
// 						m_list_read_unlock(mhead);
// 						printk(KERN_INFO "NOT registerd broken pte %lx\n",va_start);
// 						goto err;
// 					}
// 					member_read_unlock(mnode);
// 					printk(KERN_INFO "recover broken pte %lx\n",va_start);
					
// 					broken_list_read_lock(mnode);
// 					list_for_each_entry(bnode, &mnode->broken_head, list) {
// 						printk(KERN_INFO "        broken pte %d\n", bnode->offset);
// 					}
// 					broken_list_read_unlock(mnode);
// 					break;
// 				}
// 				member_read_unlock(mnode);
// 			}
// 			m_list_read_unlock(mhead);

// 			if(va_start == MAX_NUM)
// 				goto err;

// 			if(get_pmdp_from_pgtable(va_start, &pmdp) == 0) {
// 				printk(KERN_INFO "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));

// 				member_write_lock(mnode);
// 				pmd_reinstall_lock(current->mm, pmdp, mnode->dup_pte);
// 				modify_m_va(mnode, (unsigned long)mnode->dup_pte);
// 				mnode->dup_pte = NULL;
// 				member_write_unlock(mnode);
// 				delete_broken_pte_all(mnode);

// 				printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
// 			}
// 			break;
// 		}
// 	}
// 	return 0;
// err:
// 	return -1;
// }
// EXPORT_SYMBOL_GPL(recover_broken_pte_from_pgtable_va);

static long recover_broken_pte_from_user(unsigned long va)
{
	struct m_head_list *mhead;
	struct m_list *mnode;
	struct broken_pte_list *bnode;
	unsigned long base = va >> OFFSET_SHIFT;
	unsigned long va_start = MAX_NUM;
	pmd_t *pmdp;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			m_list_read_lock(mhead);
			list_for_each_entry(mnode, &mhead->head, list) {
				member_read_lock(mnode);
				if(mnode->base & PTE_FLAG_MASK && (mnode->base & PT_PGTABLE_MASK_NOT) == (base & PT_PGTABLE_MASK_NOT)) {
					va_start = mnode->base & PT_PGTABLE_MASK_NOT;
					if(!mnode->dup_pte) {
						member_read_unlock(mnode);
						m_list_read_unlock(mhead);
						printk(KERN_INFO "NOT registerd broken pte %lx\n",va_start);
						goto err;
					}
					member_read_unlock(mnode);
					printk(KERN_INFO "recover broken pte %lx\n",va_start);
					
					broken_list_read_lock(mnode);
					list_for_each_entry(bnode, &mnode->broken_head, list) {
						printk(KERN_INFO "        broken pte %d\n", bnode->offset);
					}
					broken_list_read_unlock(mnode);
					break;
				}
				member_read_unlock(mnode);
			}
			m_list_read_unlock(mhead);

			if(va_start == MAX_NUM)
				goto err;

			if(get_pmdp_from_pgtable(va_start, &pmdp) == 0) {
				printk(KERN_INFO "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));

				member_write_lock(mnode);
				pmd_reinstall_lock(current->mm, pmdp, mnode->dup_pte);
				modify_m_va(mnode, (unsigned long)mnode->dup_pte);
				mnode->dup_pte = NULL;
				member_write_unlock(mnode);
				delete_broken_pte_all(mnode);

				printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
			}
			break;
		}
	}
	return 0;
err:
	return -1;
}


SYSCALL_DEFINE1(mycall_recover_broken_pte, unsigned long, va)
{
	return recover_broken_pte_from_user(va);
}

int increment_m_list_ref_count(unsigned long va)
{
	struct m_head_list *mhead;
	struct m_list *mnode;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			m_list_read_lock(mhead);
			list_for_each_entry(mnode, &mhead->head, list) {
				member_read_lock(mnode);
				if(mnode->base & PTE_FLAG_MASK && mnode->va <= va && va < mnode->va + OFFSET_SIZE) {
					member_read_unlock(mnode);

					// broken_list_read_lock(mnode);
					// if(!list_empty(&mnode->broken_head)) {
					ref_count_lock(mnode);
					mnode->ref_count++;
					// printk(KERN_INFO "m %lx ref count %d\n", mnode->base, mnode->ref_count);
					ref_count_unlock(mnode);
					// }
					// broken_list_read_unlock(mnode);
					break;
				}
				member_read_unlock(mnode);
			}
			m_list_read_unlock(mhead);
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(increment_m_list_ref_count);

int decrement_m_list_ref_count(unsigned long va)
{
	struct m_head_list *mhead;
	struct m_list *mnode;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			m_list_read_lock(mhead);
			list_for_each_entry(mnode, &mhead->head, list) {
				member_read_lock(mnode);
				if(mnode->base & PTE_FLAG_MASK && mnode->va <= va && va < mnode->va + OFFSET_SIZE) {
					member_read_unlock(mnode);

					// broken_list_read_lock(mnode);
					// if(!list_empty(&mnode->broken_head)) {
					ref_count_lock(mnode);
					// if(mnode->ref_count > 0) {
					mnode->ref_count--;
					// }
					// printk(KERN_INFO "m %lx ref count %d\n", mnode->base, mnode->ref_count);
					ref_count_unlock(mnode);
					// }
					// broken_list_read_unlock(mnode);
					break;
				}
				member_read_unlock(mnode);
			}
			m_list_read_unlock(mhead);
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(decrement_m_list_ref_count);

static int get_pmdp_for_kthread(unsigned long base, pmd_t **pmdpp)
{
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;
	// struct mm_struct *mm;
	// pgd_t *pgdp;

	unsigned long pgd = (base >> 27) & PT_PGTABLE_MASK;
	unsigned long pud = (base >> 18) & PT_PGTABLE_MASK;
	unsigned long pmd = (base >> 9) & PT_PGTABLE_MASK;

	// pgdp = current_mm->pgd;

	printk(KERN_INFO "check pgd\n");
	if (current_kproc->mm->pgd != NULL) {
		printk(KERN_INFO "pgd is active\n");
	} else {
		printk(KERN_INFO "pgd is NULL\n");
	}

  	if(get_pgdp_for_kthread(pgd, &p4dp) == 0) {
		printk(KERN_INFO "success to get p4dp %lx\n", (unsigned long)p4d_val(*p4dp));
		if(get_pudp(p4dp, pud, &pudp) == 0) {
			printk(KERN_INFO "success to get pudp %lx\n", (unsigned long)pud_val(*pudp));
			if(get_pmdp(pudp, pmd, &pmdp) == 0) {
				*(pmdpp) = pmdp;
				printk(KERN_INFO "success to get pmdp %lx\n", (unsigned long)pmd_val(*pmdp));
				return 0;
			}
		}
	}
	return -1;
}

static void print_dup_pte(pte_t *ptep, unsigned long base)
{
	unsigned long pgd = (base >> 27) & PT_PGTABLE_MASK;
	unsigned long pud = (base >> 18) & PT_PGTABLE_MASK;
	unsigned long pmd = (base >> 9) & PT_PGTABLE_MASK;

	printk(KERN_INFO "print dup pte\n");
	for(unsigned long pte=0; pte<MAX; pte++, ptep++) {
		if(!pte_none(*ptep) && pte_present(*ptep)) {
			printk(KERN_INFO "  %lx  %lx %lx  %lx\n", make_ds_va(pgd, pud, pmd, pte), pte_pfn(*ptep), pte_flags(*ptep), (unsigned long)ptep);
		}
	}
}

static int update_pmdp_for_kthread(struct m_list *mnode, pmd_t *pmdp)
{
	struct broken_pte_list *bnode;

	broken_list_read_lock(mnode);
	if(list_empty(&mnode->broken_head)) {
		broken_list_read_unlock(mnode);
		return -1;
	}
	
	printk(KERN_INFO "recover broken pte %lx\n",mnode->base);

	list_for_each_entry(bnode, &mnode->broken_head, list) {
		printk(KERN_INFO "        broken pte %d\n", bnode->offset);
	}
	broken_list_read_unlock(mnode);

	printk(KERN_INFO "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));

	member_write_lock(mnode);
	print_dup_pte(mnode->dup_pte, mnode->base);
	pmd_reinstall(current_kproc->mm, pmdp, mnode->dup_pte);
	modify_m_va(mnode, (unsigned long)mnode->dup_pte);
	mnode->dup_pte = NULL;
	member_write_unlock(mnode);
	delete_broken_pte_all(mnode);

	printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
	return 0;
}

static int recovery_thread(void *data)
{
	struct m_list *mnode = (struct m_list *)data;
	pmd_t *pmdp;
	spinlock_t *ptl;
	unsigned long va;
	
	member_read_lock(mnode);
	va = mnode->base & PT_PGTABLE_MASK_NOT;
	member_read_unlock(mnode);

	printk(KERN_INFO "start recovery thread %lx\n",va);
	printk(KERN_INFO "current %d %d, current kproc %d %d\n", current->pid, current->tgid, current_kproc->pid, current_kproc->tgid);

	if (get_pmdp_for_kthread(va, &pmdp) < 0) {
		printk(KERN_INFO "kthread not get pmdp\n");
		return -1;
	}
	
	printk(KERN_INFO "before: kthread get pmd lock\n");
	ptl = ptlock_ptr(pmd_to_page(pmdp));
	spin_lock(ptl);
	printk(KERN_INFO "after: kthread get pmd lock\n");

	for(int i=0; i < 100; i++) {
		ref_count_lock(mnode);
		printk(KERN_INFO "pgtable ref count %d\n", mnode->ref_count);
		if(mnode->ref_count == 0) {
			if(update_pmdp_for_kthread(mnode, pmdp) < 0) {
				ref_count_unlock(mnode);
				spin_unlock(ptl);
				printk(KERN_INFO "cannot recovery\n");
				return -1;
			}
			ref_count_unlock(mnode);
			break;
		}
		ref_count_unlock(mnode);
		// wait some time msec
		msleep(50);
	}
	spin_unlock(ptl);

	printk(KERN_INFO "finish kthread\n");

	return 0;
}

static int make_recovery_thread(struct m_list *mnode)
{
	current_kproc = get_pid_task(find_get_pid(current->tgid), PIDTYPE_PID);
	if (!current_kproc) {
		printk(KERN_INFO "error: cannot get kernel proc\n");
		return -1;
	}
	// current_mm = current->mm;
	// current_pgd = current->mm->pgd;
	// preempt_enable();
	printk(KERN_INFO "current %d %d, current kproc %d %d\n", current->pid, current->tgid, current_kproc->pid, current_kproc->tgid);
	k_thread = kthread_run(recovery_thread, mnode, "kcheckd");
	if(IS_ERR(k_thread)) {
		printk(KERN_INFO "kthread_run error\n");
		return -1;
	}
	// preempt_disable();
	printk(KERN_INFO "recovery thread run\n");
	return 0;
}

int wait_to_recover_broken_pgtable(unsigned long pmd_va) 
{
	struct m_head_list *mhead;
	struct m_list *mnode;
	unsigned long base = 0;
	int flag = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			m_list_read_lock(mhead);
			list_for_each_entry(mnode, &mhead->head, list) {
				member_read_lock(mnode);
				if(!flag && mnode->base & PMD_FLAG_MASK && mnode->va <= pmd_va && pmd_va < mnode->va + OFFSET_SIZE) {
					base = make_ds_va((mnode->base >> 27) & PT_PGTABLE_MASK, (mnode->base >> 18) & PT_PGTABLE_MASK,  ((pmd_va - mnode->va) / 0x8) & PT_PGTABLE_MASK, PTE_FLAG_MASK & PT_PGTABLE_MASK);
					flag = 1;
				}
				if(flag && mnode->base == base) {
					if(mnode->dup_pte == NULL){
						member_read_unlock(mnode);
						m_list_read_unlock(mhead);
						return 0;
					}

					member_read_unlock(mnode);
					printk(KERN_INFO "wait to recover broken pgtable %lx\n",base);
					for(int i=0; i < 100; i++) {
						member_read_lock(mnode);
						if(mnode->dup_pte == NULL) {
							member_read_unlock(mnode);
							m_list_read_unlock(mhead);
							printk(KERN_INFO "DONE dup_pte\n");
							return 0;
						}
						member_read_unlock(mnode);
						// wait some time msec
						msleep(100);
					}
					break;
				}
				member_read_unlock(mnode);
			}
			m_list_read_unlock(mhead);
			break;
		}
	}
	return 0;		
}
EXPORT_SYMBOL_GPL(wait_to_recover_broken_pgtable);

// static int make_list_usr_from_pgtable(unsigned long addr, pte_t *ptep)
// {
// 	struct m_head_list *m_head;
// 	struct ds_head_list *ds_head;
// 	struct ds_list *dnode, *next, *prev;
// 	unsigned long pte_value = pte_pfn(*ptep);
// 	unsigned long pte_flag = pte_flags(*ptep);

// 	list_for_each_entry(m_head, &user_head, list){
// 		if(m_head->pid == current->pid){
// 			if(is_add_m_node_usr(addr & PT_PGTABLE_MASK_NOT, m_head)){
// 				if(add_m_node_usr((unsigned long)ptep, addr & PT_PGTABLE_MASK_NOT, m_head) < 0)
// 					return -ENOMEM;
// 			}
// 		}
// 	}


// 	list_for_each_entry(ds_head, &usr_ds_head, list){
// 		if(ds_head->pid == current->pid){
// 			if((dnode = make_ds_node(addr, addr+1, make_ds_offset(addr, pte_value), pte_flag)) == NULL)
// 				return -ENOMEM;

// 			// incert dnode
// 			if(list_empty(&ds_head->head)){ //no node
// 				list_add(&dnode->list, &ds_head->head);
// 			}else{
// 				list_for_each_entry(next, &ds_head->head, list){
// 					if(dnode->limit <= next->base){
// 						list_add_tail(&dnode->list, &next->list);
// 						if(list_is_first(&dnode->list, &ds_head->head)){
// 							ds_node_merge(dnode, next);
// 							goto end;
// 						}
// 						prev = list_prev_entry(dnode, list);
// 						break;
// 					}
// 					if(list_is_last(&next->list, &ds_head->head)){
// 						list_add_tail(&dnode->list, &ds_head->head);
// 						prev = list_prev_entry(dnode, list);
// 						ds_node_merge(prev, dnode);
// 						goto end;
// 					}
// 				}
// 				ds_node_merge(dnode, next);
// 				ds_node_merge(prev, dnode);
// 				goto end;
// 			}
// 		}
// 	}
// end:
// 	printk(KERN_INFO "make ds base: %ld\n", addr);
// 	return 0;
	
// }

// static int make_list_ker_from_pgtable(unsigned long addr, pte_t *ptep)
// {
// 	struct ds_list *dnode, *next, *prev;
// 	unsigned long pte_value = pte_pfn(*ptep);
// 	unsigned long pte_flag = pte_flags(*ptep);

// 	if((dnode = make_ds_node(addr, addr+1, make_ds_offset(addr, pte_value), pte_flag)) == NULL)
// 		return -ENOMEM;

// 	if(is_add_m_node_ker(addr & PT_PGTABLE_MASK_NOT))
// 		if(add_m_node_ker((unsigned long)ptep, addr & PT_PGTABLE_MASK_NOT) < 0)
// 			return -ENOMEM;
		
// 	// incert dnode
// 	if(list_empty(&ker_ds_head)){ //no node
// 		list_add(&dnode->list, &ker_ds_head);
// 	}else{
// 		list_for_each_entry(next, &ker_ds_head, list){
// 			if(dnode->limit <= next->base){
// 				list_add_tail(&dnode->list, &next->list);
// 				if(list_is_first(&dnode->list, &ker_ds_head)){
// 					ds_node_merge(dnode, next);
// 					goto end;
// 				}
// 				prev = list_prev_entry(dnode, list);
// 				break;
// 			}
// 			if(list_is_last(&next->list, &ker_ds_head)){
// 				list_add_tail(&dnode->list, &ker_ds_head);
// 				prev = list_prev_entry(dnode, list);
// 				ds_node_merge(prev, dnode);
// 				goto end;
// 			}
// 		}
// 		ds_node_merge(dnode, next);
// 		ds_node_merge(prev, dnode);
// 	}
// end:
// 	printk(KERN_INFO "make ds base: %ld\n", addr);
// 	return 0;
	
// }

// int make_list_from_pgtable(unsigned long address, pte_t *ptep)
// {
// 	printk(KERN_INFO "va:%ld pteva:%ld",address, (unsigned long)ptep);
// 	if(address < USER_MAX_ADDRESS)
// 		return make_list_usr_from_pgtable(address, ptep);
// 	// return make_list_ker_from_pgtable(address, ptep);
// 	return 0;
// }

static int get_ptep_and_make_m_list(pmd_t *pmdp, pid_t pid, unsigned long pte, pte_t **ptepp)
{
  	pte_t *ptep = pte_offset_index(pmdp, pte);
	*(ptepp) = ptep;

  	if(pte_none(*ptep) || !pte_present(*ptep)) {
    	// printk(KERN_INFO "pte %lu is not present.\n", pte);
    	return -1;
  	}

  	return 0;
}

static int get_pmdp_and_make_m_list(pud_t *pudp, pid_t pid, unsigned long pmd, pmd_t **pmdpp)
{
  	pmd_t *pmdp = pmd_offset_index(pudp, pmd);
	*(pmdpp) = pmdp;

  	if(pmd_none(*pmdp) || !pmd_present(*pmdp) || pmd_large(*pmdp)){
    	// printk(KERN_INFO "pmd %lu is not present.\n", pmd);
    	return -1;
  	}

	__make_pte_m_list((unsigned long)pmdp, pmd_page_vaddr(*pmdp), pid);
	
  	return 0;
}

static int get_pudp_and_make_m_list(p4d_t *p4dp, pid_t pid, unsigned long pud, pud_t **pudpp)
{
  	pud_t *pudp = pud_offset_index(p4dp, pud);
	*(pudpp) = pudp;
	
  	if(pud_none(*pudp) || !pud_present(*pudp) || pud_large(*pudp)){
	    // printk(KERN_INFO "pud %lu is not present", pud);
	    return -1;
  	}

	__make_pmd_m_list((unsigned long)pudp, (unsigned long)pud_pgtable(*pudp), pid);
	
  	return 0;  
}

static int get_p4dp_and_make_m_list(pgd_t *pgdp, pid_t pid, unsigned long p4d, p4d_t **p4dpp)
{
  	p4d_t *p4dp = p4d_offset_index(pgdp, p4d);
	*(p4dpp) = p4dp;
	
	if(p4d_none(*p4dp) || !p4d_present(*p4dp)){
	    // printk(KERN_INFO "p4d %lu is not present", pgd);
    	return -1;
  	}
	
	__make_pud_m_list((unsigned long)p4dp, (unsigned long)p4d_pgtable(*p4dp), pid);
	
	return 0;
}

static int get_pgdp_and_make_m_list(struct mm_struct *mm, pid_t pid, unsigned long pgd, p4d_t **p4dpp)
{
  	pgd_t *pgdp = pgd_offset_index(mm, pgd);

  	if(pgd_none(*pgdp) || !pgd_present(*pgdp)){
	    // printk(KERN_INFO "pgd %lu is not present.\n", pgd);
    	return -1;
  	}

	if(get_p4dp_and_make_m_list(pgdp, pid, pgd, p4dpp) < 0){
		return -1;
	}

  	return 0;
}

static long make_ds_list_usr_from_pgtable(struct task_struct *p)
{
	pid_t pid = p->tgid;
	// pgd_t *pgdp;
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep;
	
	// if p have already registered, retur -1
	if(__make_pgd_m_list((unsigned long)p->mm->pgd, pid) < 0) {
		printk(KERN_INFO "Have already made m list\n");
		return 0;
	}

	for(unsigned long pgd=0; pgd < USER_MAX; pgd++) {
		if(get_pgdp_and_make_m_list(p->mm, pid, pgd, &p4dp) == 0) {
			for(unsigned long pud=0; pud<MAX; pud++) {
				if(get_pudp_and_make_m_list(p4dp, pid, pud, &pudp) == 0) {
					for(unsigned long pmd=0; pmd<MAX; pmd++) {
						if(get_pmdp_and_make_m_list(pudp, pid, pmd, &pmdp) == 0) {
							for(unsigned long pte=0; pte<MAX; pte++) {
			                	if(get_ptep_and_make_m_list(pmdp, pid, pte, &ptep) == 0) {
									// make_ds from ptep
									if(__make_ds_list_usr((unsigned long)ptep, *ptep, pid) < 0) {
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
	recover_count = 0;
// end:
	return 0;
}

SYSCALL_DEFINE0(mycall_make_ds_usr_from_pgtable)
{
	long ret;
	ktime_t start, end;

	start = ktime_get();
	ret = make_ds_list_usr_from_pgtable(current);
	end = ktime_get();

	printk(KERN_INFO "make_ds_usr time: %lld\n", ktime_sub(end, start));
	
	return ret;
}

// static long make_usr_ds(void)
// {
// 	pte_t *ptep;
	
// 	int num;
// 	int count;
// 	// int flag=0;
		
// 	unsigned long pte_num;
// 	for(unsigned long a=0; a<USER_MAX; a++){
//         	for(unsigned long b=0; b<MAX; b++){
//             		for(unsigned long c=0; c<MAX; c++){
//                 		for(unsigned long d=0; d<MAX; d++){
//                     			if((num = search_pgtable_get_pfn(a, b, c, d, &ptep)) > 0 && num < 4){ //pte hit
// 						pte_num = make_ds_va(a, b, c, d);
// 						// if(flag == 0){
// 						// 	vaddr = (unsigned long)ptep;
// 						// 	flag = 1;
// 						// }
// 						if(make_list_usr_from_pgtable(pte_num, ptep) < 0)
// 							goto end;
						
//                         			count = num;
//                     			}else if(num == 0){ // error
// 						goto end;
// 					}else{
//                         			count = num - 3;
//                     			}
//                     			num = 0;
//                     			if(--count > 0)
//                         			break;
//                     			count = 0;
//                 		}
//                 		if(--count > 0)
//                   			break;
//                 		count = 0;
//             		}
//             		if(--count > 0)
//               			break;
//             		count = 0;
//         	}
//         	if(--count > 0)
//           		break;
//         	count = 0;
//     	}
// end:
	
// 	return 0;
// }

// static long make_ker_ds(void)
// {
// 	pte_t *ptep;
	
// 	int num;
// 	int count;

// 	unsigned long pte_num;
	
// 	for(unsigned long a=USER_MAX; a<MAX; a++){
//         	for(unsigned long b=0; b<MAX; b++){
//             		for(unsigned long c=0; c<MAX; c++){
//                 		for(unsigned long d=0; d<MAX; d++){
//                     			if((num = search_pgtable_get_pfn(a, b, c, d, &ptep)) > 0 && num < 4){ //pte hit
// 						pte_num = make_ds_va(a, b, c, d);
// 						if(make_list_ker_from_pgtable(pte_num, ptep) < 0)
// 							goto end;
						
//                         			count = num;
//                     			}else if(num == 0){ // error
// 						goto end;
// 					}else{ // pte miss
//                         			count = num - 3;
//                     			}
//                     			num = 0;
//                     			if(--count > 0)
//                         			break;
//                     			count = 0;
//                 		}
//                 		if(--count > 0)
//                   			break;
//                 		count = 0;
//             		}
//             		if(--count > 0)
//               			break;
//             		count = 0;
//         	}
//         	if(--count > 0)
//           		break;
//         	count = 0;
//     	}
// end:
// 	return 0;
// }

SYSCALL_DEFINE0(mycall_ds_make)
{
	long ret1 = 0;
	long ret2 = 0;
	// ret1 = make_usr_ds();
	// ret2 = make_ker_ds();
	
	if(ret1 == ret2)
		return 0;
   	return -1;
}

SYSCALL_DEFINE0(mycall_ds_make_user)
{
	long ret = 0;
	ktime_t start, end;

	start = ktime_get();
	// ret = make_usr_ds();
	end = ktime_get();

	printk(KERN_INFO "make_ds_usr time: %lld\n", ktime_sub(end, start));
	
	return ret;
}

SYSCALL_DEFINE0(mycall_ds_make_kernel)
{
	long ret = 0;
	ktime_t start, end;

	start = ktime_get();
	// ret = make_ker_ds();
	end = ktime_get();

	printk(KERN_INFO "make_ds_ker time: %lld\n", ktime_sub(end, start));
	
	return ret;
}

static long register_pid(pid_t pid)
{
	struct m_head_list *m_head;

	// target_task = current;
	
	list_for_each_entry(m_head, &user_head, list) {
		if(m_head->pid == pid) {
			printk(KERN_INFO "Have already registered the pid %d\n", pid);
			goto end;
		}
	}

	m_head = kmalloc(sizeof(struct m_head_list), GFP_KERNEL);
	if(!m_head)
		return -1;
	m_head->pid = pid;
	rwlock_init(&m_head->m_lock);
	INIT_LIST_HEAD(&m_head->head);
	list_add(&m_head->list, &user_head);

	printk(KERN_INFO "init pid %d\n",pid);
end:
	return 0;
}

SYSCALL_DEFINE0(mycall_ds_register_pid)
{
	return register_pid(current->tgid);
}

bool check_parent_is_target(pid_t ppid, pid_t pid)
{
	struct m_head_list *m_node;

	list_for_each_entry(m_node, &user_head, list) {
		if(m_node->pid == ppid) {
			printk(KERN_INFO "parent pid %d, child pid %d\n", ppid, pid);
			return true;
		}
	}
	return false;	
}
EXPORT_SYMBOL_GPL(check_parent_is_target);

void register_child(struct task_struct *p)
{
	// register pid & make ds_list, m_list
	printk(KERN_INFO "child pid %d, current tid %d, current pid %d\n", p->tgid, current->pid, current->tgid);
	register_pid(p->tgid);
	// print_user_pgtable(p);
	make_ds_list_usr_from_pgtable(p);
}
EXPORT_SYMBOL_GPL(register_child);
