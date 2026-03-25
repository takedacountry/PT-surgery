#include <linux/mm.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/list.h>
#include <linux/slab.h> 
#include <linux/printk.h>
#include <linux/preempt.h> 
#include <linux/xarray.h>
#include <asm/pgtable.h>
#include <asm/pgtable_64_types.h>
#include <asm/pgalloc.h>
#include <asm/paravirt.h>
#include <asm-generic/pgalloc.h>
#include <asm-generic/barrier.h>

#define PGD_USER_MAX 			(0x100)
#define PGD_KERN_MAX 			(0x200)
#define PT_OFFSET_SHIFT 		(9)
#define PT_OFFSET_SIZE			(_AT(long, 1) << PT_OFFSET_SHIFT)
#define PT_OFFSET_MASK			(PT_OFFSET_SIZE - 1)
#define PT_OFFSET_MASK_NOT		(~PT_OFFSET_MASK)
#define PAGE_OFFSET_SHIFT 		(12)
#define PAGE_OFFSET_SIZE		(_AT(long, 1) << PAGE_OFFSET_SHIFT)
#define PAGE_OFFSET_MASK		(PAGE_OFFSET_SIZE - 1)
#define PAGE_OFFSET_MASK_NOT	(~PAGE_OFFSET_MASK)
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
#define IS_KERNEL_WRITE		(1)
#define IS_KERNEL_READ		(0)

#define _PAGE_RW_NOT 		(~_PAGE_RW)
#define _PAGE_ACCESSED_DIRTY (_PAGE_ACCESSED | _PAGE_DIRTY)


// #define FAULT_RATIO (33750) // for memcached
#define FAULT_RATIO (30000) // for redis
// #define FAULT_RATIO (92050) // for apache
// #define FAULT_RATIO (1000) // for default

#define EMULATE_EMES_FOR_PTE (1)

extern struct list_head user_head;
extern spinlock_t user_head_lock;

struct ds_log {
	unsigned long base;
	unsigned long limit;
	long offset;
	unsigned long flag;
	struct list_head list;
};

enum pt_state {
	PT_PAGE_SAFE = 0,	/* pt page is healthy */
	PT_PAGE_RECOVERING, /* pt page is recovering */
	PT_PAGE_RECOVERED,	/* pt page is recovered */
};

struct m_log {
	unsigned long base;				/* user address */ 
	void *replica;					/* replica page table */
	void *old_pt;					/* old and falty page table */
	atomic_t pt_state;				/* pt state */
};

enum pt_op_type {
    PT_OP_NONE = 0,	/* pt operation is pt walk */
    PT_OP_READ,		/* pt operation is pt read */
    PT_OP_WRITE,	/* pt operation is pt write */
};

struct thread_pt_op{
	enum pt_op_type op;
};

struct m_head_struct{
	pid_t pid;					/* register pid */
	struct mm_struct *mm;		/* target mm_struct */
	struct xarray pt_op;		/* pt operation per thread */
	struct list_head list;
};

int make_pte_ds_log_usr(struct page *pte_page, pte_t *ptep, pte_t pte);
int clear_wrbit_ds_log(struct page *pte_page, pte_t *ptep);

static inline bool pte_in_corrupted_pt(struct page *pte_page, pte_t *ptep)
{
	unsigned long old_pt_addr;
	unsigned long pte_addr;

	if (!pte_page->m_log->old_pt)
		return false;

	old_pt_addr = (unsigned long)pte_page->m_log->old_pt & PAGE_MASK;
	pte_addr = (unsigned long)ptep;
	if (old_pt_addr <= pte_addr && pte_addr < old_pt_addr + PAGE_SIZE)
		return true;
	return false;
}

static inline void redirect_pte_wrprotect(struct page *pte_page, pte_t *ptep)
{
	unsigned long offset = (((unsigned long)ptep & PAGE_OFFSET_MASK) / 0x8) & PT_OFFSET_MASK;
	pte_t *replica_ptep = (pte_t *)pte_page->m_log->replica + offset;
	clear_bit(_PAGE_BIT_RW, (unsigned long *)&replica_ptep->pte);
	clear_wrbit_ds_log(virt_to_page(pte_page->m_log->replica), replica_ptep);
	// printk(KERN_INFO "redirect pte wrprotect %lx -> %lx\n", (unsigned long)pte_val(*ptep), (unsigned long)pte_val(*replica_ptep));
}

static inline void redirect_pte_write(struct page *pte_page, pte_t *ptep, pte_t pte)
{
	unsigned long offset = (((unsigned long)ptep & PAGE_OFFSET_MASK) / 0x8) & PT_OFFSET_MASK;
	pte_t *replica_ptep = (pte_t *)pte_page->m_log->replica + offset;
	WRITE_ONCE(*replica_ptep, pte);
	make_pte_ds_log_usr(virt_to_page(pte_page->m_log->replica), replica_ptep, pte);
	// printk(KERN_INFO "redirect pte write %lx -> %lx\n", (unsigned long)pte_val(*ptep), (unsigned long)pte_val(*replica_ptep));
}

static inline pte_t redirect_pte_read(struct page *pte_page, pte_t *ptep)
{
	unsigned long offset = (((unsigned long)ptep & PAGE_OFFSET_MASK) / 0x8) & PT_OFFSET_MASK;
	pte_t *replica_ptep = (pte_t *)pte_page->m_log->replica + offset;
	// printk(KERN_INFO "redirect pte read %lx -> %lx\n", (unsigned long)pte_val(*ptep), (unsigned long)pte_val(*replica_ptep));
	return *replica_ptep;
}

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
	
	for(unsigned long pgd=0; pgd<PGD_USER_MAX; pgd++) {
		if(get_pgdp(mm, pgd, &p4dp) == 0) {
			return true;
		}
    }
	return false;
}

static inline void delete_ds_log_all(struct page *page)
{
	struct ds_log *itr, *tmp;

	list_for_each_entry_safe(itr, tmp, &page->ds_head, list) {
		list_del(&itr->list);
		kfree(itr);
	}
}

static inline int init_page_for_pt_surgery(struct page *page)
{
	page->m_log = kmalloc(sizeof(struct m_log), GFP_ATOMIC);
	if (!page->m_log) {
		printk(KERN_INFO "M_LOG ERROR: kmalloc failed\n");
		return -1;
	}
	INIT_LIST_HEAD(&page->ds_head);
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
	m_log->old_pt = NULL;
	atomic_set(&m_log->pt_state, PT_PAGE_SAFE);
}

static inline void exit_m_log(struct m_log *m_log)
{
	m_log->base = 0;
	m_log->replica = NULL;
	m_log->old_pt = NULL;
	atomic_set(&m_log->pt_state, PT_PAGE_SAFE);
}

static inline unsigned long make_ds_base(unsigned long pgd_offset, unsigned long pud_offset, unsigned long pmd_offset, unsigned long pte_offset)
{
	unsigned long base = pgd_offset << 27 | pud_offset << 18 | pmd_offset << 9 | pte_offset;
	return base;	
}

static inline unsigned long make_ds_base_from_p4d(unsigned long p4d)
{
	return make_ds_base(((p4d & PAGE_OFFSET_MASK) / 0x8) & PT_OFFSET_MASK, 0, 0, PUD_FLAG_MASK & PT_OFFSET_MASK);
}

static inline unsigned long make_ds_base_from_pud(unsigned long pud, unsigned long base)
{
	return make_ds_base((base >> 27) & PT_OFFSET_MASK, ((pud & PAGE_OFFSET_MASK) / 0x8) & PT_OFFSET_MASK, 0, PMD_FLAG_MASK & PT_OFFSET_MASK);
}

static inline unsigned long make_ds_base_from_pmd(unsigned long pmd, unsigned long base)
{
	return make_ds_base((base >> 27) & PT_OFFSET_MASK, (base >> 18) & PT_OFFSET_MASK,  ((pmd & PAGE_OFFSET_MASK) / 0x8)  & PT_OFFSET_MASK, PTE_FLAG_MASK & PT_OFFSET_MASK);
}

static inline unsigned long make_ds_base_from_pte(unsigned long pte, unsigned long base)
{
	return make_ds_base((base >> 27) & PT_OFFSET_MASK, (base >> 18) & PT_OFFSET_MASK, (base >> 9) & PT_OFFSET_MASK, ((pte & PAGE_OFFSET_MASK) / 0x8) & PT_OFFSET_MASK);
}

static inline long make_ds_offset(long base, unsigned long pte_value)
{
	long offset = base - pte_value;
	return offset;
}

static inline unsigned long ds_log_rw_diff(struct ds_log *dnode)
{
	return dnode->flag & _PAGE_RW;
}

static inline unsigned long ds_log_flag_diff(unsigned long flag1, unsigned long flag2)
{
	return (flag1 | _PAGE_ACCESSED_DIRTY) ^ (flag2 | _PAGE_ACCESSED_DIRTY);
}	

static inline bool is_ds_log_node_merge(struct ds_log *prev, struct ds_log *next)
{
	if(prev->limit == next->base && prev->offset == next->offset && prev->flag == next->flag) {
		return true;
	}
	else {
		return false;
	}
}

static inline void ds_log_node_merge(struct ds_log *prev, struct ds_log *next)
{
	if(is_ds_log_node_merge(prev, next)) {
		prev->limit = next->limit;
		list_del(&next->list);
		kfree(next);
	}
}

static inline void ds_log_node_merge_prev(struct ds_log *dnode)
{
	struct ds_log *prev = list_prev_entry(dnode, list);
	ds_log_node_merge(prev, dnode);
}

static inline void ds_log_node_merge_next(struct ds_log *dnode)
{
	struct ds_log *next = list_next_entry(dnode, list);
	ds_log_node_merge(dnode, next);
}

static inline void ds_log_node_merge_both(struct ds_log *dnode)
{
	struct ds_log *prev = list_prev_entry(dnode, list);
	struct ds_log *next = list_next_entry(dnode, list);
	ds_log_node_merge(dnode, next);
	ds_log_node_merge(prev, dnode);
}

static inline void ds_log_node_split_head(struct ds_log *dnode, struct ds_log *head)
{
	dnode->base++;
	list_add_tail(&head->list, &dnode->list);
}

static inline void ds_log_node_split_tail(struct ds_log *dnode, struct ds_log *tail)
{
	dnode->limit--;
	list_add(&tail->list, &dnode->list);
}

static inline void ds_log_node_split_middle(struct ds_log *dnode, struct ds_log *middle, struct ds_log *tail)
{
	dnode->limit = middle->base;
	list_add(&middle->list, &dnode->list);
	list_add(&tail->list, &middle->list);
}

static inline struct ds_log *init_ds_log_node(unsigned long base, unsigned long limit, long offset, unsigned long flag)
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

static inline void switch_pt_op_write(struct m_head_struct *mhead, pid_t tid)
{
	struct thread_pt_op *itr;
	itr = xa_load(&mhead->pt_op, tid);
	if (likely(itr)) {
		WRITE_ONCE(itr->op, PT_OP_WRITE);
	}
}

static inline void switch_pt_op_read(struct m_head_struct *mhead, pid_t tid)
{
	struct thread_pt_op *itr;
	itr = xa_load(&mhead->pt_op, tid);
	if (likely(itr)) {
		WRITE_ONCE(itr->op, PT_OP_READ);
	}
}

static inline void switch_pt_op_none(struct m_head_struct *mhead, pid_t tid)
{
	struct thread_pt_op *itr;
	itr = xa_load(&mhead->pt_op, tid);
	if (likely(itr)) {
		WRITE_ONCE(itr->op, PT_OP_NONE);
	}
}

static inline void add_thread_pt_op(struct m_head_struct *mhead, pid_t tid)
{
	struct thread_pt_op *op;
	op = kmalloc(sizeof(struct thread_pt_op), GFP_ATOMIC);
	if (!op) {
		printk(KERN_INFO "THREAD_PT_OP ERROR: kmalloc failed\n");
		return;
	}
	op->op = PT_OP_NONE;
	xa_store(&mhead->pt_op, tid, op, GFP_ATOMIC);
	printk(KERN_INFO "THREAD_PT_OP: added thread id %d in %d\n", tid, mhead->pid);
}

static inline void init_thread_pt_op(struct m_head_struct *mhead, struct task_struct *p)
{
	struct task_struct *t;
	rcu_read_lock();
	for_each_thread(p->group_leader, t) {
		add_thread_pt_op(mhead, t->pid);
	}
	rcu_read_unlock();
}

static inline struct m_head_struct *init_m_head_struct_node(pid_t pid, struct mm_struct *mm, struct task_struct *p)
{
	struct m_head_struct *itr;
	itr = kmalloc(sizeof(struct m_head_struct), GFP_ATOMIC);
	if (!itr) {
		printk(KERN_INFO "M_HEAD_STRUCT ERROR: kmalloc failed\n");
		return NULL;
	}
	itr->pid = pid;
	itr->mm = mm;
	xa_init(&itr->pt_op);
	init_thread_pt_op(itr, p);
	
	return itr;
}

static inline int add_m_head_struct_node(pid_t pid, struct mm_struct *mm, struct task_struct *p)
{
	struct m_head_struct *itr;

	itr = init_m_head_struct_node(pid, mm, p);
	if (!itr)
		return -1;

	spin_lock(&user_head_lock);
	list_add_tail(&itr->list, &user_head);
	spin_unlock(&user_head_lock);
	return 0;
}

static inline void delete_m_head_struct_node(struct m_head_struct *mhead)
{
	mhead->pid = 0;
	mhead->mm = NULL;
	xa_destroy(&mhead->pt_op);
	spin_lock(&user_head_lock);
	list_del(&mhead->list);
	spin_unlock(&user_head_lock);
	kfree(mhead);
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

static inline pte_t *pte_realloc(struct mm_struct *mm)
{
	struct page *new = (struct page *)pte_alloc_one(mm);
	unsigned long pte;
	if(!new)
		return	NULL;
	
	pte = (unsigned long)page_address(new);
	return (pte_t *)pte;
}

static inline int restore_replica_from_ds_log(pte_t **ptep, struct ds_log *itr, unsigned long start)
{
	unsigned long count;
	pte_t *pte = *(ptep);

#ifdef EMULATE_EMES_FOR_PTE
	unsigned int emes;
	get_random_bytes(&emes, sizeof(emes));
	if (emes % FAULT_RATIO == 0) {	
		printk(KERN_INFO "RECOVERY ERROR: EMEs in ds_log\n");
		return -1;
	}
#endif

	for (count=start; count < itr->limit; count++) {
		if(itr->base <= count)
			native_set_pte(pte, __pte(((pteval_t)(count - itr->offset) << PAGE_SHIFT) | itr->flag));
		pte++;
	}
	
	*(ptep) = pte;

	return 0;
}

static inline int restore_replica(unsigned long va_start, pte_t *pte, struct page *page)
{
	struct ds_log *itr;
	
	list_for_each_entry(itr, &page->ds_head, list) {
		if (restore_replica_from_ds_log(&pte, itr, va_start) < 0) {
			return -1;
		}
		va_start = itr->limit;
	}
	return 0;
}

static inline void copy_ds_log(struct page *before, struct page *after)
{
	struct ds_log *itr, *tmp;

	list_for_each_entry_safe(itr, tmp, &before->ds_head, list) {
		list_del(&itr->list);
		list_add_tail(&itr->list, &after->ds_head);
	}
}

static inline int restore_page(struct page *before, struct page *after)
{
#ifdef EMULATE_EMES_FOR_PTE
	unsigned int emes;
	get_random_bytes(&emes, sizeof(emes));
	if (emes % FAULT_RATIO == 0) {	
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
	if (count < 10) {
		preempt_count_sub(count);
	} else {
		preempt_count_add(1);
	}
	return count;
}

static inline void inc_preempt_after_schedule(int count)
{
	if (count < 10) {
		preempt_count_add(count);
	} else {
		preempt_count_sub(1);
	}
}
