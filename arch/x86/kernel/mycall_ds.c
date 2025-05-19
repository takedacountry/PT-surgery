#include <linux/syscalls.h> //
// #include <linux/printk.h>
// #include <linux/types.h>
// #include <linux/mm.h>
// #include <linux/rwlock.h> // 
// #include <linux/spinlock.h> //
#include <linux/export.h>
#include <linux/delay.h> //
#include <linux/kthread.h> //
#include <linux/preempt.h> //
#include <linux/pid.h>
#include <linux/hugetlb.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <linux/oom.h>
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

// struct task_struct *target_task;

static int make_recovery_thread(struct page *pte_page);
static int recover_broken_pgtable(struct mm_struct *mm, struct m_head_struct *mhead, struct page *pte_page);

static struct task_struct *k_thread;
static pid_t current_tgid = 0;
static int recover_count = 0;

static int __make_pgd_m_list(pgd_t *pgd, pid_t pid)
{
	struct page *pgd_page;
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == pid) {
			pgd_page = virt_to_page((pgd_t *)(((unsigned long)pgd) & PAGE_MASK));	/* get pgd page */
			if (pgd_page->base & PGD_FLAG_MASK)	/* have already registered */
				return -1;

			pgd_page->base = PGD_FLAG_MASK;					/* update pgd page, (and duplicate pgd,pud,pmd) */
			printk(KERN_INFO "make m pgd alloc %lx, %lx, %d, %d\n", (((unsigned long)pgd) & PAGE_MASK), PGD_FLAG_MASK, current->pid, current->tgid);
		}
	}
	return 0;
}

int make_pgd_m_list(pgd_t *pgd)
{
	return __make_pgd_m_list(pgd, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pgd_m_list);

static unsigned long get_p4d_base(p4d_t *p4d, struct page *p4d_page)
{
	unsigned long base = 0;

	if (p4d_page->base & PGD_FLAG_MASK) 
		base = make_ds_va(((((unsigned long)p4d) & OFFSET_MASK) / 0x8) & PT_PGTABLE_MASK, 0, 0, PUD_FLAG_MASK & PT_PGTABLE_MASK);

	return base;
}

static int __make_pud_m_list(p4d_t *p4d, pud_t *pud, pid_t pid)
{
	struct page *p4d_page;
	struct page *pud_page;
	struct m_head_struct *mhead;
	unsigned long base;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == pid) {
			p4d_page = virt_to_page((p4d_t *)(((unsigned long)p4d) & PAGE_MASK)); /* get p4d page */
			pud_page = virt_to_page((pud_t *)(((unsigned long)pud) & PAGE_MASK)); /* get pud page */
			if ((base = get_p4d_base(p4d, p4d_page)) != 0) {	/* calculate base, update pud page */
				pud_page->base = base;
				printk(KERN_INFO "make m pud alloc %lx, %lx, %d, %d\n", (((unsigned long)pud) & PAGE_MASK), pud_page->base, current->pid, current->tgid);
			}
		}
	}
	return 0;
}

int make_pud_m_list(p4d_t *p4d, pud_t *pud)
{
	return __make_pud_m_list(p4d, pud, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pud_m_list);

static unsigned long get_pud_base(pud_t *pud, struct page *pud_page)
{
	unsigned long base = 0;

	if (pud_page->base & PUD_FLAG_MASK) 
		base = make_ds_va((pud_page->base >> 27) & PT_PGTABLE_MASK, ((((unsigned long)pud) & OFFSET_MASK) / 0x8) & PT_PGTABLE_MASK, 0, PMD_FLAG_MASK & PT_PGTABLE_MASK);

	return base;
}

static int __make_pmd_m_list(pud_t *pud, pmd_t *pmd, pid_t pid)
{
	struct page *pud_page;
	struct page *pmd_page;
	struct m_head_struct *mhead;
	unsigned long base;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == pid) {
			pud_page = virt_to_page((pud_t *)(((unsigned long)pud) & PAGE_MASK)); /* get pud page */
			pmd_page = virt_to_page((pmd_t *)(((unsigned long)pmd) & PAGE_MASK)); /* get pmd page */
			if ((base = get_pud_base(pud, pud_page)) != 0) {	/* calculate base, update pmd page */
				pmd_page->base = base;
				printk(KERN_INFO "make m pmd alloc %lx, %lx, %d, %d\n", (((unsigned long)pmd) & PAGE_MASK), pmd_page->base, current->pid, current->tgid);
			}
		}
	}
	return 0;
}

int make_pmd_m_list(pud_t *pud, pmd_t *pmd)
{
	return __make_pmd_m_list(pud, pmd, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pmd_m_list);

static unsigned long get_pmd_base(pmd_t *pmd, struct page *pmd_page)
{
	unsigned long base = 0;

	if (pmd_page->base & PMD_FLAG_MASK) 
		base = make_ds_va((pmd_page->base >> 27) & PT_PGTABLE_MASK, (pmd_page->base >> 18) & PT_PGTABLE_MASK,  ((((unsigned long)pmd) & OFFSET_MASK) / 0x8)  & PT_PGTABLE_MASK, PTE_FLAG_MASK & PT_PGTABLE_MASK);
	
	return base;
}

static int __make_pte_m_list(pmd_t *pmd, pte_t *pte, pid_t pid)
{
	struct page *pmd_page;
	struct page *pte_page;
	struct m_head_struct *mhead;
	unsigned long base;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == pid) {
			pmd_page = virt_to_page((pmd_t *)(((unsigned long)pmd) & PAGE_MASK)); /* get pmd page */
			pte_page = virt_to_page((pte_t *)(((unsigned long)pte) & PAGE_MASK)); /* get pte page */
			if ((base = get_pmd_base(pmd, pmd_page)) != 0) {	/* calculate base, update pte page */
				pte_page->base = base;
				printk(KERN_INFO "make m pte alloc %lx, %lx, %d, %d\n", (((unsigned long)pte) & PAGE_MASK), pte_page->base, current->pid, current->tgid);
			}
		}
	}
	return 0;
}

int make_pte_m_list(pmd_t *pmd, pte_t *pte) 
{
	return __make_pte_m_list(pmd, pte, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pte_m_list);

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
		if((next = make_ds_node(new->limit, ds_node->limit, ds_node->offset, ds_node->flag)) == NULL)
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
		if((next = make_ds_node(new->limit, ds_node->limit, ds_node->offset, ds_node->flag)) == NULL)
			return;
		ds_node->limit = new->base;
		list_add(&new->list, &ds_node->list);
		list_add(&next->list, &new->list);
	}
	return;
}

static void delete_ds(struct ds_log *ds_node, struct ds_log *new)
{
	struct ds_log *next;

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

static bool is_ds_write(struct ds_log *ds_node)
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

static int __make_ds_log_usr(pte_t *ptep, pte_t pte, pid_t pid)
{
	struct ds_log *dnode, *next, *prev;
	struct m_head_struct *mhead;
	struct page *pte_page;
	unsigned long pte_value = pte_pfn(pte);
	unsigned long pte_flag = pte_flags(pte);
	unsigned long base = MAX_NUM;
	int ret = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == pid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK)); /* get pte page */

			if (pte_page->base & PTE_FLAG_MASK) 
				base = make_ds_va((pte_page->base >> 27) & PT_PGTABLE_MASK, (pte_page->base >> 18) & PT_PGTABLE_MASK, (pte_page->base >> 9) & PT_PGTABLE_MASK, ((((unsigned long)ptep) & OFFSET_MASK) / 0x8) & PT_PGTABLE_MASK);
			
			if (base == MAX_NUM) {
				ret = -1;
				goto end;
			}

			if (pte_page->dup_pt)
				dup_pte_update(base, pte_page->dup_pt, pte);

			if ((dnode = make_ds_node(base, base+1, make_ds_offset(base, pte_value), pte_flag)) == NULL) {
				ret = -1;
				goto end;
			}

			if (list_empty(&pte_page->ds_head)) {
				list_add(&dnode->list, &pte_page->ds_head);
				goto end;
			}
			else {
				list_for_each_entry_reverse(prev, &pte_page->ds_head, list) {
					if (prev->base <= dnode->base && dnode->limit <= prev->limit) {
						// printk(KERN_INFO "make ds hit ds %lx %lx %lx %d\n", base, pte_pfn(*ptep), pte_flags(*ptep), pid);
						if (pte_value == 0 && pte_flag == 0) { /* delete ds due to clear pte */
							delete_ds(prev, dnode);
							// printk(KERN_INFO "delete ds %lx-%lx\n", dnode->base, dnode->limit);
						}
						else if (dnode->offset != prev->offset) { // modify pte value
							modify_ds_offset(prev, dnode, pte_page);
							printk(KERN_INFO "modify ds offset %lx %lx %lx %d\n", base, pte_value, pte_flag, pid);
						}
						else if(dnode->flag != prev->flag) { // modify pte flag 
							if(!is_ds_write(prev) && is_ds_write(dnode)) { // ds_mkwrite
								// printk(KERN_INFO "make write %lx %lx-%lx", base, prev->base, prev->limit);
								modify_ds_flag(prev, dnode, pte_page);
								printk(KERN_INFO "make write %lx %lx %d\n", base, pte_flag, pid);
							}
							else if(is_ds_write(prev) && !is_ds_write(dnode)) {	// ds_wrprotect
								// printk(KERN_INFO "make wrprotect %lx %lx-%lx", base, prev->base, prev->limit);
								modify_ds_flag(prev, dnode, pte_page);
								printk(KERN_INFO "make wrprotect %lx %lx %d\n", base, pte_flag, pid);
							}
							else {
								printk(KERN_INFO "not modify ds flag %lx  %lx->%lx %d\n", base, prev->flag, pte_flag, pid);
							}
						}
						goto end;
					}
					else if(dnode->base >= prev->limit) { /* create new pte */
						list_add(&dnode->list, &prev->list);
						// printk(KERN_INFO "make ds %lx %lx %lx %d\n", base, pte_value, pte_flag, pid);
						if(list_is_last(&dnode->list, &pte_page->ds_head)) {
							ds_node_merge(prev, dnode);
							goto end;
						}
						next = list_next_entry(dnode, list);
						ds_node_merge(dnode, next);
						ds_node_merge(prev, dnode);
						goto end;
					}
				}
				list_add(&dnode->list, &pte_page->ds_head); /* create new pte that is top of pt */
				// printk(KERN_INFO "make ds %lx %lx %lx %d\n", base, pte_value, pte_flag, pid);
				next = list_next_entry(dnode, list);
				ds_node_merge(dnode, next);
				goto end;
			}
			break;
		}
	}
	
end:
	return ret;
}

int make_ds_log_usr(pte_t *ptep, pte_t pte)
{
	return __make_ds_log_usr(ptep, pte, current->tgid);
}
EXPORT_SYMBOL_GPL(make_ds_log_usr);

static void clear_wrbit_ds_flag(struct ds_log *ds_node, unsigned long base, unsigned long limit, struct page *page)
{
	struct ds_log *next, *prev;

	if(ds_node->base == base && ds_node->limit == limit) {
		ds_node->flag &= FLAG_RW_NOT;
		printk(KERN_INFO "make wrprotect %lx %lx %d\n", base, ds_node->flag, current->tgid);
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
		if((next = make_ds_node(base, limit, ds_node->offset, ds_node->flag & FLAG_RW_NOT)) == NULL)
			return;
		printk(KERN_INFO "make wrprotect %lx %lx %d\n", base, next->flag, current->tgid);
		list_add_tail(&next->list, &ds_node->list);
		if(!list_is_first(&next->list, &page->ds_head)) {
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
		if(!list_is_last(&prev->list, &page->ds_head)) {
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

static void dup_pte_clear_bit(unsigned long base, pte_t *ptep)
{
	pte_t *target_pte = ptep + (base & PT_PGTABLE_MASK);
	clear_bit(_PAGE_BIT_RW, (unsigned long *)&target_pte->pte);
}

int clear_wrbit_ds_log(pte_t *ptep)
{
	struct page *pte_page;
	struct ds_log *prev;
	struct m_head_struct *mhead;
	unsigned long base = MAX_NUM;
	unsigned long limit = MAX_NUM;
	int ret = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));

			if (pte_page->base & PTE_FLAG_MASK) {
				base = make_ds_va((pte_page->base >> 27) & PT_PGTABLE_MASK, (pte_page->base >> 18) & PT_PGTABLE_MASK, (pte_page->base >> 9) & PT_PGTABLE_MASK, ((((unsigned long)ptep) & OFFSET_MASK) / 0x8) & PT_PGTABLE_MASK);
				limit = base + 1;
			}
					
			if (base == MAX_NUM) {
				ret = -1;
				goto end;
			}

			if (pte_page->dup_pt)
				dup_pte_clear_bit(base, pte_page->dup_pt);

			list_for_each_entry_reverse(prev, &pte_page->ds_head, list) {
				if (prev->base <= base && limit <= prev->limit) {
					if (is_ds_write(prev)) {
						clear_wrbit_ds_flag(prev, base, limit, pte_page);
					}
					break;
				}
			}
			printk(KERN_INFO "  pte pfn %lx %lx\n", pte_pfn(*ptep), pte_flags(*ptep));
			break;
		}
	}

end:
	return ret;
}
EXPORT_SYMBOL_GPL(clear_wrbit_ds_log);


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


int register_broken_pte_and_recover_broken_pgtable(unsigned long pte_va)
{
	struct page *pte_page;
	struct m_head_struct *mhead;
	struct broken_pte_log *bnode;
	unsigned long target_base;
	pte_t *ptep_new;
	int ret = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(pte_va & PAGE_MASK));
			if (pte_page->base & PTE_FLAG_MASK) {
				target_base = make_ds_va((pte_page->base >> 27) & PT_PGTABLE_MASK, (pte_page->base >> 18) & PT_PGTABLE_MASK, (pte_page->base >> 9) & PT_PGTABLE_MASK, ((pte_va & OFFSET_MASK) / 0x8) & PT_PGTABLE_MASK);

				list_for_each_entry(bnode, &mhead->head, list) {
					if (target_base == bnode->base) {
						printk(KERN_INFO "Have already registered the broken pte %lx\n", target_base);
						goto end;
					}
				}
			}

			if (add_broken_pte_node(target_base, mhead) < 0) {
				ret = -1;
				goto end;
			}
			printk(KERN_INFO "register broken pte %lx\n", target_base);
			
			if (!pte_page->dup_pt) {
				ptep_new = pte_realloc(mhead->mm);
				if (!ptep_new) {
					printk(KERN_INFO "out of memory\n");
					ret = -1;
					goto end;
				}
				printk(KERN_INFO "duplicate pte %lx", (unsigned long)__pa(ptep_new));
				update_dup_pgtable(target_base & PT_PGTABLE_MASK_NOT, ptep_new, pte_page);
				pte_page->dup_pt = ptep_new;

				// check ref count 
				if (recover_broken_pgtable(mhead->mm, mhead, pte_page) < 0)
					ret = -1;
				goto end;
			}
			break;
		}
	}
end:
	return ret;
}
EXPORT_SYMBOL_GPL(register_broken_pte_and_recover_broken_pgtable);

int register_broken_pte_and_make_recovery_thread(unsigned long pte_va)
{
	struct page *pte_page;
	struct m_head_struct *mhead;
	struct broken_pte_log *bnode;
	unsigned long target_base;
	pte_t *ptep_new;
	int ret = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(pte_va & PAGE_MASK));
			if (pte_page->base & PTE_FLAG_MASK) {
				target_base = make_ds_va((pte_page->base >> 27) & PT_PGTABLE_MASK, (pte_page->base >> 18) & PT_PGTABLE_MASK, (pte_page->base >> 9) & PT_PGTABLE_MASK, ((pte_va & OFFSET_MASK) / 0x8) & PT_PGTABLE_MASK);

				list_for_each_entry(bnode, &mhead->head, list) {
					if (target_base == bnode->base) {
						printk(KERN_INFO "Have already registered the broken pte %lx\n", target_base);
						goto end;
					}
				}
			}

			if (add_broken_pte_node(target_base, mhead) < 0) {
				ret = -1;
				goto end;
			}
			printk(KERN_INFO "register broken pte %lx\n", target_base);
			
			if (!pte_page->dup_pt) {
				ptep_new = pte_realloc(mhead->mm);
				if (!ptep_new) {
					printk(KERN_INFO "out of memory\n");
					ret = -1;
					goto end;
				}
				printk(KERN_INFO "duplicate pte %lx", (unsigned long)__pa(ptep_new));
				update_dup_pgtable(target_base & PT_PGTABLE_MASK_NOT, ptep_new, pte_page);
				pte_page->dup_pt = ptep_new;

				// check ref count 
				if (make_recovery_thread(pte_page) < 0)
					ret = -1;
				goto end;
			}
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
	struct broken_pte_log *bnode;
	unsigned long base = user_va >> OFFSET_SHIFT;
	struct page *pte_page;
	pte_t *ptep_new; 
	int ret = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			/* get struct page */
			if ((pte_page = get_page_from_addr(mhead->mm, base)) == NULL) {
				ret = -1;
				goto end;
			}

			if (pte_page->base & PTE_FLAG_MASK) {
				list_for_each_entry(bnode, &mhead->head, list) {
					if (base == bnode->base) {
						printk(KERN_INFO "Have already registered the broken pte %lx\n", base);
						goto end;
					}
				}
			}

			if (add_broken_pte_node(base, mhead) < 0) {
				ret = -1;
				goto end;
			}
			printk(KERN_INFO "register broken pte %lx\n", base);
			
			if (!pte_page->dup_pt) {
				ptep_new = pte_realloc(mhead->mm);
				if (!ptep_new) {
					printk(KERN_INFO "out of memory\n");
					ret = -1;
					goto end;
				}
				printk(KERN_INFO "duplicate pte %lx", (unsigned long)__pa(ptep_new));
				update_dup_pgtable(base & PT_PGTABLE_MASK_NOT, ptep_new, pte_page);
				pte_page->dup_pt = ptep_new;
				
				// check ref count 
				if(recover_broken_pgtable(mhead->mm, mhead, pte_page) < 0) 
					ret = -1;
				goto end;
			}
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
	
	if (!ptep) { // NULL pointer
		return -1;
	}

	entry = *ptep; // check EMEs and register broken pte

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			// register broken pte
			// if(recover_count == 100) {
			// 	register_broken_pte_and_make_recovery_thread((unsigned long)ptep);
			// 	recover_count++;
			// } else {
			// 	recover_count++;
			// }
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			target_base = make_ds_va((pte_page->base >> 27) & PT_PGTABLE_MASK, (pte_page->base >> 18) & PT_PGTABLE_MASK, (pte_page->base >> 9) & PT_PGTABLE_MASK, ((((unsigned long)ptep) & OFFSET_MASK) / 0x8) & PT_PGTABLE_MASK);
		
			if (pte_page->dup_pt) {
				list_for_each_entry(bnode, &mhead->head, list) {
					if (target_base == bnode->base) {
						printk(KERN_INFO "Hit broken pte write %lx %d\n", target_base, current->tgid);
						return 1; // pte is broken
					}
				}
			}
			return 0; // pte is safety
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

	if(!ptep) { // NULL pointer
		return native_make_pte(0);
	}

	entry = *ptep; // check EMEs and register broken pte

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			// register broken pte
			if(recover_count == 200) {
				register_broken_pte_and_make_recovery_thread((unsigned long)ptep);
				recover_count++;
			} else {
				recover_count++;
			}

			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			target_base = make_ds_va((pte_page->base >> 27) & PT_PGTABLE_MASK, (pte_page->base >> 18) & PT_PGTABLE_MASK, (pte_page->base >> 9) & PT_PGTABLE_MASK, ((((unsigned long)ptep) & OFFSET_MASK) / 0x8) & PT_PGTABLE_MASK);

			if (pte_page->dup_pt) {
				list_for_each_entry(bnode, &mhead->head, list) {
					if (target_base == bnode->base) {
						printk(KERN_INFO "Hit broken pte read %lx %d\n", target_base, current->tgid);
						offset = target_base & PT_PGTABLE_MASK;
						pte = (pte_t *)pte_page->dup_pt + offset;
						entry = *pte;
						printk(KERN_INFO "get broken    pte %lx %lx\n", pte_pfn(*ptep), pte_flags(*ptep));
						printk(KERN_INFO "get recovered pte %lx %lx\n", pte_pfn(*pte), pte_flags(*pte));
						goto end; // pte is broken
					}
				}
			}
			break;
		}
	}
end:
	return entry; // pte is safety or not managed by ds_log
}
EXPORT_SYMBOL_GPL(check_pte_is_broken_for_pte_read);


static void print_dup_pt(pte_t *ptep, unsigned long base)
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

static int update_pmdp(struct mm_struct *mm, struct m_head_struct *mhead, struct page *pte_page, pmd_t *pmdp)
{
	struct broken_pte_log *bnode;

	if(list_empty(&mhead->head)) {
		printk(KERN_INFO "DO NOT have broken pte\n");
		return -1;
	}
	
	printk(KERN_INFO "recover broken pte %lx\n",pte_page->base);

	list_for_each_entry(bnode, &mhead->head, list) {
		printk(KERN_INFO "        broken pte %lx\n", bnode->base);
	}

	printk(KERN_INFO "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));

	print_dup_pt(pte_page->dup_pt, pte_page->base);
	pmd_reinstall(mm, pmdp, pte_page->dup_pt);
	restore_page(pte_page, virt_to_page(pte_page->dup_pt));

	printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
	return 0;
}

static int recover_broken_pgtable(struct mm_struct *mm, struct m_head_struct *mhead, struct page *pte_page)
{
	pmd_t *pmdp;
	spinlock_t *ptl;
	unsigned long base = 0;
	unsigned long addr = 0;

	base = pte_page->base & PT_PGTABLE_MASK_NOT;
	
	printk(KERN_INFO "start recovery %lx\n",base);

	if(get_pmdp_for_recover_pgtable(mm, base, &pmdp) < 0)
		return -1;

	ptl = ptlock_ptr(pmd_to_page(pmdp));
	spin_lock(ptl);

	preempt_enable();
	for(int i=0; i < 100000; i++) {
		// ref_count_lock(pte_page);
		printk(KERN_INFO "pgtable ref count %d\n", page_count(pte_page));
		if (page_count(pte_page) == 1) {
			// printk(KERN_INFO "pgtable ref count %d\n", page_count(pte_page));
			if (update_pmdp(mm, mhead, pte_page, pmdp) < 0) {
				// ref_count_unlock(pte_page);
				preempt_disable();
				spin_unlock(ptl);
				goto err;
			}
			delete_broken_pte_log(mhead, base, base | PT_PGTABLE_MASK);
			// ref_count_unlock(pte_page);
			break;
		}
		// ref_count_unlock(pte_page);
		// wait some time msec
		fsleep(10);
	}
	preempt_disable();
	spin_unlock(ptl);

	// want to add TLB flush operation
	addr = base << OFFSET_SHIFT;
	flush_tlb_mm_range(mm, addr, addr + PMD_SIZE, OFFSET_SHIFT, false);

	printk(KERN_INFO "finish recovery\n");

	return 0;
err:
	printk(KERN_INFO "cannot recovery\n");
	return -1;
}

// static int get_pmdp_for_kthread(struct mm_struct *mm, unsigned long base, pmd_t **pmdpp)
// {
// 	p4d_t *p4dp;
// 	pud_t *pudp;
// 	pmd_t *pmdp;

// 	unsigned long pgd = (base >> 27) & PT_PGTABLE_MASK;
// 	unsigned long pud = (base >> 18) & PT_PGTABLE_MASK;
// 	unsigned long pmd = (base >> 9) & PT_PGTABLE_MASK;

//   	if(get_pgdp(mm, pgd, &p4dp) == 0) {
// 		// printk(KERN_INFO "success to get p4dp %lx\n", (unsigned long)p4d_val(*p4dp));
// 		if(get_pudp(p4dp, pud, &pudp) == 0) {
// 			// printk(KERN_INFO "success to get pudp %lx\n", (unsigned long)pud_val(*pudp));
// 			if(get_pmdp(pudp, pmd, &pmdp) == 0) {
// 				*(pmdpp) = pmdp;
// 				// printk(KERN_INFO "success to get pmdp %lx\n", (unsigned long)pmd_val(*pmdp));
// 				return 0;
// 			}
// 		}
// 	}
// 	return -1;
// }

// static int update_pmdp_for_kthread(struct mm_struct *mm, struct m_list *mnode, pmd_t *pmdp)
// {
// 	struct broken_pte_list *bnode;

// 	broken_list_read_lock(mnode);
// 	if(list_empty(&mnode->user_head)) {
// 		printk(KERN_INFO "DO NOT have broken pte\n");
// 		broken_list_read_unlock(mnode);
// 		return -1;
// 	}
	
// 	printk(KERN_INFO "recover broken pte %lx\n",mnode->base);

// 	list_for_each_entry(bnode, &mnode->user_head, list) {
// 		printk(KERN_INFO "        broken pte %d\n", bnode->offset);
// 	}
// 	broken_list_read_unlock(mnode);

// 	printk(KERN_INFO "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));

// 	member_write_lock(mnode);
// 	print_dup_pt(mnode->dup_pt, mnode->base);
// 	pmd_reinstall(mm, pmdp, mnode->dup_pt);
// 	modify_m_va(mnode, (unsigned long)mnode->dup_pt);
// 	mnode->dup_pt = NULL;
// 	member_write_unlock(mnode);
// 	delete_broken_pte_all(mnode);

// 	printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
// 	return 0;
// }

static int recovery_thread(void *data)
{
	struct page *pte_page = (struct page *)data;
	struct m_head_struct *mhead;
	struct mm_struct *mm;
	pmd_t *pmdp;
	spinlock_t *ptl;
	unsigned long base = 0;
	unsigned long addr = 0;
	
	base = pte_page->base & PT_PGTABLE_MASK_NOT;

	printk(KERN_INFO "start recovery thread %lx\n", base);

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current_tgid) {
			mm = mhead->mm;
			break;
		}
	}

	if (!mm) {
		printk(KERN_INFO "m_head_struct->mm is NULL\n");
		return -1;
	}

	if (get_pmdp_for_recover_pgtable(mm, base, &pmdp) < 0)
		return -1;
	
	ptl = ptlock_ptr(pmd_to_page(pmdp));
	spin_lock(ptl);

	// preempt_enable();
	// for(int i=0; i < 100000; i++) {
	// 	// ref_count_lock(pte_page);
	// 	printk(KERN_INFO "pgtable ref count %d\n", page_count(pte_page));
	// 	if (page_count(pte_page) == 1) {
	// 		// printk(KERN_INFO "pgtable ref count %d\n", page_count(pte_page));
	// 		if(update_pmdp(mm, mhead, pte_page, pmdp) < 0) {
	// 			// ref_count_unlock(pte_page);
	// 			preempt_disable();
	// 			spin_unlock(ptl);
	// 			goto err;			
	// 		}
	// 		delete_broken_pte_log(mhead, base, base | PT_PGTABLE_MASK);
	// 		// ref_count_unlock(pte_page);
	// 		break;
	// 	}
	// 	// ref_count_unlock(pte_page);
	// 	// wait some time msec
	// 	fsleep(10);
	// }
	// preempt_disable();

	preempt_enable();
	while (!kthread_should_stop())
		schedule();

	printk(KERN_INFO "pgtable ref count %d\n", page_count(pte_page));
	if(update_pmdp(mm, mhead, pte_page, pmdp) < 0) {
		preempt_disable();
		spin_unlock(ptl);
		goto err;			
	}
	delete_broken_pte_log(mhead, base, base | PT_PGTABLE_MASK);
	preempt_disable();
	spin_unlock(ptl);

	// want to add TLB flush operation
	addr = base << OFFSET_SHIFT;
	flush_tlb_mm_range(mm, addr, addr + PMD_SIZE, OFFSET_SHIFT, false);

	printk(KERN_INFO "finish kthread\n");

	return 0;
err:
	printk(KERN_INFO "cannot recovery\n");
	return -1;
}

static int make_recovery_thread(struct page *pte_page)
{
	current_tgid = current->tgid;
	printk(KERN_INFO "current pid %d, tgid %d\n", current->pid, current->tgid);
	preempt_enable();
	k_thread = kthread_run(recovery_thread, pte_page, "kcheckd");
	preempt_disable();
	if(IS_ERR(k_thread)) {
		printk(KERN_INFO "kthread_run error\n");
		return -1;
	}
	printk(KERN_INFO "recovery thread pid %d comm %s\n", k_thread->pid, k_thread->comm);
	return 0;
}

int wait_to_recover_broken_pgtable(pmd_t *pmdp) 
{
	struct page *pte_page;
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			pte_page = pmd_page(*pmdp);
			if (pte_page->dup_pt == NULL)
				goto end;

			printk(KERN_INFO "wait to recover broken pgtable %lx\n", pte_page->base);
			preempt_enable();
			for (int i=0; i < 100000; i++) {
				if (pte_page->dup_pt == NULL) {
					printk(KERN_INFO "DONE replace dup_pt\n");
					preempt_disable();
					goto end;
				}
				// wait some time msec
				fsleep(100);
			}
			preempt_disable();
			break;
		}
	}
end:
	return 0;		
}
EXPORT_SYMBOL_GPL(wait_to_recover_broken_pgtable);

int inc_page_ref_count(pte_t *ptep)
{
	struct page *pte_page;
	struct m_head_struct *mhead;
	int ref_count = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (pte_page->base & PTE_FLAG_MASK) {
				while (pte_page->dup_pt)
					schedule();

				// ref_count_lock(pte_page);
				ref_count = page_ref_inc_return(pte_page);
				// printk(KERN_INFO "m %lx ref count %d %d\n", pte_page->base, ref_count, current->tgid);
				// ref_count_unlock(pte_page);
			}
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(inc_page_ref_count);

int dec_page_ref_count(pte_t *ptep)
{
	struct page *pte_page; 
	struct m_head_struct *mhead;
	int ref_count = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (pte_page->base & PTE_FLAG_MASK) {
				// ref_count_lock(pte_page);
				ref_count = page_ref_dec_return(pte_page);
				// printk(KERN_INFO "m %lx ref count %d %d\n", pte_page->base, ref_count, current->tgid);
				// ref_count_unlock(pte_page);
				if (pte_page->dup_pt && k_thread) {
					kthread_stop(k_thread);
					k_thread = NULL;
				}
			}
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(dec_page_ref_count);

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

	__make_pte_m_list(pmdp, (pte_t *)pmd_page_vaddr(*pmdp), pid);
	
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

	__make_pmd_m_list(pudp, (pmd_t *)pud_pgtable(*pudp), pid);
	
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
	
	__make_pud_m_list(p4dp, (pud_t *)p4d_pgtable(*p4dp), pid);
	
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

static long make_ds_log_usr_from_pgtable(struct task_struct *p)
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
	if(__make_pgd_m_list(mm->pgd, pid) < 0) {
		printk(KERN_INFO "Have already made m list\n");
		return 0;
	}

	for(unsigned long pgd=0; pgd < USER_MAX; pgd++) {
		if(get_pgdp_and_make_m_list(mm, pid, pgd, &p4dp) == 0) {
			for(unsigned long pud=0; pud<MAX; pud++) {
				if(get_pudp_and_make_m_list(p4dp, pid, pud, &pudp) == 0) {
					for(unsigned long pmd=0; pmd<MAX; pmd++) {
						if(get_pmdp_and_make_m_list(pudp, pid, pmd, &pmdp) == 0) {
							for(unsigned long pte=0; pte<MAX; pte++) {
			                	if(get_ptep_and_make_m_list(pmdp, pid, pte, &ptep) == 0) {
									// make_ds from ptep
									if(__make_ds_log_usr(ptep, *ptep, pid) < 0) {
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
	ret = make_ds_log_usr_from_pgtable(current);
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

// static void make_count_node(pid_t pid)
// {
// 	struct count_struct *list = kmalloc(sizeof(struct count_struct), GFP_KERNEL);
// 	if(!list)
// 		return NULL;

// 	list->pid = pid;
// 	list->m_num = 0;
// 	list->pte_num = 0;
// 	list->ds_num = 0;
// 	return list;
// }

static long register_pid(struct mm_struct *mm, pid_t pid)
{
	struct m_head_struct *mhead;
	int ret = 0;

	// target_task = current;
	
	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == pid) {
			printk(KERN_INFO "Have already registered the pid %d\n", pid);
			goto end;
		}
	}

	mhead = kmalloc(sizeof(struct m_head_struct), GFP_KERNEL);
	if (!mhead) {
		ret = -1;
		goto end;
	}
	mhead->pid = pid;
	mhead->mm = mm;
	INIT_LIST_HEAD(&mhead->head);
	list_add(&mhead->list, &user_head);

	printk(KERN_INFO "init pid %d\n",pid);
end:
	return ret;
}

SYSCALL_DEFINE0(mycall_ds_register_pid)
{
	long ret = register_pid(current->mm, current->tgid); 
	recover_count = 0;
	return ret;
	// return register_pid(current->mm, current->tgid);
}

bool check_parent_is_target(pid_t ppid, pid_t pid)
{
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == ppid) {
			printk(KERN_INFO "parent pid %d, child pid %d\n", ppid, pid);
			return true;
		}
	}
	return false;	
}
EXPORT_SYMBOL_GPL(check_parent_is_target);

void register_child(struct task_struct *p)
{
	// register pid & make ds_log, m_list
	printk(KERN_INFO "child pid %d, current tid %d, current pid %d\n", p->tgid, current->pid, current->tgid);
	register_pid(p->mm, p->tgid);
	// print_user_pgtable(p);
	make_ds_log_usr_from_pgtable(p);
}
EXPORT_SYMBOL_GPL(register_child);
