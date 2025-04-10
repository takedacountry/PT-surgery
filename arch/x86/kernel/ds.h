#include <linux/types.h>
#include <linux/mm.h>
#include <linux/rwlock.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/slab.h> 
#include <linux/printk.h>
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

extern struct list_head user_head;
extern struct list_head kern_head;
extern rwlock_t user_head_lock;

static inline void m_list_read_lock(struct m_head_list *mhead)
{
	read_lock(&mhead->m_lock);
}

static inline void m_list_write_lock(struct m_head_list *mhead)
{
	write_lock(&mhead->m_lock);
}

static inline void ds_list_read_lock(struct m_list *mnode)
{
	read_lock(&mnode->ds_lock);
}

static inline void ds_list_write_lock(struct m_list *mnode)
{
	write_lock(&mnode->ds_lock);
}

static inline void member_read_lock(struct m_list *mnode)
{
	read_lock(&mnode->member_lock);
}

static inline void member_write_lock(struct m_list *mnode)
{
	write_lock(&mnode->member_lock);
}

static inline void broken_list_read_lock(struct m_list *mnode)
{
	read_lock(&mnode->broken_lock);
}

static inline void broken_list_write_lock(struct m_list *mnode)
{
	write_lock(&mnode->broken_lock);
}

static inline void ref_count_lock(struct m_list *mnode)
{
	spin_lock(&mnode->ref_lock);
}

static inline void m_list_read_unlock(struct m_head_list *mhead)
{
	read_unlock(&mhead->m_lock);
}

static inline void m_list_write_unlock(struct m_head_list *mhead)
{
	write_unlock(&mhead->m_lock);
}

static inline void ds_list_read_unlock(struct m_list *mnode)
{
	read_unlock(&mnode->ds_lock);
}

static inline void ds_list_write_unlock(struct m_list *mnode)
{
	write_unlock(&mnode->ds_lock);
}

static inline void member_read_unlock(struct m_list *mnode)
{
	read_unlock(&mnode->member_lock);
}

static inline void member_write_unlock(struct m_list *mnode)
{
	write_unlock(&mnode->member_lock);
}

static inline void broken_list_read_unlock(struct m_list *mnode)
{
	read_unlock(&mnode->broken_lock);
}

static inline void broken_list_write_unlock(struct m_list *mnode)
{
	write_unlock(&mnode->broken_lock);
}

static inline void ref_count_unlock(struct m_list *mnode)
{
	spin_unlock(&mnode->ref_lock);
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


static inline long make_ds_va(unsigned long a, unsigned long b, unsigned long c, unsigned long d)
{
	unsigned long va = a << 27 | b << 18 | c << 9 | d;
	return va;	
}

static inline long make_ds_offset(long base, unsigned long pte_value)
{
	long offset = base - pte_value;
	return offset;
}

static inline struct ds_list *make_ds_node(unsigned long base, unsigned long limit, long offset, unsigned long flag)
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

static inline struct m_list *make_m_node(unsigned long va, unsigned long base)
{
	struct m_list *list = kmalloc(sizeof(struct m_list), GFP_KERNEL);
	if(!list)
		return NULL;

	list->va = va & PAGE_MASK;
	list->base = base;
	list->dup_pte = NULL;
	list->ref_count = 0;
	spin_lock_init(&list->ref_lock);
	rwlock_init(&list->member_lock);
	rwlock_init(&list->ds_lock);
	rwlock_init(&list->broken_lock);
	INIT_LIST_HEAD(&list->ds_head);
	INIT_LIST_HEAD(&list->broken_head);
	return list;
}

static inline struct broken_pte_list *make_broken_pte_node(unsigned int offset) 
{
	struct broken_pte_list *list = kmalloc(sizeof(struct broken_pte_list), GFP_KERNEL);
	if(!list)
		return NULL;

	list->offset = offset;
	return list;
}

static inline bool is_ds_node_merge(struct ds_list *prev, struct ds_list *next)
{
	if(prev->limit == next->base && prev->offset == next->offset && prev->flag == next->flag) {
		return true;
	}
	else {
		return false;
	}
}

static inline void ds_node_merge(struct ds_list *prev, struct ds_list *next)
{
	if(is_ds_node_merge(prev, next)) {
		prev->limit = next->limit;
		list_del(&next->list);
		kfree(next);
	}
}

// static inline bool is_add_m_node_usr(unsigned long base, struct m_head_list *m_head)
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

static inline int add_first_m_node(unsigned long va, unsigned long base, struct m_head_list *m_head)
{
	struct m_list *mnode;

	if((mnode = make_m_node(va, base)) == NULL)
		return -ENOMEM;

	if(!list_empty(&m_head->head)) 
		return -1;
	
	list_add(&mnode->list, &m_head->head);

	return 0;
}
	
// static inline int add_m_node_usr(unsigned long va, unsigned long base, struct m_head_list *m_head)
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

static inline int add_m_node(unsigned long va, unsigned long base, struct m_list *m_node)
{
	struct m_list *itr;

	if((itr = make_m_node(va, base)) == NULL)
		return -ENOMEM;

	list_add(&itr->list, &m_node->list);
	return 0;
}

static inline int add_tail_m_node(unsigned long va, unsigned long base, struct m_list *m_node)
{
	struct m_list *itr;

	if((itr = make_m_node(va, base)) == NULL)
		return -ENOMEM;

	list_add_tail(&itr->list, &m_node->list);
	return 0;
}

static inline int add_broken_pte_node(unsigned int offset, struct m_list *mnode)
{
	struct broken_pte_list *itr;

	if((itr = make_broken_pte_node(offset)) == NULL)
		return -ENOMEM;

	list_add_tail(&itr->list, &mnode->broken_head);
	return 0;
}

// static inline bool is_add_m_node_ker(unsigned long base)
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

// static inline int add_m_node_ker(unsigned long va, unsigned long base)
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

static inline void modify_m_va(struct m_list *itr, unsigned long va)
{
	itr->va = va & PAGE_MASK;
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

static inline void update_dup_pte(pte_t **ptep, struct ds_list *itr, unsigned long start, unsigned long end)
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

static inline void update_dup_pgtable(unsigned long va_start, pte_t *pte, struct m_list *mnode)
{
	struct ds_list *itr;
	unsigned long va_end;
	
	va_end = va_start | PT_PGTABLE_MASK;

	ds_list_read_lock(mnode);
	list_for_each_entry(itr, &mnode->ds_head, list) {
		printk(KERN_INFO "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
		update_dup_pte(&pte, itr, va_start, va_end);
		va_start = itr->limit;
	}
	ds_list_read_unlock(mnode);
	return;
}

// static inline int update_dup_pgtable_ker(unsigned long va_start, pte_t *pte, struct file *file, loff_t *pos)
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
			
// 			update_dup_pte(&pte, itr, va_start, va_end);
// 			va_start = itr->limit;
// 			flag = 1;
// 		}
// 	}
// 	return flag;
// }

static inline void delete_ds_all(struct m_list *m_node)
{
	struct ds_list *itr, *tmp;

	ds_list_write_lock(m_node);
	list_for_each_entry_safe(itr, tmp, &m_node->ds_head, list) {
		list_del(&itr->list);
		kfree(itr);
	}
	ds_list_write_unlock(m_node);
}

static inline void delete_broken_pte_all(struct m_list *mnode)
{
	struct broken_pte_list *itr, *tmp;

	broken_list_write_lock(mnode);
	list_for_each_entry_safe(itr, tmp, &mnode->broken_head, list) {
		list_del(&itr->list);
		kfree(itr);
	}
	broken_list_write_unlock(mnode);
}
