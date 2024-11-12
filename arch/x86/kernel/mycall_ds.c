#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/export.h>
#include <asm/current.h>
#include <asm/io.h>
#include <asm/ds.h>
#include <asm/ds_struct.h>
#include <asm/page.h>
#include <asm/user_64.h>
#include <asm/pgtable.h>
#include <asm/pgtable_64.h>
#include <asm/pgalloc.h>
#include <asm/page_types.h>
#include <asm/pgtable_types.h>
#include <asm/paravirt.h>
#include <asm-generic/pgalloc.h>
#include <asm-generic/barrier.h>
#include <asm-generic/memory_model.h>

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


unsigned long vaddr;
// struct task_struct *target_task;

static void delete_pte_log_all(struct thread_log_list *t_lhead);

static pte_t *pte_offset_index(pmd_t *pmd, unsigned long index)
{
	return (pte_t *)pmd_page_vaddr(*pmd) + index;
}

static pmd_t *pmd_offset_index(pud_t *pud, unsigned long index)
{
	return pud_pgtable(*pud) + index;
}

static pud_t *pud_offset_index(p4d_t *p4d, unsigned long index)
{
	return p4d_pgtable(*p4d) + index;
}

static p4d_t *p4d_offset_index(pgd_t *pgd, unsigned long index)
{
	if(!pgtable_l5_enabled())
    		return (p4d_t *)pgd;
	printk(KERN_INFO "pagetable level 5");
  	return (p4d_t *)pgd_page_vaddr(*pgd) + index;
}

static pgd_t *pgd_offset_index(struct mm_struct *mm, unsigned long index)
{
  	return mm->pgd + index;
}


// static int get_pfn_scan_pte(pmd_t *pmdp, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
// {
//   	pte_t *ptep = pte_offset_index(pmdp, pte);
// 	*(ptepp) = ptep;

//   	if(pte_none(*ptep) || !pte_present(*ptep)) {
//     		// printk(KERN_INFO "pte %lu is not present.\n", pte);
//     		return 4;
//   	}
//   	return 1;
// }

// static int get_pfn_scan_pmd(pud_t *pudp, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
// {
//   	pmd_t *pmdp = pmd_offset_index(pudp, pmd);

//   	if(pmd_none(*pmdp) || !pmd_present(*pmdp) || pmd_large(*pmdp)){
//     		// printk(KERN_INFO "pmd %lu is not present.\n", pmd);
//     		return 5;
//   	}
//   // 	if(pmd_large(*pmdp)){
//   //   		pte_value = pmd_pfn(*pmdp);
// 		// pte_flag = pmd_flags(*pmdp);
//   //   		return 2;
//   // 	}
//   	return get_pfn_scan_pte(pmdp, pgd, pud, pmd, pte, ptepp);
// }

// static int get_pfn_scan_pud(p4d_t *p4dp, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
// {
//   	pud_t *pudp = pud_offset_index(p4dp, pud);
	  
//   	if(pud_none(*pudp) || !pud_present(*pudp) || pud_large(*pudp)){
// 	    	// printk(KERN_INFO "pud %lu is not present", pud);
// 	    	return 6;
//   	}
//   // 	if(pud_large(*pudp)){
//   //   		pte_value = pud_pfn(*pudp);
// 		// pte_flag = pud_flags(*pudp);
//   //   		return 3;
//   // 	}
//   	return get_pfn_scan_pmd(pudp, pgd, pud, pmd, pte, ptepp);  
// }

// static int get_pfn_scan_p4d(pgd_t *pgdp, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
// {
//   	p4d_t *p4dp = p4d_offset_index(pgdp, pgd);
	
// 	if(p4d_none(*p4dp) || !p4d_present(*p4dp)){
// 	    	// printk(KERN_INFO "p4d %lu is not present", pgd);
//     		return 7;
//   	}
//   	return get_pfn_scan_pud(p4dp, pgd, pud, pmd, pte, ptepp);
// }

// static int get_pfn_scan_pgd(struct mm_struct *mm, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
// {
//   	pgd_t *pgdp = pgd_offset_index(mm, pgd);

//   	if(pgd_none(*pgdp) || !pgd_present(*pgdp)){
// 	    	// printk(KERN_INFO "pgd %lu is not present.\n", pgd);
//     		return 7;
//   	}
//   	return get_pfn_scan_p4d(pgdp, pgd, pud, pmd, pte, ptepp);
// }

// static int search_pgtable_get_pfn(unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
// {
//   	struct mm_struct *mm = current->mm;

//   	if(pgd<0 || 512<=pgd || pud<0 || 512<=pud || pmd<0 || 512<=pmd || pte<0 || 512<=pte) {
//     		printk(KERN_INFO "error: The numbers are not appropriate.\n");
//     		return 0;
//   	}
//   	return get_pfn_scan_pgd(mm, pgd, pud, pmd, pte, ptepp);
// }

// LIST_HEAD(user_head);
// LIST_HEAD(ker_m_head);
// LIST_HEAD(usr_ds_head);
// LIST_HEAD(ker_ds_head);
LIST_HEAD(user_head);
LIST_HEAD(kern_head);

static long make_ds_va(unsigned long a, unsigned long b, unsigned long c, unsigned long d)
{
	unsigned long va = a << 27 | b << 18 | c << 9 | d;
	return va;	
}

static long make_ds_offset(long base, unsigned long pte_value)
{
	long offset = base - pte_value;
	return offset;
}

static struct ds_list *make_ds_node(unsigned long base, unsigned long limit, long offset, unsigned long flag)
{
	struct ds_list *list = kmalloc(sizeof(struct ds_list), GFP_KERNEL);
	if(!list)
		return NULL;

	list->base = base;
	list->limit = limit;
	list->offset = offset;
	list->flag = flag;
	return list;
}

static struct m_list *make_m_node(unsigned long va, unsigned long base)
{
	struct m_list *list = kmalloc(sizeof(struct m_list), GFP_KERNEL);
	if(!list)
		return NULL;

	list->va = va & PAGE_MASK;
	list->base = base;
	spin_lock_init(&list->ds_lock);
	spin_lock_init(&list->log_lock);
	INIT_LIST_HEAD(&list->ds_head);
	INIT_LIST_HEAD(&list->log_head);
	return list;
}

static struct pte_log_list *make_pte_log_node(unsigned long base, pte_t pte, int flag)
{
	struct pte_log_list *list = kmalloc(sizeof(struct pte_log_list), GFP_KERNEL);
	if(!list)
		return NULL;

	list->base = base;
	list->pte = pte;
	list->flag = flag;
	return list;
}

static struct thread_log_list *make_thread_log_node(u32 cpu, int commit)
{
	struct thread_log_list *list = kmalloc(sizeof(struct thread_log_list), GFP_KERNEL);
	if(!list)
		return NULL;

	list->cpu = cpu;
	list->commit = commit;
	INIT_LIST_HEAD(&list->head);
	return list;
}

static bool is_ds_node_merge(struct ds_list *prev, struct ds_list *next)
{
	if(prev->limit == next->base && prev->offset == next->offset && prev->flag == next->flag)
		return true;
	else
		return false;
}

static void ds_node_merge(struct ds_list *prev, struct ds_list *next)
{
	if(is_ds_node_merge(prev, next)) {
		prev->limit = next->limit;
		list_del(&next->list);
		kfree(next);
	}
}

// static bool is_add_m_node_usr(unsigned long base, struct m_head_list *m_head)
// {
// 	struct m_list *itr;
	
// 	list_for_each_entry(itr, &m_head->head, list){
// 		if(itr->base == base)
// 			return false;
// 		if(base < itr->base)
// 			break;
// 	}
// 	return true;
// }

static int add_first_m_node(unsigned long va, unsigned long base, struct m_head_list *m_head)
{
	struct m_list *mnode;

	if((mnode = make_m_node(va, base)) == NULL)
		return -ENOMEM;

	if(list_empty(&m_head->head)) { //no node
		list_add(&mnode->list, &m_head->head);
	}
	else {
		return -1;
	}
	return 0;
}
	
// static int add_m_node_usr(unsigned long va, unsigned long base, struct m_head_list *m_head)
// {
// 	struct m_list *mnode, *itr;

// 	if((mnode = make_m_node(va, base)) == NULL)
// 		return -ENOMEM;

// 	if(list_empty(&m_head->head)){ //no node
// 		list_add(&mnode->list, &m_head->head);
// 	}else{
// 		list_for_each_entry(itr, &m_head->head, list){
// 			if(base < itr->base){
// 				list_add_tail(&mnode->list, &itr->list);
// 				return 0;
// 			}
// 		}
// 		list_add_tail(&mnode->list, &m_head->head);
// 	}
// 	return 0;
// }

static int add_m_node(unsigned long va, unsigned long base, struct m_list *m_node)
{
	struct m_list *itr;

	if((itr = make_m_node(va, base)) == NULL)
		return -ENOMEM;

	list_add(&itr->list, &m_node->list);
	return 0;
}

static int add_tail_m_node(unsigned long va, unsigned long base, struct m_list *m_node)
{
	struct m_list *itr;

	if((itr = make_m_node(va, base)) == NULL)
		return -ENOMEM;

	list_add_tail(&itr->list, &m_node->list);
	return 0;
}

static int add_pte_log_node(unsigned long base, pte_t pte, int flag, struct thread_log_list *lnode)
{
	struct pte_log_list *itr;

	if((itr = make_pte_log_node(base, pte, flag)) == NULL)
		return -ENOMEM;

	list_add_tail(&itr->list, &lnode->head);
	return 0;
}

static int add_thread_log_node(u32 cpu, struct m_list *mnode)
{
	struct thread_log_list *itr;

	if((itr = make_thread_log_node(cpu, 0)) == NULL)
		return -ENOMEM;

	list_add_tail(&itr->list, &mnode->log_head);
	return 0;
}

// static bool is_add_m_node_ker(unsigned long base)
// {
// 	struct m_list *itr;
	
//  	list_for_each_entry(itr, &ker_m_head, list){
//  		if(itr->base == base)
//  			return false;
//  		if(base < itr->base)
//  			break;
//  	}
//  	return true;
// }

// static int add_m_node_ker(unsigned long va, unsigned long base)
// {
//  	struct m_list *mnode, *itr;
	
//  	if((mnode = make_m_node(va, base)) == NULL)
//  		return -ENOMEM;
//  	if(list_empty(&ker_m_head)){ //no node
//  		list_add(&mnode->list, &ker_m_head);
//  	}else{
//  		list_for_each_entry(itr, &ker_m_head, list){
//  			if(base < itr->base){
//  				list_add_tail(&mnode->list, &itr->list);
//  				return 0;
//  			}
//  		}
//  		list_add_tail(&mnode->list, &ker_m_head);
//  	}
//  	return 0;
// }

static void m_list_lock(struct m_head_list *mhead)
{
	spin_lock(&mhead->m_lock);
}

static void ds_list_lock(struct m_list *mnode)
{
	spin_lock(&mnode->ds_lock);
}

static void log_list_lock(struct m_list *mnode)
{
	spin_lock(&mnode->log_lock);
}

static void m_list_unlock(struct m_head_list *mhead)
{
	spin_unlock(&mhead->m_lock);
}

static void ds_list_unlock(struct m_list *mnode)
{
	spin_unlock(&mnode->ds_lock);
}

static void log_list_unlock(struct m_list *mnode)
{
	spin_unlock(&mnode->log_lock);
}

static int __make_pgd_m_list(unsigned long pgd_va, pid_t pid)
{
	struct m_head_list *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == pid) {
			m_list_lock(mhead);
			if(add_first_m_node(pgd_va & PAGE_MASK, PGD_FLAG_MASK, mhead) < 0) {
				m_list_unlock(mhead);
				return -1;
			}
			m_list_unlock(mhead);
			printk(KERN_INFO "make m pgd alloc %lx, %lx, %d\n", pgd_va & PAGE_MASK, PGD_FLAG_MASK, pid);
			break;
		}
	}
	return 0;
}

int make_pgd_m_list(unsigned long pgd_va)
{
	return __make_pgd_m_list(pgd_va, current->pid);
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
	printk(KERN_INFO "make m pud alloc %lx, %lx, %d\n", pud_va, base, pid);
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
			m_list_lock(mhead);
			if((base = get_p4d_base(p4d_va, pud_va, mhead, pid)) >= MAX_NUM) {
				m_list_unlock(mhead);
				return -1;
			}
			m_list_unlock(mhead);
			break;
		}
	}
	return 0;
}

int make_pud_m_list(unsigned long p4d_va, unsigned long pud_va)
{
	return __make_pud_m_list(p4d_va, pud_va, current->pid);
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
	printk(KERN_INFO "make m pmd alloc %lx, %lx, %d\n", pmd_va, base, pid);
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
			m_list_lock(mhead);
			if((base = get_pud_base(pud_va, pmd_va, mhead, pid)) >= MAX_NUM) {
				m_list_unlock(mhead);
				return -1;
			}
			m_list_unlock(mhead);
			break;
		}
	}
	return 0;
}

int make_pmd_m_list(unsigned long pud_va, unsigned long pmd_va)
{
	return __make_pmd_m_list(pud_va, pmd_va, current->pid);
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
	printk(KERN_INFO "make m pte alloc %lx, %lx, %d\n", pte_va, base, pid);
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
			m_list_lock(mhead);
			if((base = get_pmd_base(pmd_va, pte_va, mhead, pid)) >= MAX_NUM) {
				m_list_unlock(mhead);
				return -1;
			}
			m_list_unlock(mhead);
			break;
		}
	}
	return 0;
}

int make_pte_m_list(unsigned long pmd_va, unsigned long pte_va)
{
	return __make_pte_m_list(pmd_va, pte_va, current->pid);
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


static void delete_ds(struct ds_list *ds_node, struct ds_list *new)
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
			m_list_lock(mhead);
			list_for_each_entry(mnode, &mhead->head, list) {
				if(mnode->base & PTE_FLAG_MASK && mnode->va <= va && va < mnode->va + OFFSET_SIZE) {
					base = make_ds_va((mnode->base >> 27) & PT_PGTABLE_MASK, (mnode->base >> 18) & PT_PGTABLE_MASK, (mnode->base >> 9) & PT_PGTABLE_MASK, ((va - mnode->va) / 0x8) & PT_PGTABLE_MASK);
					break;
				}
			}
			m_list_unlock(mhead);

			if(base == MAX_NUM)
				goto err;

			if((dnode = make_ds_node(base, base+1, make_ds_offset(base, pte_value), pte_flag)) == NULL)
				goto err;

			ds_list_lock(mnode);
			if(list_empty(&mnode->ds_head)) {
				list_add(&dnode->list, &mnode->ds_head);
				goto end;
			}
			else {
				list_for_each_entry_reverse(prev, &mnode->ds_head, list) {
					if(prev->base <= dnode->base && dnode->limit <= prev->limit) {
						// printk(KERN_INFO "make ds hit ds %lx %lx %lx %lx %d\n", base, pte_value, pte_flag, va, pid);
						if(pte_value == 0 && pte_flag == 0) {
							delete_ds(prev, dnode);
							// printk(KERN_INFO "delete ds %lx-%lx\n", dnode->base, dnode->limit);
						}
						else if(dnode->offset != prev->offset) {
							// modify pte value
							modify_ds_offset(prev, dnode, mnode);
							printk(KERN_INFO "modify ds offset %lx %lx %lx %lx %d\n", base, pte_value, pte_flag, va, pid);
						}
						else if(dnode->flag != prev->flag) {
							// modify pte flag 
							if(!is_ds_write(prev) && is_ds_write(dnode)) {
								// ds_mkwrite
								// printk(KERN_INFO "make write %lx %lx-%lx", base, prev->base, prev->limit);
								modify_ds_flag(prev, dnode, mnode);
								printk(KERN_INFO "make write %lx %lx %lx %lx %d\n", base, pte_value, pte_flag, va, pid);
							}
							else if(is_ds_write(prev) && !is_ds_write(dnode)) {
								// ds_wrprotect
								// printk(KERN_INFO "make wrprotect %lx %lx-%lx", base, prev->base, prev->limit);
								modify_ds_flag(prev, dnode, mnode);
								printk(KERN_INFO "make wrprotect %lx %lx %lx %lx %d\n", base, pte_value, pte_flag, va, pid);
							}
							else {
								printk(KERN_INFO "not modify ds flag %lx  %lx->%lx %lx %d\n", base, prev->flag, pte_flag, va, pid);
							}
						}
						goto end;
					}
					else if(dnode->base >= prev->limit) {
						list_add(&dnode->list, &prev->list);
						// printk(KERN_INFO "make ds %lx %lx %lx %lx %d\n", base, pte_value, pte_flag, va, pid);
						if(list_is_last(&dnode->list, &mnode->ds_head)) {
							ds_node_merge(prev, dnode);
							goto end;
						}
						next = list_next_entry(dnode, list);
						ds_node_merge(dnode, next);
						ds_node_merge(prev, dnode);
						goto end;
					}
				}
				list_add(&dnode->list, &mnode->ds_head);
				// printk(KERN_INFO "make ds %lx %lx %lx %lx %d\n", base, pte_value, pte_flag, va, pid);
				next = list_next_entry(dnode, list);
				ds_node_merge(dnode, next);
				goto end;
			}
			break;
		}
	}
	
	// list_for_each_entry(dhead, &usr_ds_head, list){
	// 	if(dhead->pid == pid){
	// 		if((dnode = make_ds_node(base, base+1, make_ds_offset(base, pte_value), pte_flag)) == NULL)
	// 			return -ENOMEM;

	// 		if(list_empty(&dhead->head)){ //no node
	// 			list_add(&dnode->list, &dhead->head);
	// 			goto end;
	// 		}else{
	// 			list_for_each_entry_reverse(prev, &dhead->head, list){
	// 				if(prev->base <= dnode->base && dnode->limit <= prev->limit){
	// 					// printk(KERN_INFO "make ds hit ds %lx %lx %lx %lx %d\n", base, pte_value, pte_flag, va, pid);
	// 					if(pte_value == 0 && pte_flag == 0) {
	// 						delete_ds(prev, dnode->base, dnode->limit);
	// 						printk(KERN_INFO "delete ds %lx-%lx\n", dnode->base, dnode->limit);
	// 					}
	// 					else if(dnode->offset != prev->offset){
	// 						// modify pte value
	// 						modify_ds_offset(prev, dnode, dhead);
	// 						printk(KERN_INFO "modify ds offset %lx %lx %lx %lx %d\n", base, pte_value, pte_flag, va, pid);
	// 					}
	// 					else if(dnode->flag != prev->flag){
	// 						// modify pte flag 
	// 						if(!is_ds_write(prev) && is_ds_write(dnode)){
	// 							// ds_mkwrite
	// 							// printk(KERN_INFO "make write %lx %lx-%lx", base, prev->base, prev->limit);
	// 							modify_ds_flag(prev, dnode, dhead);
	// 							printk(KERN_INFO "make write %lx %lx %lx %lx %d\n", base, pte_value, pte_flag, va, pid);
	// 						}
	// 						else if(is_ds_write(prev) && !is_ds_write(dnode)){
	// 							// ds_wrprotect
	// 							// printk(KERN_INFO "make wrprotect %lx %lx-%lx", base, prev->base, prev->limit);
	// 							modify_ds_flag(prev, dnode, dhead);
	// 							printk(KERN_INFO "make wrprotect %lx %lx %lx %lx %d\n", base, pte_value, pte_flag, va, pid);
	// 						}
	// 						else{
	// 							printk(KERN_INFO "not modify ds flag %lx  %lx->%lx %lx %d\n", base, prev->flag, pte_flag, va, pid);
	// 						}
	// 					}
	// 					flag = 0;
	// 					goto end;
	// 				}
	// 				else if(dnode->base >= prev->limit){
	// 					list_add(&dnode->list, &prev->list);
	// 					if(list_is_last(&dnode->list, &dhead->head)){
	// 						ds_node_merge(prev, dnode);
	// 						goto end;
	// 					}
	// 					next = list_next_entry(dnode, list);
	// 					ds_node_merge(dnode, next);
	// 					ds_node_merge(prev, dnode);
	// 					goto end;
	// 				}
	// 			}
	// 			list_add(&dnode->list, &dhead->head);
	// 			next = list_next_entry(dnode, list);
	// 			ds_node_merge(dnode, next);
	// 			goto end;
	// 		}
	// 		break;
	// 	}
	// }
	
end:
	ds_list_unlock(mnode);
	return 0;
err:
	return -1;
}

int make_ds_list_usr(unsigned long va, pte_t pte)
{
	return __make_ds_list_usr(va, pte, current->pid);
}
EXPORT_SYMBOL_GPL(make_ds_list_usr);

int make_thread_log_list_usr(unsigned long va)
{
	struct m_head_list *mhead;
	struct m_list *mnode;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->pid) {
			list_for_each_entry(mnode, &mhead->head, list) {
				if(mnode->base & PTE_FLAG_MASK && mnode->va <= va && va < mnode->va + OFFSET_SIZE) {
					if(add_thread_log_node(current_thread_info()->cpu, mnode) < 0)
						goto err;
					printk(KERN_INFO "make thread log %d 0\n", current_thread_info()->cpu);
					break;
				}
			}
			break;
		}
	}
	return 0;
err:
	return -1;
}
EXPORT_SYMBOL_GPL(make_thread_log_list_usr);

int make_pte_log_list_usr(unsigned long va, pte_t pte, int flag)
{
	struct m_head_list *mhead;
	struct m_list *mnode;
	struct thread_log_list *lnode;
	unsigned long base;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->pid) {
			list_for_each_entry(mnode, &mhead->head, list) {
				if(mnode->base & PTE_FLAG_MASK && mnode->va <= va && va < mnode->va + OFFSET_SIZE) {
					list_for_each_entry(lnode, &mnode->log_head, list) {
						if(lnode->cpu == current_thread_info()->cpu) {
							base = make_ds_va((mnode->base >> 27) & PT_PGTABLE_MASK, (mnode->base >> 18) & PT_PGTABLE_MASK, (mnode->base >> 9) & PT_PGTABLE_MASK, ((va - mnode->va) / 0x8) & PT_PGTABLE_MASK);
							if(add_pte_log_node(base, pte, flag, lnode) < 0)
								goto err;
							printk(KERN_INFO "make pte log %lx %lx %lx %d 0\n", base, pte_pfn(pte), pte_flags(pte), flag);
							break;
						}
					}
					break;
				}
			}
			break;
		}
	}
	return 0;
err:
	return -1;
}
EXPORT_SYMBOL_GPL(make_pte_log_list_usr);

int delete_thread_log_list_usr(unsigned long va)
{
	struct m_head_list *mhead;
	struct m_list *mnode;
	struct thread_log_list *t_lnode;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->pid) {
			list_for_each_entry(mnode, &mhead->head, list) {
				if(mnode->base & PTE_FLAG_MASK && mnode->va <= va && va < mnode->va + OFFSET_SIZE) {
					list_for_each_entry(t_lnode, &mnode->log_head, list) {
						if(t_lnode->cpu == current_thread_info()->cpu) {
							delete_pte_log_all(t_lnode);
							t_lnode->commit = 1;
							printk(KERN_INFO "delete thread log %d %d\n", t_lnode->cpu, t_lnode->commit);
							list_del(&t_lnode->list);
							kfree(t_lnode);
							break;
						}
					}
					break;
				}
			}
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(delete_thread_log_list_usr);

int delete_pte_log_list_usr(unsigned long va, int flag)
{
	struct m_head_list *mhead;
	struct m_list *mnode;
	struct thread_log_list *t_lnode;
	struct pte_log_list *p_lnode;
	unsigned long base;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->pid) {
			list_for_each_entry(mnode, &mhead->head, list) {
				if(mnode->base & PTE_FLAG_MASK && mnode->va <= va && va < mnode->va + OFFSET_SIZE) {
					list_for_each_entry(t_lnode, &mnode->log_head, list) {
						if(t_lnode->cpu == current_thread_info()->cpu) {
							base = make_ds_va((mnode->base >> 27) & PT_PGTABLE_MASK, (mnode->base >> 18) & PT_PGTABLE_MASK, (mnode->base >> 9) & PT_PGTABLE_MASK, ((va - mnode->va) / 0x8) & PT_PGTABLE_MASK);
							list_for_each_entry(p_lnode, &t_lnode->head, list) {
								if(p_lnode->base == base && p_lnode->flag == flag){
									printk(KERN_INFO "delete pte log %lx %lx %lx %d\n", p_lnode->base, pte_pfn(p_lnode->pte), pte_flags(p_lnode->pte), p_lnode->flag);
									list_del(&p_lnode->list);
									kfree(p_lnode);
									break;
								}
							}
							break;
						}
					}
					break;
				}
			}
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(delete_pte_log_list_usr);

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

static int get_ptep(pmd_t *pmdp, pid_t pid, unsigned long pte, pte_t **ptepp)
{
  	pte_t *ptep = pte_offset_index(pmdp, pte);
	*(ptepp) = ptep;

  	if(pte_none(*ptep) || !pte_present(*ptep)) {
    	// printk(KERN_INFO "pte %lu is not present.\n", pte);
    	return -1;
  	}

  	return 0;
}

static int get_pmdp(pud_t *pudp, pid_t pid, unsigned long pmd, pmd_t **pmdpp)
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

static int get_pudp(p4d_t *p4dp, pid_t pid, unsigned long pud, pud_t **pudpp)
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

static int get_p4dp(pgd_t *pgdp, pid_t pid, unsigned long p4d, p4d_t **p4dpp)
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

static int get_pgdp(struct mm_struct *mm, pid_t pid, unsigned long pgd, p4d_t **p4dpp)
{
  	pgd_t *pgdp = pgd_offset_index(mm, pgd);

  	if(pgd_none(*pgdp) || !pgd_present(*pgdp)){
	    // printk(KERN_INFO "pgd %lu is not present.\n", pgd);
    	return -1;
  	}

	if(get_p4dp(pgdp, pid, pgd, p4dpp) < 0){
		return -1;
	}

  	return 0;
}

static long make_ds_list_usr_from_pgtable(struct task_struct *p)
{
	pid_t pid = p->pid;
	// pgd_t *pgdp;
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep;
	
	int flag=0;
	
	if(__make_pgd_m_list((unsigned long)p->mm->pgd, pid) < 0)
		return -1;

	for(unsigned long pgd=0; pgd < USER_MAX; pgd++) {
		if(get_pgdp(p->mm, pid, pgd, &p4dp) == 0) {
			for(unsigned long pud=0; pud<MAX; pud++) {
				if(get_pudp(p4dp, pid, pud, &pudp) == 0) {
					for(unsigned long pmd=0; pmd<MAX; pmd++) {
						if(get_pmdp(pudp, pid, pmd, &pmdp) == 0) {
							for(unsigned long pte=0; pte<MAX; pte++) {
			                	if(get_ptep(pmdp, pid, pte, &ptep) == 0) {
									if(flag == 0) {
										vaddr = (unsigned long)ptep;
										flag = 1;
									}

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

static int print_usr_ds(pid_t pid)
{
	struct ds_list *dnode;
	struct m_head_list *mhead;
	struct m_list *mnode;
	int count = 0;

	struct file *file;
	char *filename = "./usr_ds_txt";
	int size;
	char *buf;
    loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)) {
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
    buf = kmalloc(PATH_MAX, GFP_KERNEL);
    if(!buf) {
		filp_close(file, NULL);
		return -1;
	}
	memset(buf, '\0', 256);

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == pid) {
		// if(mhead->pid == target_task->pid) {
			m_list_lock(mhead);
			printk(KERN_INFO "ds pid: %d\n", pid);
			size = sprintf(buf, "ds pid: %d\n", pid);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);

			list_for_each_entry(mnode, &mhead->head, list) {
				size = sprintf(buf, "m list %lx\n", mnode->base);
				kernel_write(file, buf, size, &pos);
				vfs_fsync_range(file, 0, size, 1);

				ds_list_lock(mnode);
				list_for_each_entry(dnode, &mnode->ds_head, list) {
					// printk(KERN_INFO "%lx %lx %lx %lx  %lx\n", dnode->base, dnode->limit, dnode->offset, dnode->flag, __pa((unsigned long)dnode));
					size = sprintf(buf, "  %lx %lx %lx %lx\n", dnode->base, dnode->limit, dnode->offset, dnode->flag);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
					count++;
				}
				ds_list_unlock(mnode);
			}
			m_list_unlock(mhead);
			break;
		}
	}
	printk(KERN_INFO "user ds count %d, pid %d\n", count, pid);
	size = sprintf(buf, "user ds count %d, pid %d\n", count, pid);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);

	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}

// static int print_ker_ds(void)
// {
// 	struct ds_list *itr;
// 	int count = 0;

// 	struct file *file;
// 	char *filename = "./ker_ds_txt";
// 	int size;
// 	char *buf;
//         loff_t pos = 0;

// 	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
// 	if(IS_ERR(file)){
// 		printk("pre_file open err=%ld", PTR_ERR(file));
// 		return -1;
// 	}
	
//         buf = kmalloc(PATH_MAX, GFP_KERNEL);
//         if(!buf)
// 		return -1;
// 	memset(buf, '\0', 100);
	
// 	list_for_each_entry(itr, &ker_ds_head, list){
// 		// printk(KERN_INFO "%lx %lx %lx %lx   %lx\n", itr->base, itr->limit, itr->offset, itr->flag, __pa((unsigned long)itr));
// 		size = sprintf(buf, "%lx %lx %lx %lx   %lx\n", itr->base, itr->limit, itr->offset, itr->flag, __pa((unsigned long)itr));
// 		kernel_write(file, buf, size, &pos);
// 		vfs_fsync_range(file, 0, size, 1);
// 		count++;
// 	}
// 	printk(KERN_INFO "kern ds count %d\n", count);
// 	size = sprintf(buf, "kern ds count %d\n", count);
// 	kernel_write(file, buf, size, &pos);
// 	vfs_fsync_range(file, 0, size, 1);

// 	kfree(buf);
// 	filp_close(file, NULL);
	
// 	return 0;
// }

SYSCALL_DEFINE1(mycall_ds_search, pid_t, pid)
{
	print_usr_ds(pid);
	// print_ker_ds();
	return 0;
}

static int print_usr_ds2(pid_t pid)
{
	struct ds_list *dnode;
	struct m_head_list *mhead;
	struct m_list *mnode;
	int count = 0;

	struct file *file;
	char *filename = "./usr_ds_txt2";
	int size;
	char *buf;
    loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)) {
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
    buf = kmalloc(PATH_MAX, GFP_KERNEL);
    if(!buf) {
		filp_close(file, NULL);
		return -1;
	}
	memset(buf, '\0', 256);

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == pid) {
		// if(mhead->pid == target_task->pid) {
			m_list_lock(mhead);
			printk(KERN_INFO "ds pid: %d\n", pid);
			size = sprintf(buf, "ds pid: %d\n", pid);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);

			list_for_each_entry(mnode, &mhead->head, list) {
				size = sprintf(buf, "m list %lx\n", mnode->base);
				kernel_write(file, buf, size, &pos);
				vfs_fsync_range(file, 0, size, 1);

				ds_list_lock(mnode);
				list_for_each_entry(dnode, &mnode->ds_head, list) {
					// printk(KERN_INFO "%lx %lx %lx %lx   %lx\n", dnode->base, dnode->limit, dnode->offset, dnode->flag, __pa((unsigned long)dnode));
					size = sprintf(buf, "  %lx %lx %lx %lx\n", dnode->base, dnode->limit, dnode->offset, dnode->flag);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
					count++;
				}
				ds_list_unlock(mnode);
			}
			m_list_unlock(mhead);
			break;
		}
	}
	printk(KERN_INFO "user ds count %d, pid %d\n", count, pid);
	size = sprintf(buf, "user ds count %d, pid %d\n", count, pid);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);

	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}

SYSCALL_DEFINE1(mycall_ds_search2, pid_t, pid)
{
	print_usr_ds2(pid);
	return 0;
}

static int print_usr_m(pid_t pid)
{
	struct m_list *itr;
	struct m_head_list *m_head;
	int count=0;
	
	struct file *file;
	char *filename = "./usr_m_txt";
	int size;
	char *buf;
    loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)) {
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
    buf = kmalloc(PATH_MAX, GFP_KERNEL);
    if(!buf) {
		filp_close(file, NULL);
		return -1;
	}
	memset(buf, '\0', 256);

	list_for_each_entry(m_head, &user_head, list) {
		if(m_head->pid == pid) {
		// if(m_head->pid == target_task->pid){
			m_list_lock(m_head);
			printk(KERN_INFO "m pid: %d\n", pid);
			size = sprintf(buf, "m pid: %d\n", pid);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			list_for_each_entry(itr, &m_head->head, list) {
				// printk(KERN_INFO "%lx %lx   %lx\n",itr->va, itr->base, __pa((unsigned long)itr));
				size = sprintf(buf, "%lx %lx\n",itr->va, itr->base);
				kernel_write(file, buf, size, &pos);
				vfs_fsync_range(file, 0, size, 1);
				count++;
			}
			m_list_unlock(m_head);
			break;
		}
	}
	printk(KERN_INFO "user m count %d, pid %d\n", count, pid);
	size = sprintf(buf, "user m count %d pid %d\n", count, pid);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);

	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}

// static int print_ker_m(void)
// {
// 	struct m_list *itr;
// 	int count=0;
	
// 	struct file *file;
// 	char *filename = "./ker_m_txt";
// 	int size;
// 	char *buf;
//         loff_t pos = 0;

// 	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
// 	if(IS_ERR(file)){
// 		printk("pre_file open err=%ld", PTR_ERR(file));
// 		return -1;
// 	}
	
//         buf = kmalloc(PATH_MAX, GFP_KERNEL);
//         if(!buf)
// 		return -1;
// 	memset(buf, '\0', 100);
	
// 	list_for_each_entry(itr, &ker_m_head, list){
// 		// printk(KERN_INFO "%lx %lx   %lx\n",itr->va, itr->base, __pa((unsigned long)itr));
// 		size = sprintf(buf, "%lx %lx   %lx\n",itr->va, itr->base, __pa((unsigned long)itr));
// 		kernel_write(file, buf, size, &pos);
// 		vfs_fsync_range(file, 0, size, 1);
// 		count++;
// 	}
// 	printk(KERN_INFO "kern m count %d\n", count);
// 	size = sprintf(buf, "kern m count %d\n", count);
// 	kernel_write(file, buf, size, &pos);
// 	vfs_fsync_range(file, 0, size, 1);

// 	kfree(buf);
// 	filp_close(file, NULL);
	
// 	return 0;
// }

SYSCALL_DEFINE1(mycall_m_search, pid_t, pid)
{
	print_usr_m(pid);
	// print_ker_m();
	return 0;
}

static int print_usr_m2(pid_t pid)
{
	struct m_list *itr;
	struct m_head_list *m_head;
	int count=0;
	
	struct file *file;
	char *filename = "./usr_m_txt2";
	int size;
	char *buf;
    loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)) {
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
	buf = kmalloc(PATH_MAX, GFP_KERNEL);
	if(!buf) {
		filp_close(file, NULL);
		return -1;
	}
	memset(buf, '\0', 256);

	list_for_each_entry(m_head, &user_head, list) {
		if(m_head->pid == pid) {
		// if(m_head->pid == target_task->pid){
			m_list_lock(m_head);
			printk(KERN_INFO "m pid: %d\n", pid);
			size = sprintf(buf, "m pid: %d\n", pid);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			list_for_each_entry(itr, &m_head->head, list) {
				// printk(KERN_INFO "%lx %lx   %lx\n",itr->va, itr->base, __pa((unsigned long)itr));
				size = sprintf(buf, "%lx %lx\n",itr->va, itr->base);
				kernel_write(file, buf, size, &pos);
				vfs_fsync_range(file, 0, size, 1);
				count++;
			}
			m_list_unlock(m_head);
			break;
		}
	}
	printk(KERN_INFO "user m count %d, pid %d\n", count, pid);
	size = sprintf(buf, "user m count %d pid %d\n", count, pid);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);

	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}

SYSCALL_DEFINE1(mycall_m_search2, pid_t, pid)
{
	print_usr_m2(pid);
	return 0;
}

static long register_pid(pid_t pid)
{
	struct m_head_list *m_node;

	// target_task = current;
	
	// list_for_each_entry(ds_node, &usr_ds_head, list){
	// 	if(ds_node->pid == pid){
	// 		goto end;
	// 	}
	// }
	list_for_each_entry(m_node, &user_head, list) {
		if(m_node->pid == pid){
			goto end;
		}
	}

	// ds_node = kmalloc(sizeof(struct ds_head_list), GFP_KERNEL);
	m_node = kmalloc(sizeof(struct m_head_list), GFP_KERNEL);
	// if(!ds_node || !m_node)
	if(!m_node)
		return -1;
	// ds_node->pid = pid;
	m_node->pid = pid;
	spin_lock_init(&m_node->m_lock);
	// INIT_LIST_HEAD(&ds_node->head);
	INIT_LIST_HEAD(&m_node->head);
	// list_add(&ds_node->list, &usr_ds_head);
	list_add(&m_node->list, &user_head);

	printk(KERN_INFO "init pid %d\n",pid);
end:
	return 0;
}

SYSCALL_DEFINE1(mycall_ds_register_pid, pid_t, pid)
{
	return register_pid(pid);
}


static int get_pmd_scan_pmd(pud_t *pudp, unsigned long pgd, unsigned long pud, unsigned long pmd, pmd_t **pmdpp)
{
  	pmd_t *pmdp = pmd_offset_index(pudp, pmd);
	*(pmdpp) = pmdp;

  	if(pmd_none(*pmdp) || !pmd_present(*pmdp) || pmd_large(*pmdp)) {
    		// printk(KERN_INFO "pmd %lu is not present.\n", pmd);
    		return 4;
  	}
	if(pgd < 0x100) // in user space
		return 1;
	
  	return 2; // in kernel space
}

static int get_pmd_scan_pud(p4d_t *p4dp, unsigned long pgd, unsigned long pud, unsigned long pmd, pmd_t **pmdp)
{
  	pud_t *pudp = pud_offset_index(p4dp, pud);
	  
  	if(pud_none(*pudp) || !pud_present(*pudp) || pud_large(*pudp)) {
	    	// printk(KERN_INFO "pud %lu is not present", pud);
	    	return 5;
  	}
  	return get_pmd_scan_pmd(pudp, pgd, pud, pmd, pmdp);  
}

static int get_pmd_scan_p4d(pgd_t *pgdp, unsigned long pgd, unsigned long pud, unsigned long pmd, pmd_t **pmdp)
{
  	p4d_t *p4dp = p4d_offset_index(pgdp, pgd);
	
	if(p4d_none(*p4dp) || !p4d_present(*p4dp)) {
	    	// printk(KERN_INFO "p4d %lu is not present", pgd);
    		return 6;
  	}
  	return get_pmd_scan_pud(p4dp, pgd, pud, pmd, pmdp);
}

static int get_pmd_scan_pgd(struct mm_struct *mm, unsigned long pgd, unsigned long pud, unsigned long pmd, pmd_t **pmdp)
{
  	pgd_t *pgdp = pgd_offset_index(mm, pgd);

  	if(pgd_none(*pgdp) || !pgd_present(*pgdp)) {
	    	// printk(KERN_INFO "pgd %lu is not present.\n", pgd);
    		return 6;
  	}
  	return get_pmd_scan_p4d(pgdp, pgd, pud, pmd, pmdp);
}

static int search_pgtable_get_pmd(unsigned long base, pmd_t **pmdp)
{
  	struct mm_struct *mm = current->mm;
	// struct mm_struct *mm = target_task->mm;

	unsigned long pgd = (base >> 27) & PT_PGTABLE_MASK;
	unsigned long pud = (base >> 18) & PT_PGTABLE_MASK;
	unsigned long pmd = (base >> 9) & PT_PGTABLE_MASK;

  	if(pgd<0 || 512<=pgd || pud<0 || 512<=pud || pmd<0 || 512<=pmd) {
    		printk(KERN_INFO "error: The numbers are not appropriate.\n");
    		return 0;
  	}
  	return get_pmd_scan_pgd(mm, pgd, pud, pmd, pmdp);
}

static void modify_pte_m_list(struct m_list *itr, unsigned long va)
{
	itr->va = va;
}

static void pmd_repopulate(struct mm_struct *mm, pmd_t *pmd, pte_t *pte)
{
	paravirt_alloc_pte(mm, __pa(pte) >> PAGE_SHIFT);
	set_pmd(pmd, __pmd(__pa(pte) | pmd_flags(*pmd)));
}

static void pmd_reinstall(struct mm_struct *mm, pmd_t *pmdp, pte_t *ptep, struct m_list *itr)
{
	spinlock_t *ptl = pmd_lock(mm, pmdp);
	
	if (!pmd_none(*pmdp) && pmd_present(*pmdp)) {
		smp_wmb(); /* See comment in pmd_install() */
		pmd_repopulate(mm, pmdp, ptep);
		modify_pte_m_list(itr, (unsigned long)ptep);
		ptep = NULL;
	}
	spin_unlock(ptl);
	
	if (ptep)
		pte_free_recover(mm, virt_to_page(ptep));
}

static pte_t *pte_realloc(struct mm_struct *mm)
{
	struct page *new = (struct page *)pte_alloc_one(mm);
	unsigned long pte;
	if(!new)
		return	NULL;
	
	pte = (unsigned long)page_address(new);
	return (pte_t *)pte;
}

static void pmd_reinstall_kernel(pmd_t *pmdp, pte_t *ptep, struct m_list *itr)
{
	spin_lock(&init_mm.page_table_lock);
	if (!pmd_none(*pmdp) && pmd_present(*pmdp)) {
		smp_wmb(); /* See comment in pmd_install() */
		pmd_repopulate(&init_mm, pmdp, ptep);
		modify_pte_m_list(itr, (unsigned long)ptep);
		ptep = NULL;
	}
	spin_unlock(&init_mm.page_table_lock);
	
	if (ptep)
		pte_free_kernel(&init_mm, ptep);
}

static pte_t *pte_realloc_kernel(void)
{
	pte_t *new = pte_alloc_one_kernel(&init_mm);
	if (!new)
		return NULL;
	return new;
}

static void dup_pte(pte_t **ptep, struct ds_list *itr, unsigned long start, unsigned long end)
{
	unsigned long count;
	pte_t *pte = *(ptep);

	for(count=start; count < itr->limit; count++) {
		if(itr->base <= count)
			set_pte_recover(pte, __pte(((pteval_t)(count - itr->offset) << PAGE_SHIFT) | itr->flag));
		pte++;
	}
	
	*(ptep) = pte;
}

static void update_pgtable(unsigned long va_start, pte_t *pte, struct m_list *mnode, struct file *file, loff_t *pos)
{
	struct ds_list *itr;
	unsigned long va_end;
	
	int size;
	char *buf;
	
    buf = kmalloc(PATH_MAX, GFP_KERNEL);
	memset(buf, '\0', 256);

	va_end = va_start | PT_PGTABLE_MASK;

	// printk(KERN_INFO "%ld-%ld-%ld-0  %lx %lx\n", (va_start >> 27) & PT_PGTABLE_MASK, (va_start >> 18) & PT_PGTABLE_MASK, (va_start >> 9) & PT_PGTABLE_MASK, va_start, va_end);
	size = sprintf(buf, "%ld-%ld-%ld-0  %lx %lx\n", (va_start >> 27) & PT_PGTABLE_MASK, (va_start >> 18) & PT_PGTABLE_MASK, (va_start >> 9) & PT_PGTABLE_MASK, va_start, va_end);
	kernel_write(file, buf, size, pos);
	vfs_fsync_range(file, 0, size, 1);

	ds_list_lock(mnode);
	list_for_each_entry(itr, &mnode->ds_head, list) {
		// printk(KERN_INFO "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
		size = sprintf(buf, "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
		kernel_write(file, buf, size, pos);
		vfs_fsync_range(file, 0, size, 1);
		
		dup_pte(&pte, itr, va_start, va_end);
		va_start = itr->limit;
	}
	ds_list_unlock(mnode);
	return;
}

// static int update_pgtable_ker(unsigned long va_start, pte_t *pte, struct file *file, loff_t *pos)
// {
// 	struct ds_list *itr;
// 	unsigned long va_end;
// 	int flag = 0;
	
// 	int size;
// 	char *buf;
	
//     buf = kmalloc(PATH_MAX, GFP_KERNEL);
// 	memset(buf, '\0', 100);

// 	va_end = va_start | PT_PGTABLE_MASK;

// 	// printk(KERN_INFO "%ld-%ld-%ld-0  %lx %lx\n", (va_start >> 27) & PT_PGTABLE_MASK, (va_start >> 18) & PT_PGTABLE_MASK, (va_start >> 9) & PT_PGTABLE_MASK, va_start, va_end);
// 	size = sprintf(buf, "%ld-%ld-%ld-0  %lx %lx\n", (va_start >> 27) & PT_PGTABLE_MASK, (va_start >> 18) & PT_PGTABLE_MASK, (va_start >> 9) & PT_PGTABLE_MASK, va_start, va_end);
// 	kernel_write(file, buf, size, pos);
// 	vfs_fsync_range(file, 0, size, 1);

// 	list_for_each_entry(itr, &ker_ds_head, list){
// 		if(itr->limit <= va_start){
// 			continue; // not hit yet
// 		}else if(va_end < itr->base){
// 			break; // already finished
// 		}else{ // recover pgtable from ds
// 			// printk(KERN_INFO "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
// 			size = sprintf(buf, "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
// 			kernel_write(file, buf, size, pos);
// 			vfs_fsync_range(file, 0, size, 1);
			
// 			dup_pte(&pte, itr, va_start, va_end);
// 			va_start = itr->limit;
// 			flag = 1;
// 		}
// 	}
// 	return flag;
// }

static int __recover_pgtable(unsigned long va_start, struct m_list *mnode, struct file *file, loff_t *pos)
{
	pmd_t *pmdp;
	pte_t *ptep_old;
	pte_t *ptep_new;
	int num;

	int size;
	char *buf;
	
    buf = kmalloc(PATH_MAX, GFP_KERNEL);
	memset(buf, '\0', 256);
		
	if((num = search_pgtable_get_pmd(va_start, &pmdp)) == 1){ // in user
		ptep_old = pte_offset_index(pmdp, 0);

		// printk(KERN_INFO "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
		size = sprintf(buf, "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
		kernel_write(file, buf, size, pos);
		vfs_fsync_range(file, 0, size, 1);
		// printk(KERN_INFO "pte before: %lx\n", (unsigned long)__pa(ptep_old));
		size = sprintf(buf, "pte before: %lx\n", (unsigned long)__pa(ptep_old));
		kernel_write(file, buf, size, pos);
		vfs_fsync_range(file, 0, size, 1);
		
		ptep_new = pte_realloc(current->mm);
		// ptep_new = pte_realloc(target_task->mm);
		
		if(!ptep_new){
			printk(KERN_INFO "out of memory\n");
			goto err;
		}
		
		update_pgtable(va_start, ptep_new, mnode, file, pos);
		pmd_reinstall(current->mm, pmdp, ptep_new, mnode);
		// pmd_reinstall(target_task->mm, pmdp, ptep_new, mnode);

		// printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
		size = sprintf(buf, "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
		kernel_write(file, buf, size, pos);
		vfs_fsync_range(file, 0, size, 1);

		// printk(KERN_INFO "pte after:  %lx", (unsigned long)__pa(ptep_new));
		size = sprintf(buf, "pte after: %lx\n",(unsigned long)__pa(ptep_new));
		kernel_write(file, buf, size, pos);
		vfs_fsync_range(file, 0, size, 1);
	}
	else if(num == 2) { // in kernel
		ptep_old = pte_offset_index(pmdp, 0);
		
		// printk(KERN_INFO "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
		size = sprintf(buf, "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
		kernel_write(file, buf, size, pos);
		vfs_fsync_range(file, 0, size, 1);
		// printk(KERN_INFO "pte before: %lx\n", (unsigned long)__pa(ptep_old));
		size = sprintf(buf, "pte before: %lx\n", (unsigned long)__pa(ptep_old));
		kernel_write(file, buf, size, pos);
		vfs_fsync_range(file, 0, size, 1);
		
		ptep_new = pte_realloc_kernel();
		
		if(!ptep_new){
			printk(KERN_INFO "out of memory\n");
			goto err;
		}
		
		update_pgtable(va_start, ptep_new, mnode, file, pos);
		pmd_reinstall_kernel(pmdp, ptep_new, mnode);
		
		// printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
		size = sprintf(buf, "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
		kernel_write(file, buf, size, pos);
		vfs_fsync_range(file, 0, size, 1);

		// printk(KERN_INFO "pte after:  %lx", (unsigned long)__pa(ptep_new));
		size = sprintf(buf, "pte after: %lx\n",(unsigned long)__pa(ptep_new));
		kernel_write(file, buf, size, pos);
		vfs_fsync_range(file, 0, size, 1);
	}
	else if(num == 0){
		goto err;
	}
	else{
		return num - 3;
	}
	return 0;
err:
	return -1;
}

static long recover_all_pgtable(void)
{
	struct m_list *itr;
	struct m_head_list *m_head;
	
	unsigned long va_start;
	unsigned long base;
	int count = 0;

	struct file *file;
	char *filename = "./write_log_txt";
    loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)) {
		printk("pre_file open err=%ld", PTR_ERR(file));
		goto err;
	}
	
	list_for_each_entry(m_head, &user_head, list) {
		if(m_head->pid == current->pid) {
		// if(m_head->pid == target_task->pid){
			m_list_lock(m_head);
			// printk(KERN_INFO "target_task pid: %d\n",target_task->pid);
			for(unsigned long a=0; a<USER_MAX; a++) {
		        for(unsigned long b=0; b<MAX; b++) {
		        	for(unsigned long c=0; c<MAX; c++) {
						va_start = make_ds_va(a, b, c, 0);
		
						list_for_each_entry(itr, &m_head->head, list) {
							base = itr->base & PT_PGTABLE_MASK_NOT;
							if(base == va_start) {
								if((count = __recover_pgtable(va_start, itr, file, &pos)) < 0) {
									m_list_unlock(m_head);
									goto err;
								}
							}else if(va_start < base){
								break;
							}
						}
						if(--count > 0)
							break;
						count = 0;
					}
					if(--count > 0)
						break;
					count = 0;
				}
				if(--count > 0)
					break;
				count = 0;
			}
			m_list_unlock(m_head);
			break;
		}
	}

	filp_close(file, NULL);
	return 0;
err:
	return -1;	
}


SYSCALL_DEFINE0(mycall_recover_all_pgtable)
{
	int ret = -1;
	printk(KERN_INFO "start recover pgtable\n");
	ret = recover_all_pgtable();
	printk(KERN_INFO "end recover pgtable\n");
	return ret;
}

static long recover_pgtable(unsigned long va)
{
	struct m_list *itr;
	struct m_head_list *m_head;
	
	struct file *file;
	char *filename = "./write_log_txt";
    loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)) {
		printk("pre_file open err=%ld", PTR_ERR(file));
		goto err;
	}
	
	list_for_each_entry(m_head, &user_head, list) {
		if(m_head->pid == current->pid) {
		// if(m_head->pid == target_task->pid) {
			m_list_lock(m_head);
			list_for_each_entry(itr, &m_head->head, list) {
				if(itr->base & PTE_FLAG_MASK && itr->va <= va && va < itr->va + OFFSET_SIZE) {
					printk(KERN_INFO "recover pte found %lx %lx\n", itr->base, itr->va);
					if(__recover_pgtable(itr->base & PT_PGTABLE_MASK_NOT, itr, file, &pos) < 0) {
						m_list_unlock(m_head);
						goto err;
					}
					break;
				}
			}
			m_list_unlock(m_head);
			break;
		}
	}
	
	filp_close(file, NULL);
	return 0;
err:
	printk(KERN_INFO "recover pte va:%lx error!\n",va);
	return -1;
}

SYSCALL_DEFINE1(mycall_recover_pgtable, unsigned long, va)
{
	int ret = -1;
	printk(KERN_INFO "start recover pgtable %lx\n",va);
	ret = recover_pgtable(vaddr);
	printk(KERN_INFO "end recover pgtable %lx\n",va);
	return ret;
}

// void delete_ds(struct ds_list *itr, unsigned long start, unsigned long end)
// {
// 	struct ds_list *next;

// 	if(itr->base < start && end < itr->limit){ // base->start, end->limit
// 		// printk(KERN_INFO "    %lx %lx", itr->base, itr->limit);
// 		if((next = make_ds_node(end, itr->limit, itr->offset, itr->flag)) == NULL)
// 			goto out;
// 		itr->limit = start;
// 		list_add(&next->list, &itr->list);
// 		goto out;
// 	}

// 	while(end > itr->base){
// 		// printk(KERN_INFO "    %lx %lx", itr->base, itr->limit);
// 		next = list_next_entry(itr, list);
// 		if(itr->base < start && itr->limit <= end){ // base->start
// 			itr->limit = start;
// 		}else if(start <= itr->base && end < itr->limit){ // end->limit
// 			itr->base = end;
// 		}else if(start <= itr->base && itr->limit <= end){ // all delete
// 			list_del(&itr->list);
// 			kfree(itr);
// 		}
// 		itr = next;
// 	}
// out:
// 	return;
// }

static void delete_ds_all(struct m_list *m_node)
{
	struct ds_list *itr, *tmp;

	ds_list_lock(m_node);
	list_for_each_entry_safe(itr, tmp, &m_node->ds_head, list) {
		list_del(&itr->list);
		kfree(itr);
	}
	ds_list_unlock(m_node);
}

static void delete_pte_log_all(struct thread_log_list *t_lhead)
{
	struct pte_log_list *itr, *tmp;

	list_for_each_entry_safe(itr, tmp, &t_lhead->head, list) {
		list_del(&itr->list);
		kfree(itr);
	}
}

static void delete_thread_log_all(struct m_list *m_node)
{
	struct thread_log_list *itr, *tmp;

	list_for_each_entry_safe(itr, tmp, &m_node->log_head, list) {
		delete_pte_log_all(itr);
		list_del(&itr->list);
		kfree(itr);
	}
}

void delete_ds_m_free_pte(unsigned long va)
{
	// struct ds_list *ds_node;
	struct m_list *m_node;
	struct m_head_list *m_head;

	unsigned long va_start = 0, va_end = 0;

	list_for_each_entry(m_head, &user_head, list) {
		if(m_head->pid == current->pid) {
			// printk(KERN_INFO "delete m pte %lx", va);
			m_list_lock(m_head);
			list_for_each_entry(m_node, &m_head->head, list) {
				if(m_node->base & PTE_FLAG_MASK && m_node->va <= va && va < m_node->va + OFFSET_SIZE) {
					va_start = m_node->base & PT_PGTABLE_MASK_NOT;
					va_end = va_start + PT_PGTABLE_SIZE;
	
					delete_ds_all(m_node);
					delete_thread_log_all(m_node);

					list_del(&m_node->list);
					kfree(m_node);
					printk(KERN_INFO "delete m pte %lx %lx-%lx pid %d\n", va, va_start, va_end, current->pid);
					break;
				}
			}
			m_list_unlock(m_head);
			break;
		}
	}

	return;
}
EXPORT_SYMBOL_GPL(delete_ds_m_free_pte);

void delete_m_free_pmd(unsigned long va)
{
	struct m_list *m_node;
	struct m_head_list *m_head;

	list_for_each_entry(m_head, &user_head, list){
		if(m_head->pid == current->pid){
			// printk(KERN_INFO "delete m pmd %lx", va);
			m_list_lock(m_head);
			list_for_each_entry(m_node, &m_head->head, list){
				if(m_node->base & PMD_FLAG_MASK && m_node->va <= va && va < m_node->va + OFFSET_SIZE){
					printk(KERN_INFO "delete m pmd %lx %lx pid %d\n", va, m_node->base, current->pid);
					list_del(&m_node->list);
					kfree(m_node);
					break;
				}
			}
			m_list_unlock(m_head);
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(delete_m_free_pmd);

void delete_m_free_pud(unsigned long va)
{
	struct m_list *m_node;
	struct m_head_list *m_head;

	list_for_each_entry(m_head, &user_head, list){
		if(m_head->pid == current->pid){
			// printk(KERN_INFO "delete m pud %lx", va);
			m_list_lock(m_head);
			list_for_each_entry(m_node, &m_head->head, list){
				if(m_node->base & PUD_FLAG_MASK && m_node->va <= va && va < m_node->va + OFFSET_SIZE){
					printk(KERN_INFO "delete m pud %lx %lx pid %d\n", va, m_node->base, current->pid);
					list_del(&m_node->list);
					kfree(m_node);
					break;
				}
			}
			m_list_unlock(m_head);
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(delete_m_free_pud);

void delete_m_free_pgd(unsigned long va)
{
	struct m_list *m_node;
	struct m_head_list *m_head;

	list_for_each_entry(m_head, &user_head, list) {
		if(m_head->pid == current->pid) {
			// printk(KERN_INFO "delete m pgd %lx", va);
			m_list_lock(m_head);
			list_for_each_entry(m_node, &m_head->head, list) {
				if(m_node->base & PGD_FLAG_MASK && m_node->va <= va && va < m_node->va + OFFSET_SIZE) {
					printk(KERN_INFO "delete m pgd %lx %lx pid %d\n", va, PGD_FLAG_MASK, current->pid);
					list_del(&m_node->list);
					kfree(m_node);
					break;
				}
			}
			m_list_unlock(m_head);

			if(list_empty(&m_head->head)){
				list_del(&m_head->list);
				kfree(m_head);
				printk(KERN_INFO "delete m head %d\n", current->pid);
			}
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(delete_m_free_pgd);

static long delete_all(void)
{
	struct m_list *mnode, *itr;
	struct m_head_list *mhead, *tmp; 

	list_for_each_entry_safe(mhead, tmp, &user_head, list) {
		m_list_lock(mhead);
		list_for_each_entry_safe(mnode, itr, &mhead->head, list) {
			delete_ds_all(mnode);
			delete_thread_log_all(mnode);
			list_del(&mnode->list);
			kfree(mnode);
		}
		m_list_unlock(mhead);
		list_del(&mhead->list);
		kfree(mhead);
	}
	printk(KERN_INFO "delete user all\n");

	return 0;
}

SYSCALL_DEFINE0(mycall_ds_m_delete)
{
	return delete_all();
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
	printk(KERN_INFO "child pid %d, current pid %d\n", p->pid, current->pid);
	register_pid(p->pid);
	make_user_pgtable(p);
	make_ds_list_usr_from_pgtable(p);
}
EXPORT_SYMBOL_GPL(register_child);
