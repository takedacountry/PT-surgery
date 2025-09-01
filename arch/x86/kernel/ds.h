#include <linux/types.h>
#include <linux/mm.h>
#include <linux/rwlock.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/slab.h> 
#include <linux/printk.h>
#include <linux/preempt.h> 
#include <asm/pgtable.h>
#include <asm/pgtable_64_types.h>
#include <asm/pgalloc.h>
#include <asm/paravirt.h>
#include <asm-generic/pgalloc.h>
#include <asm-generic/barrier.h>
#include "ds_struct.h"

#define USER_MAX 			(0x100)
#define MAX 				(0x200)
#define USER_MAX_ADDRESS_SHIFT	(47)
#define USER_MAX_ADDRESS  	(_AT(long, 1) << USER_MAX_ADDRESS_SHIFT)
#define MAX_NUM_SHIFT		(36)
#define MAX_NUM		  		(_AT(long, 1) << MAX_NUM_SHIFT)
#define PT_PGTABLE_SHIFT 	(9)
#define PT_PGTABLE_SIZE		(_AT(long, 1) << PT_PGTABLE_SHIFT)
#define PT_PGTABLE_MASK		(PT_PGTABLE_SIZE - 1)
#define PT_PGTABLE_MASK_NOT	(~PT_PGTABLE_MASK)
#define PGD_FLAG_SHIFT		(0)
#define PGD_FLAG_MASK		(_AT(long, 1) << PGD_FLAG_SHIFT)
#define P4D_FLAG_SHIFT		(1)
#define P4D_FLAG_MASK		(_AT(long, 1) << P4D_FLAG_SHIFT)
#define PUD_FLAG_SHIFT		(2)
#define PUD_FLAG_MASK		(_AT(long, 1) << PUD_FLAG_SHIFT)
#define PMD_FLAG_SHIFT		(3)
#define PMD_FLAG_MASK		(_AT(long, 1) << PMD_FLAG_SHIFT)
#define PTE_FLAG_SHIFT		(4)
#define PTE_FLAG_MASK		(_AT(long, 1) << PTE_FLAG_SHIFT)
#define OFFSET_SHIFT 		(12)
#define OFFSET_SIZE			(_AT(long, 1) << OFFSET_SHIFT)
#define OFFSET_MASK			(OFFSET_SIZE - 1)
#define OFFSET_MASK_NOT		(~OFFSET_MASK)
#define RW_BIT				(1)
#define FLAG_RW				(_AT(long, 1) << RW_BIT)
#define FLAG_RW_NOT			(~FLAG_RW)

#define PT_IS_VALID 		(0)
#define PT_IS_DAMAGED		(1)
#define PT_IS_RECOVERYING	(2)
// #define CONFIG_RECOVERY_COUNT

// for memcached
// #define DIVISION_NUM (43750)

// for redis
// #define DIVISION_NUM (40000)

// for apache
#define DIVISION_NUM (50000)

extern struct list_head user_head;
// extern struct list_head kern_head;
extern struct list_head count_head;
extern rwlock_t user_head_lock;

static inline pte_t *pte_offset_index(pmd_t *pmd, unsigned long index)
{
	return (pte_t *)pmd_page_vaddr(*pmd) + index;
}

static inline pmd_t *pmd_offset_index(pud_t *pud, unsigned long index)
{
	return pud_pgtable(*pud) + index;
}

static inline pud_t *pud_offset_index(p4d_t *p4d, unsigned long index)
{
	return p4d_pgtable(*p4d) + index;
}

static inline p4d_t *p4d_offset_index(pgd_t *pgd, unsigned long index)
{
	if(!pgtable_l5_enabled())
    		return (p4d_t *)pgd;
	printk(KERN_INFO "pagetable level 5");
  	return (p4d_t *)pgd_page_vaddr(*pgd) + index;
}

static inline pgd_t *pgd_offset_index(struct mm_struct *mm, unsigned long index)
{
  	return mm->pgd + index;
}

static inline unsigned long make_ds_base(unsigned long pgd_offset, unsigned long pud_offset, unsigned long pmd_offset, unsigned long pte_offset)
{
	unsigned long base = pgd_offset << 27 | pud_offset << 18 | pmd_offset << 9 | pte_offset;
	return base;	
}

static inline unsigned long make_ds_base_from_p4d(unsigned long p4d)
{
	return make_ds_base(((p4d & OFFSET_MASK) / 0x8) & PT_PGTABLE_MASK, 0, 0, PUD_FLAG_MASK & PT_PGTABLE_MASK);
}

static inline unsigned long make_ds_base_from_pud(unsigned long pud, unsigned long base)
{
	return make_ds_base((base >> 27) & PT_PGTABLE_MASK, ((pud & OFFSET_MASK) / 0x8) & PT_PGTABLE_MASK, 0, PMD_FLAG_MASK & PT_PGTABLE_MASK);
}

static inline unsigned long make_ds_base_from_pmd(unsigned long pmd, unsigned long base)
{
	return make_ds_base((base >> 27) & PT_PGTABLE_MASK, (base >> 18) & PT_PGTABLE_MASK,  ((pmd & OFFSET_MASK) / 0x8)  & PT_PGTABLE_MASK, PTE_FLAG_MASK & PT_PGTABLE_MASK);
}

static inline unsigned long make_ds_base_from_pte(unsigned long pte, unsigned long base)
{
	return make_ds_base((base >> 27) & PT_PGTABLE_MASK, (base >> 18) & PT_PGTABLE_MASK, (base >> 9) & PT_PGTABLE_MASK, ((pte & OFFSET_MASK) / 0x8) & PT_PGTABLE_MASK);
}

static inline long make_ds_offset(long base, unsigned long pte_value)
{
	long offset = base - pte_value;
	return offset;
}

static inline struct ds_log *make_ds_log_node(unsigned long base, unsigned long limit, long offset, unsigned long flag)
{
	struct ds_log *itr;
	itr = kmalloc(sizeof(struct ds_log), GFP_KERNEL);
	if(!itr)
		return NULL;

	itr->base = base;
	itr->limit = limit;
	itr->offset = offset;
	itr->flag = flag;
	return itr;
}

static inline struct ds_info *make_ds_info_node(unsigned long base) 
{
	struct ds_info *itr;
	itr = kmalloc(sizeof(struct ds_info), GFP_KERNEL);
	if(!itr)
		return NULL;

	itr->base = base;
	itr->dup_pt = NULL;
	INIT_LIST_HEAD(&itr->ds_head);
	itr->recovery_state = 0;
	spin_lock_init(&itr->recovery_lock);
	return itr;
}

static inline struct broken_pte_log *make_broken_pte_node(unsigned long base) 
{
	struct broken_pte_log *itr;
	itr = kmalloc(sizeof(struct broken_pte_log), GFP_KERNEL);
	if(!itr)
		return NULL;

	itr->base = base;
	return itr;
}

static inline struct m_head_struct *make_m_head_struct_node(struct mm_struct *mm, pid_t pid)
{
	struct m_head_struct *itr;
	itr = kmalloc(sizeof(struct m_head_struct), GFP_KERNEL);
	if (!itr) 
		return NULL;

	mhead->pid = pid;
	mhead->mm = mm;
	mhead->krecoverd_task = NULL;
	spin_lock_init(&mhead->krecoverd_lock);
	INIT_LIST_HEAD(&mhead->head);
	list_add(&mhead->list, &user_head);
	return itr;
}

static inline struct recovery_count *make_recovery_count_node(pid_t pid)
{
	struct recovery_count *itr;
	
	itr = kmalloc(sizeof(struct recovery_count), GFP_KERNEL);
	if (!itr)
		return NULL;
	
	itr->pid = pid;
	itr->kcount = 0;
	itr->ksuccount = 0;
	itr->ucount = 0;
	itr->usuccount = 0;
	list_add(&itr->list, &count_head);
	return itr;
}

static inline bool is_ds_node_merge(struct ds_log *prev, struct ds_log *next)
{
	if(prev->limit == next->base && prev->offset == next->offset && prev->flag == next->flag) {
		return true;
	}
	else {
		return false;
	}
}

static inline void ds_node_merge(struct ds_log *prev, struct ds_log *next)
{
	if(is_ds_node_merge(prev, next)) {
		prev->limit = next->limit;
		list_del(&next->list);
		kfree(next);
	}
}

static inline int add_broken_pte_node(unsigned long base, struct m_head_struct *mnode)
{
	struct broken_pte_log *itr;

	itr = make_broken_pte_node(base);
	if (!itr)
		return -1;

	list_add_tail(&itr->list, &mnode->head);
	return 0;
}

static inline void pmd_repopulate(struct mm_struct *mm, pmd_t *pmd, pte_t *pte)
{
	paravirt_alloc_pte(mm, __pa(pte) >> PAGE_SHIFT);
	set_pmd(pmd, __pmd(__pa(pte) | pmd_flags(*pmd)));
}

static inline void pmd_reinstall(struct mm_struct *mm, pmd_t *pmdp, pte_t *ptep)
{
	// spinlock_t *ptl = pmd_lock(mm, pmdp);
	
	if (!pmd_none(*pmdp) && pmd_present(*pmdp)) {
		smp_wmb(); /* See comment in pmd_install() */
		pmd_repopulate(mm, pmdp, ptep);		
		ptep = NULL;
	}
	// spin_unlock(ptl);
	
	if (ptep)
		pte_free(mm, virt_to_page(ptep));
}

static inline void pmd_reinstall_lock(struct mm_struct *mm, pmd_t *pmdp, pte_t *ptep)
{
	spinlock_t *ptl = pmd_lock(mm, pmdp);
	
	if (!pmd_none(*pmdp) && pmd_present(*pmdp)) {
		smp_wmb(); /* See comment in pmd_install() */
		pmd_repopulate(mm, pmdp, ptep);		
		ptep = NULL;
	}
	spin_unlock(ptl);
	
	if (ptep)
		pte_free(mm, virt_to_page(ptep));
}

static inline pte_t *pte_realloc(struct mm_struct *mm)
{
	struct page *new = (struct page *)pte_alloc_one(mm);
	unsigned long pte;
	if(!new)
		return	NULL;
	
	pte = (unsigned long)page_address(new);
	return (pte_t *)pte;
}

// static inline void pmd_reinstall_kernel(pmd_t *pmdp, pte_t *ptep)
// {
// 	spin_lock(&init_mm.page_table_lock);
// 	if (!pmd_none(*pmdp) && pmd_present(*pmdp)) {
// 		smp_wmb(); /* See comment in pmd_install() */
// 		pmd_repopulate(&init_mm, pmdp, ptep);
// 		ptep = NULL;
// 	}
// 	spin_unlock(&init_mm.page_table_lock);
	
// 	if (ptep)
// 		pte_free_kernel(&init_mm, ptep);
// }

// static inline pte_t *pte_realloc_kernel(void)
// {
// 	pte_t *new = pte_alloc_one_kernel(&init_mm);
// 	if (!new)
// 		return NULL;
// 	return new;
// }

static inline int update_dup_pte(pte_t **ptep, struct ds_log *itr, unsigned long start, unsigned long end)
{
	unsigned long count;
	pte_t *pte = *(ptep);

#ifdef CONFIG_RECOVERY_COUNT
	unsigned int emes;
	get_random_bytes(&emes, sizeof(emes));
	if (emes % DIVISION_NUM == 0)
		return -1;
#endif

	for (count=start; count < itr->limit; count++) {
		if(itr->base <= count)
			set_pte_recover(pte, __pte(((pteval_t)(count - itr->offset) << PAGE_SHIFT) | itr->flag));
		pte++;
	}
	
	*(ptep) = pte;

	return 0;
}

static inline int update_dup_pgtable(unsigned long va_start, pte_t *pte, struct page *page)
{
	struct ds_log *itr;
	unsigned long va_end;
	
	va_end = va_start | PT_PGTABLE_MASK;

	list_for_each_entry(itr, &page->ds_head, list) {
		// printk(KERN_INFO "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
		if (update_dup_pte(&pte, itr, va_start, va_end) < 0)
			return -1;
		va_start = itr->limit;
	}
	return 0;
}

static inline void delete_ds_all(struct page *page)
{
	struct ds_log *itr, *tmp;

	list_for_each_entry_safe(itr, tmp, &page->ds_head, list) {
		list_del(&itr->list);
		kfree(itr);
	}
}

static inline void delete_broken_pte_all(struct m_head_struct *mnode)
{
	struct broken_pte_log *itr, *tmp;

	list_for_each_entry_safe(itr, tmp, &mnode->head, list) {
		list_del(&itr->list);
		kfree(itr);
	}
}

static inline void print_ds_log(struct page *pte_page)
{
	struct ds_log *dnode;

	printk(KERN_INFO "page base %lx, page addr %lx\n", pte_page->base, (unsigned long)page_address(pte_page));
	if (list_empty(&pte_page->ds_head)) {
		printk(KERN_INFO "     nothing ds log\n");
	} else {
		list_for_each_entry(dnode, &pte_page->ds_head, list) {
			printk("     %lx %lx %lx %lx\n", dnode->base, dnode->limit, dnode->offset, dnode->flag);
		}
	}
}

static inline void copy_ds_log(struct page *before, struct page *after)
{
	struct ds_log *itr, *tmp;

	list_for_each_entry_safe(itr, tmp, &before->ds_head, list) {
		list_del(&itr->list);
		list_add_tail(&itr->list, &after->ds_head);
	}
	// print_ds_log(before);
	// print_ds_log(after);
}

static inline void restore_page(struct page *before, struct page *after)
{
	if (before != NULL && after != NULL) {
		after->base = before->base;
		after->dup_pt = NULL;
		before->base = 0;
		before->dup_pt = NULL;

		copy_ds_log(before, after);
	}
}

static inline int delete_broken_pte_log(struct m_head_struct *mhead, unsigned long start, unsigned long end)
{
	struct broken_pte_log *itr, *tmp;
	int count = 0;

	list_for_each_entry_safe(itr, tmp, &mhead->head, list) {
		if (start <= itr->base && itr->base <= end) {
			list_del(&itr->list);
			kfree(itr);
			count++;
		}
	}
	return count;
}

static inline int dec_preempt_before_schedule(void)
{
	int count = preempt_count();
	// printk(KERN_INFO "dec preempt %d before %d\n", current->tgid, preempt_count());
	if (count < 10) {
		preempt_count_sub(count);
	} else {
		preempt_count_add(1);
	}
	// printk(KERN_INFO "dec preempt %d after %d\n", current->tgid, preempt_count());
	return count;
}

static inline void inc_preempt_after_schedule(int count)
{
	// printk(KERN_INFO "inc preempt %d before %d\n", current->tgid, preempt_count());
	if (count < 10) {
		preempt_count_add(count);
	} else {
		preempt_count_sub(1);
	}
	// printk(KERN_INFO "inc preempt %d after %d\n", current->tgid, preempt_count());
}