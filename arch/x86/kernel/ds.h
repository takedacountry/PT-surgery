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

#define _PAGE_RW_NOT 		(~_PAGE_RW)
#define _PAGE_ACCESSED_DIRTY (_PAGE_ACCESSED | _PAGE_DIRTY)

// #define CONFIG_RECOVERY_COUNT

// for memcached
// #define DIVISION_NUM (43750)

// for redis
// #define DIVISION_NUM (40000)

// for apache
#define DIVISION_NUM (102050)

// for debug
// #define DIVISION_NUM (5000)

extern struct list_head user_head;
// extern struct list_head kern_head;
extern struct list_head count_head;

// #define DS_LOG_MEMPOOL_NUM (1024)
// struct kmem_cache *ds_log_cache;
// mempool_t *ds_log_mempool;

static inline int delete_broken_pte_all(struct m_log *m_log);

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

static inline int get_ptep(pmd_t *pmdp, unsigned long pte, pte_t **ptepp)
{
  	pte_t *ptep = pte_offset_index(pmdp, pte);
	*(ptepp) = ptep;

  	if(pte_none(*ptep) || !pte_present(*ptep)) {
    	// printk(KERN_INFO "pte %lu is not present.\n", pte);
    	return -1;
  	}
		
  	return 0;
}

static inline int get_pmdp(pud_t *pudp, unsigned long pmd, pmd_t **pmdpp)
{
  	pmd_t *pmdp = pmd_offset_index(pudp, pmd);
	*(pmdpp) = pmdp;

  	if(pmd_none(*pmdp) || !pmd_present(*pmdp) || pmd_large(*pmdp)){
   		// printk(KERN_INFO "pmd %lu is not present.\n", pmd);
   		return -1;
  	}

	return 0;
}

static inline int get_pudp(p4d_t *p4dp, unsigned long pud, pud_t **pudpp)
{
  	pud_t *pudp = pud_offset_index(p4dp, pud);
	*(pudpp) = pudp;
	  
  	if(pud_none(*pudp) || !pud_present(*pudp) || pud_large(*pudp)){
	    // printk(KERN_INFO "pud %lu is not present", pud);
	    return -1;
  	}

  	return 0;  
}

static inline int get_p4dp(pgd_t *pgdp, unsigned long p4d, p4d_t **p4dpp)
{
  	p4d_t *p4dp = p4d_offset_index(pgdp, p4d);
	*(p4dpp) = p4dp;
	
	if(p4d_none(*p4dp) || !p4d_present(*p4dp)){
	    // printk(KERN_INFO "p4d %lu is not present", pgd);
    	return -1;
  	}

  	return 0;
}

static inline int get_pgdp(struct mm_struct *mm, unsigned long pgd, p4d_t **p4dpp)
{
  	pgd_t *pgdp = pgd_offset_index(mm, pgd);

  	if(pgd_none(*pgdp) || !pgd_present(*pgdp)){
	    // printk(KERN_INFO "pgd %lu is not present.\n", pgd);
    	return -1;
  	}

	if(get_p4dp(pgdp, pgd, p4dpp) < 0)
		return -1;
	
	return 0;
}

static inline bool is_available_pgd(struct mm_struct *mm)
{
	p4d_t *p4dp;
	
	for(unsigned long pgd=0; pgd<USER_MAX; pgd++) {
		if(get_pgdp(mm, pgd, &p4dp) == 0) {
			return true;
		}
    }
	return false;
}


static inline unsigned long ds_log_rw_diff(struct ds_log *dnode)
{
	return dnode->flag & _PAGE_RW;
}

static inline unsigned long ds_log_flag_diff(unsigned long flag1, unsigned long flag2)
{
	return (flag1 | _PAGE_ACCESSED_DIRTY) ^ (flag2 | _PAGE_ACCESSED_DIRTY);
}	


static inline int init_page_for_pt_surgery(struct page *page)
{
	page->m_log = kmalloc(sizeof(struct m_log), GFP_ATOMIC);
	if (!page->m_log) {
		printk(KERN_INFO "M_LOG ERROR: kmalloc failed\n");
		return -1;
	}
	INIT_LIST_HEAD(&page->ds_head);
	spin_lock_init(&page->ds_lock);
	return 0;
}

static inline void exit_page_for_pt_surgery(struct page *page)
{
	kfree(page->m_log);
}

static inline void init_m_log(struct m_log *m_log, unsigned long base)
{
	m_log->base = base;
	m_log->replica = NULL;
	INIT_LIST_HEAD(&m_log->head);
	spin_lock_init(&m_log->broken_lock);
	// m_log->recovery_state = PT_IS_HEALTHY;
	spin_lock_init(&m_log->recovery_lock);
}

static inline void exit_m_log(struct m_log *m_log)
{
	m_log->base = 0;
	m_log->replica = NULL;
	// m_log->recovery_state = PT_IS_HEALTHY;
	delete_broken_pte_all(m_log);
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
	itr = kmalloc(sizeof(struct ds_log), GFP_ATOMIC);
	if(!itr) {
		printk(KERN_INFO "DS_LOG ERROR: kmalloc failed\n");
		return NULL;
	}

	itr->base = base;
	itr->limit = limit;
	itr->offset = offset;
	itr->flag = flag;
	return itr;
}

static inline struct broken_pte_log *make_broken_pte_node(unsigned long base) 
{
	struct broken_pte_log *itr;
	itr = kmalloc(sizeof(struct broken_pte_log), GFP_ATOMIC);
	if (!itr) {
		printk(KERN_INFO "BROKEN_LOG ERROR: kmalloc failed\n");
		return NULL;
	}

	itr->base = base;
	return itr;
}

// static inline struct krecoverd_info *make_kinfo_node(struct m_head_struct *mhead, struct page *page)
// {
// 	struct krecoverd_info *itr = kmalloc((sizeof(struct krecoverd_info)), GFP_ATOMIC);
// 	if (!itr) {
// 		printk(KERN_INFO "KRECOVERED_INFO ERROR: kmalloc failed\n");
// 		return NULL;
// 	}
// 	itr->mhead = mhead;
// 	itr->page = page;
// 	itr->krecoverd_task = NULL;
// 	return itr;
// }

// static inline void destroy_kinfo_node(struct krecoverd_info *itr)
// {
// 	itr->mhead = NULL;
// 	itr->page = NULL;
// 	itr->krecoverd_task = NULL;
// 	kfree(itr);
// }

// static inline struct m_head_struct *make_m_head_struct_node(struct mm_struct *mm, pid_t pid)
// {
// 	struct m_head_struct *itr;
// 	itr = kmalloc(sizeof(struct m_head_struct), GFP_KERNEL);
// 	if (!itr) 
// 		return NULL;

// 	mhead->pid = pid;
// 	mhead->mm = mm;
// 	mhead->krecoverd_task = NULL;
// 	spin_lock_init(&mhead->krecoverd_lock);
// 	INIT_LIST_HEAD(&mhead->head);
// 	list_add(&mhead->list, &user_head);
// 	return itr;
// }

static inline struct recovery_count *make_recovery_count_node(pid_t pid)
{
	struct recovery_count *itr;
	
	itr = kmalloc(sizeof(struct recovery_count), GFP_ATOMIC);
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

static inline int add_broken_pte_node(unsigned long base, struct m_log *m_log)
{
	struct broken_pte_log *itr;

	itr = make_broken_pte_node(base);
	if (!itr)
		return -1;

	spin_lock(&m_log->broken_lock);
	list_add_tail(&itr->list, &m_log->head);
	spin_unlock(&m_log->broken_lock);
	return 0;
}

static inline int is_broken_pte_node_registered(unsigned long base, struct m_log *m_log)
{
	struct broken_pte_log *bnode;

	spin_lock(&m_log->broken_lock);
	list_for_each_entry(bnode, &m_log->head, list) {
		if (base == bnode->base) {
			spin_unlock(&m_log->broken_lock);
			printk(KERN_INFO "Already registered the broken pte %lx\n", base);
			return 1;
		}
	}
	spin_unlock(&m_log->broken_lock);
	return 0;
}

static inline void pmd_repopulate(struct mm_struct *mm, pmd_t *pmd, pte_t *pte)
{
	paravirt_alloc_pte(mm, __pa(pte) >> PAGE_SHIFT);
	set_pmd(pmd, __pmd(__pa(pte) | pmd_flags(*pmd)));
}

static inline void pmd_reinstall(struct mm_struct *mm, pmd_t *pmdp, pte_t *ptep)
{
	
	if (!pmd_none(*pmdp) && pmd_present(*pmdp)) {
		smp_wmb(); /* See comment in pmd_install() */
		pmd_repopulate(mm, pmdp, ptep);		
		ptep = NULL;
	}
	
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

static inline int restore_replica_from_ds_log(pte_t **ptep, struct ds_log *itr, unsigned long start)
{
	unsigned long count;
	pte_t *pte = *(ptep);

#ifdef CONFIG_RECOVERY_COUNT
	unsigned int emes;
	get_random_bytes(&emes, sizeof(emes));
	if (emes % DIVISION_NUM == 0) {	
		printk(KERN_INFO "RECOVERY ERROR: EMEs in ds_log\n");
		return -1;
	}
#endif

	for (count=start; count < itr->limit; count++) {
		if(itr->base <= count)
			set_pte_recover(pte, __pte(((pteval_t)(count - itr->offset) << PAGE_SHIFT) | itr->flag));
		pte++;
	}
	
	*(ptep) = pte;

	return 0;
}

static inline int restore_replica(unsigned long va_start, pte_t *pte, struct page *page)
{
	struct ds_log *itr;
	
	spin_lock(&page->ds_lock);
	list_for_each_entry(itr, &page->ds_head, list) {
		// printk(KERN_INFO "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
		if (restore_replica_from_ds_log(&pte, itr, va_start) < 0) {
			spin_unlock(&page->ds_lock);
			return -1;
		}
		va_start = itr->limit;
	}
	spin_unlock(&page->ds_lock);
	return 0;
}

static inline void delete_ds_all(struct page *page)
{
	struct ds_log *itr, *tmp;

	spin_lock(&page->ds_lock);
	list_for_each_entry_safe(itr, tmp, &page->ds_head, list) {
		list_del(&itr->list);
		kfree(itr);
	}
	spin_unlock(&page->ds_lock);
}

static inline int delete_broken_pte_all(struct m_log *m_log)
{
	struct broken_pte_log *itr, *tmp;
	int count=0;

	spin_lock(&m_log->broken_lock);
	list_for_each_entry_safe(itr, tmp, &m_log->head, list) {
		list_del(&itr->list);
		kfree(itr);
		count++;
	}
	spin_unlock(&m_log->broken_lock);
	return count;
}

// static inline void print_ds_log(struct page *pte_page)
// {
// 	struct ds_log *dnode;

// 	printk(KERN_INFO "page base %lx, page addr %lx\n", pte_page->base, (unsigned long)page_address(pte_page));
// 	if (list_empty(&pte_page->ds_head)) {
// 		printk(KERN_INFO "     nothing ds log\n");
// 	} else {
// 		list_for_each_entry(dnode, &pte_page->ds_head, list) {
// 			printk("     %lx %lx %lx %lx\n", dnode->base, dnode->limit, dnode->offset, dnode->flag);
// 		}
// 	}
// }

static inline void copy_ds_log(struct page *before, struct page *after)
{
	struct ds_log *itr, *tmp;

	spin_lock(&before->ds_lock);
	spin_lock(&after->ds_lock);
	list_for_each_entry_safe(itr, tmp, &before->ds_head, list) {
		list_del(&itr->list);
		list_add_tail(&itr->list, &after->ds_head);
	}
	spin_unlock(&after->ds_lock);
	spin_unlock(&before->ds_lock);
	// print_ds_log(before);
	// print_ds_log(after);
}

static inline int restore_page(struct page *before, struct page *after)
{
#ifdef CONFIG_RECOVERY_COUNT
	unsigned int emes;
	get_random_bytes(&emes, sizeof(emes));
	if (emes % DIVISION_NUM == 0) {	
		printk(KERN_INFO "RECOVERY ERROR: EMEs in pmd\n");
		return -1;
	}
#endif

	if(init_page_for_pt_surgery(after) < 0)
		return -1;

	init_m_log(after->m_log, before->m_log->base);
	copy_ds_log(before, after);
	return 0;
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