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
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/page_types.h>
#include <asm/pgtable_types.h>
#include <asm/paravirt.h>
#include <asm-generic/pgalloc.h>
#include <asm-generic/barrier.h>
#include <asm-generic/memory_model.h>

#define USER_MAX 		0x100
#define MAX 			0x200
#define USER_MAX_ADDRESS_SHIFT	47
#define USER_MAX_ADDRESS  	(_AT(long, 1) << USER_MAX_ADDRESS_SHIFT)
#define MAX_NUM_SHIFT		36
#define MAX_NUM		  	(_AT(long, 1) << MAX_NUM_SHIFT)
#define PT_PGTABLE_SHIFT 	9
#define PT_PGTABLE_SIZE		(_AT(long, 1) << PT_PGTABLE_SHIFT)
#define PT_PGTABLE_MASK		(PT_PGTABLE_SIZE - 1)
#define PT_PGTABLE_MASK_NOT	(~PT_PGTABLE_MASK)
#define PGD_FLAG_SHIFT		0
#define PGD_FLAG_MASK		(_AT(long, 1) << PGD_FLAG_SHIFT)
#define P4D_FLAG_SHIFT		1
#define P4D_FLAG_MASK		(_AT(long, 1) << P4D_FLAG_SHIFT)
#define PUD_FLAG_SHIFT		2
#define PUD_FLAG_MASK		(_AT(long, 1) << PUD_FLAG_SHIFT)
#define PMD_FLAG_SHIFT		3
#define PMD_FLAG_MASK		(_AT(long, 1) << PMD_FLAG_SHIFT)
#define PTE_FLAG_SHIFT		4
#define PTE_FLAG_MASK		(_AT(long, 1) << PTE_FLAG_SHIFT)
#define OFFSET_SHIFT 		12
#define OFFSET_SIZE		(_AT(long, 1) << OFFSET_SHIFT)
#define OFFSET_MASK		(OFFSET_SIZE - 1)
#define OFFSET_MASK_NOT		(~OFFSET_MASK)
#define RW_BIT			1
#define FLAG_RW			(_AT(long, 1) << RW_BIT)
#define FLAG_RW_NOT		(~FLAG_RW)

#define SAME_ADDR_SHIFT 	16
#define SAME_ADDR_MASK 		(_AT(long, 1) << SAME_ADDR_SHIFT)
#define SAME_ADDR_MASK_NOT 	(~(SAME_ADDR_MASK))
#define HIT_FLAG_SHIFT 		0
#define CONTI_FLAG_SHIFT 	1
#define SAME_FLAG_SHIFT 	2
#define HIT_FLAG_MASK 		(_AT(int, 1) << HIT_FLAG_SHIFT)
#define CONTI_FLAG_MASK 	(_AT(int, 1) << CONTI_FLAG_SHIFT)
#define SAME_FLAG_MASK		(_AT(int, 1) << SAME_FLAG_SHIFT)
#define HIT_FLAG_MASK_NOT 	(~(HIT_FLAG_MASK))
#define CONTI_FLAG_MASK_NOT 	(~(CONTI_FLAG_MASK))
#define SAME_FLAG_MASK_NOT 	(~(SAME_FLAG_MASK))

unsigned long vaddr;
// extern struct ds_list_head *ds_list;
// extern struct m_list_head *m_list;
// struct task_struct *target_task;

static long register_pid(pid_t pid);


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


static int get_pfn_scan_pte(pmd_t *pmdp, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
{
  	pte_t *ptep = pte_offset_index(pmdp, pte);
	*(ptepp) = ptep;

  	if(pte_none(*ptep) || !pte_present(*ptep)) {
    		// printk(KERN_INFO "pte %lu is not present.\n", pte);
    		return 4;
  	}
  	return 1;
}

static int get_pfn_scan_pmd(pud_t *pudp, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
{
  	pmd_t *pmdp = pmd_offset_index(pudp, pmd);

  	if(pmd_none(*pmdp) || !pmd_present(*pmdp) || pmd_large(*pmdp)){
    		// printk(KERN_INFO "pmd %lu is not present.\n", pmd);
    		return 5;
  	}
  // 	if(pmd_large(*pmdp)){
  //   		pte_value = pmd_pfn(*pmdp);
		// pte_flag = pmd_flags(*pmdp);
  //   		return 2;
  // 	}
  	return get_pfn_scan_pte(pmdp, pgd, pud, pmd, pte, ptepp);
}

static int get_pfn_scan_pud(p4d_t *p4dp, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
{
  	pud_t *pudp = pud_offset_index(p4dp, pud);
	  
  	if(pud_none(*pudp) || !pud_present(*pudp) || pud_large(*pudp)){
	    	// printk(KERN_INFO "pud %lu is not present", pud);
	    	return 6;
  	}
  // 	if(pud_large(*pudp)){
  //   		pte_value = pud_pfn(*pudp);
		// pte_flag = pud_flags(*pudp);
  //   		return 3;
  // 	}
  	return get_pfn_scan_pmd(pudp, pgd, pud, pmd, pte, ptepp);  
}

static int get_pfn_scan_p4d(pgd_t *pgdp, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
{
  	p4d_t *p4dp = p4d_offset_index(pgdp, pgd);
	
	if(p4d_none(*p4dp) || !p4d_present(*p4dp)){
	    	// printk(KERN_INFO "p4d %lu is not present", pgd);
    		return 7;
  	}
  	return get_pfn_scan_pud(p4dp, pgd, pud, pmd, pte, ptepp);
}

static int get_pfn_scan_pgd(struct mm_struct *mm, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
{
  	pgd_t *pgdp = pgd_offset_index(mm, pgd);

  	if(pgd_none(*pgdp) || !pgd_present(*pgdp)){
	    	// printk(KERN_INFO "pgd %lu is not present.\n", pgd);
    		return 7;
  	}
  	return get_pfn_scan_p4d(pgdp, pgd, pud, pmd, pte, ptepp);
}

static int search_pgtable_get_pfn(unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
{
  	struct mm_struct *mm = current->mm;

  	if(pgd<0 || 512<=pgd || pud<0 || 512<=pud || pmd<0 || 512<=pmd || pte<0 || 512<=pte) {
    		printk(KERN_INFO "error: The numbers are not appropriate.\n");
    		return 0;
  	}
  	return get_pfn_scan_pgd(mm, pgd, pud, pmd, pte, ptepp);
}

// struct m_list_head *m_list;
// struct ds_list_head *ds_list;
// EXPORT_SYMBOL(ds_list);
// EXPORT_SYMBOL(m_list);

LIST_HEAD(usr_m_head);
LIST_HEAD(ker_m_head);
LIST_HEAD(usr_ds_head);
LIST_HEAD(ker_ds_head);

void init_ds_list_head(void)
{
//        ds_list = kmalloc(sizeof(struct ds_list_head), GFP_KERNEL);
//        if(!ds_list)
//                return;
//        INIT_LIST_HEAD(&usr_ds_head);
//        INIT_LIST_HEAD(&ker_ds_head);
//        printk(KERN_INFO "init ds list head\n");
}
// EXPORT_SYMBOL_GPL(init_ds_list_head);

void init_m_list_head(void)
{
//        m_list = kmalloc(sizeof(struct m_list_head), GFP_KERNEL);
//        if(!m_list)
//                return;
//        INIT_LIST_HEAD(&usr_m_head);
//        INIT_LIST_HEAD(&ker_m_head);
//        printk(KERN_INFO "init m list head\n");
}
// EXPORT_SYMBOL_GPL(init_m_list_head);

void free_list_head(void)
{
//        kfree(ds_list);
//        kfree(m_list);
}
// EXPORT_SYMBOL_GPL(free_list_head);

SYSCALL_DEFINE0(mycall_ds_init)
{
	init_ds_list_head();
	init_m_list_head();
	return 0;
}

SYSCALL_DEFINE0(mycall_ds_free)
{
	free_list_head();
	return 0;		
}

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
	// list_add_tail(&list->list, &usr_ds_head);
	return list;
}

static struct m_list *make_m_node(unsigned long va, unsigned long num)
{
	struct m_list *list = kmalloc(sizeof(struct m_list), GFP_KERNEL);
	if(!list)
		return NULL;

	list->va = va & PAGE_MASK;
	list->num = num;
	// list_add_tail(&list->list, &usr_m_head);
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
	if(is_ds_node_merge(prev, next)){
		prev->limit = next->limit;
		list_del(&next->list);
		kfree(next);
	}
}

static bool is_add_m_node_usr(unsigned long num, struct m_head_list *m_head)
{
	struct m_list *itr;
	
	list_for_each_entry(itr, &m_head->head, list){
		if(itr->num == num)
			return false;
		if(num < itr->num)
			break;
	}
	return true;
}

static bool is_add_m_node_va_usr(unsigned long va, struct m_head_list *m_head)
{
	struct m_list *itr;

	va &= PAGE_MASK;
	list_for_each_entry(itr, &m_head->head, list){
		if(itr->va == va)
			return false;
	}
	return true;
}

static int add_first_m_node_usr(unsigned long va, unsigned long num, struct m_head_list *m_head)
{
	struct m_list *mnode;

	if((mnode = make_m_node(va, num)) == NULL)
		return -ENOMEM;

	if(list_empty(&m_head->head)){ //no node
		list_add(&mnode->list, &m_head->head);
	}
	return 0;
}
	
static int add_m_node_usr(unsigned long va, unsigned long num, struct m_head_list *m_head)
{
	struct m_list *mnode, *itr;

	if((mnode = make_m_node(va, num)) == NULL)
		return -ENOMEM;

	if(list_empty(&m_head->head)){ //no node
		list_add(&mnode->list, &m_head->head);
	}else{
		list_for_each_entry(itr, &m_head->head, list){
			if(num < itr->num){
				list_add_tail(&mnode->list, &itr->list);
				return 0;
			}
		}
		list_add_tail(&mnode->list, &m_head->head);
	}
	return 0;
}

static int add_m_node(unsigned long va, unsigned long num, struct m_list *m_node)
{
	struct m_list *itr;

	if((itr = make_m_node(va, num)) == NULL)
		return -ENOMEM;

	list_add(&itr->list, &m_node->list);
	return 0;
}

static int add_tail_m_node(unsigned long va, unsigned long num, struct m_list *m_node)
{
	struct m_list *itr;

	if((itr = make_m_node(va, num)) == NULL)
		return -ENOMEM;

	list_add_tail(&itr->list, &m_node->list);
	return 0;
}

static bool is_add_m_node_ker(unsigned long num)
{
	struct m_list *itr;
	
 	list_for_each_entry(itr, &ker_m_head, list){
 		if(itr->num == num)
 			return false;
 		if(num < itr->num)
 			break;
 	}
 	return true;
}

// static bool is_add_ker_m_node_va(unsigned long va)
// {
// 	struct m_list *itr;
	
// 	list_for_each_entry(itr, &ker_m_head, list){
// 		if(itr->va == va)
// 			return false;
// 	}
// 	return true;
// }

static int add_m_node_ker(unsigned long va, unsigned long num)
{
 	struct m_list *mnode, *itr;
	
 	if((mnode = make_m_node(va, num)) == NULL)
 		return -ENOMEM;
 	if(list_empty(&ker_m_head)){ //no node
 		list_add(&mnode->list, &ker_m_head);
 	}else{
 		list_for_each_entry(itr, &ker_m_head, list){
 			if(num < itr->num){
 				list_add_tail(&mnode->list, &itr->list);
 				return 0;
 			}
 		}
 		list_add_tail(&mnode->list, &ker_m_head);
 	}
 	return 0;
}

int make_pgd_m_list(unsigned long pgd_va, pid_t pid)
{
	struct m_head_list *m_head;

	list_for_each_entry(m_head, &usr_m_head, list){
		if(m_head->pid == pid){
			if(is_add_m_node_usr(PGD_FLAG_MASK, m_head)){
				if(add_first_m_node_usr(pgd_va & PAGE_MASK, PGD_FLAG_MASK, m_head) < 0)
					return -ENOMEM;
				printk(KERN_INFO "make m pgd alloc %lx, %lx, %d\n", pgd_va, PGD_FLAG_MASK, pid);
				return 1;
			}
			// printk(KERN_INFO "already same pgd\n");
		}
	}
	// printk(KERN_INFO "pgd no pid: %d\n", pid);
	return 0;
}
EXPORT_SYMBOL_GPL(make_pgd_m_list);

static unsigned long get_pgd_num(unsigned long pgd_va, unsigned long pud_va, struct m_head_list *m_head)
{
	struct m_list *itr;
	unsigned long base;

	list_for_each_entry(itr, &m_head->head, list){
		// if(itr->num & PGD_FLAG_MASK && itr->va <= pgd_va && pgd_va < itr->va + OFFSET_SIZE){
		if(itr->num & PGD_FLAG_MASK){
			base = make_ds_va(((pgd_va - itr->va) / 0x8) & PT_PGTABLE_MASK, 0, 0, PUD_FLAG_MASK & PT_PGTABLE_MASK);
			goto pud_va;
		}
	}
	goto end;
	
pud_va:
	pud_va &= PAGE_MASK;
	while(itr->num <= base){
		if(itr->va == pud_va || itr->num == base){
			// printk(KERN_INFO "already same pud\n");
			goto end;
		}
		if(list_is_last(&itr->list, &m_head->head)){
			if(add_m_node(pud_va, base, itr) < 0){
				goto end;
			}
			goto ret;
		}
		itr = list_next_entry(itr, list);
	}
	if(add_tail_m_node(pud_va, base, itr) < 0)
		goto end;
ret:
	printk(KERN_INFO "make m pud alloc %lx, %lx, %d\n", pud_va, base, current->pid);
	return base;
end:
	// printk(KERN_INFO "no pgd m list va %ld\n",va);
	return MAX_NUM;
}

int make_pud_m_list(unsigned long pgd_va, unsigned long pud_va, pid_t pid)
{
	struct m_head_list *m_head;
	unsigned long num;

	list_for_each_entry(m_head, &usr_m_head, list){
		if(m_head->pid == pid){
			// if(is_add_m_node_va_usr(pud_va, m_head)){
			if((num = get_pgd_num(pgd_va, pud_va, m_head)) >= MAX_NUM)
				return -1;
		
			// if(add_m_node_usr(pud_va, num, m_head, m_node) < 0)
			// 	return -ENOMEM;
			
			// printk(KERN_INFO "make pud %ld, pid %d\n", num | PUD_FLAG_MASK, pid);
			return 1;
		}
	}
	// printk(KERN_INFO "pud no pid: %d\n", pid);
	return 0;
}
EXPORT_SYMBOL_GPL(make_pud_m_list);

static unsigned long get_pud_num(unsigned long pud_va, unsigned long pmd_va, struct m_head_list *m_head)
{
	struct m_list *itr;
	unsigned long base = 0;

	list_for_each_entry(itr, &m_head->head, list){
		if(itr->num & PUD_FLAG_MASK && itr->va <= pud_va && pud_va < itr->va + OFFSET_SIZE){
			base = make_ds_va((itr->num >> 27) & PT_PGTABLE_MASK, ((pud_va - itr->va) / 0x8) & PT_PGTABLE_MASK, 0, PMD_FLAG_MASK & PT_PGTABLE_MASK);
			goto pmd_va;
		}
	}
	goto end;

pmd_va:
	pmd_va &= PAGE_MASK;
	while(itr->num <= base){
		if(itr->va == pmd_va || itr->num == base){
			// printk(KERN_INFO "already same pmd\n");
			goto end;
		}
		if(list_is_last(&itr->list, &m_head->head)){
			if(add_m_node(pmd_va, base, itr) < 0){
				goto end;
			}
			goto ret;
		}
		itr = list_next_entry(itr, list);
	}
	if(add_tail_m_node(pmd_va, base, itr) < 0)
		goto end;
ret:		
	printk(KERN_INFO "make m pmd alloc %lx, %lx, %d\n", pmd_va, base, current->pid);
	return base;
end:
	// printk(KERN_INFO "no pud m list va %ld\n",va);
	return MAX_NUM;
}

int make_pmd_m_list(unsigned long pud_va, unsigned long pmd_va, pid_t pid)
{
	struct m_head_list *m_head;
	unsigned long num;

	list_for_each_entry(m_head, &usr_m_head, list){
		if(m_head->pid == pid){
			// if(is_add_m_node_va_usr(pmd_va, m_head)){
			if((num = get_pud_num(pud_va, pmd_va, m_head)) >= MAX_NUM)
				return -1;
			
			// if(add_m_node_usr(pmd_va, num, m_head) < 0)
			// 	return -ENOMEM;

			// printk(KERN_INFO "make pmd %ld, pid %d\n", num | PMD_FLAG_MASK, pid);
			return 1;
		}
	}
	// printk(KERN_INFO "no m pid: %d\n", pid);
	return 0;
}
EXPORT_SYMBOL_GPL(make_pmd_m_list);

static unsigned long get_pmd_num(unsigned long pmd_va, unsigned long pte_va, struct m_head_list *m_head)
{
	struct m_list *itr;
	unsigned long base = 0;

	list_for_each_entry(itr, &m_head->head, list){
		if(itr->num & PMD_FLAG_MASK && itr->va <= pmd_va && pmd_va < itr->va + OFFSET_SIZE){
			base = make_ds_va((itr->num >> 27) & PT_PGTABLE_MASK, (itr->num >> 18) & PT_PGTABLE_MASK,  ((pmd_va - itr->va) / 0x8) & PT_PGTABLE_MASK, PTE_FLAG_MASK & PT_PGTABLE_MASK);
			goto pte_va;
		}
	}
	goto end;

pte_va:
	pte_va &= PAGE_MASK;
	while(itr->num <= base){
		if(itr->va == pte_va || itr->num == base){
			// printk(KERN_INFO "already same pte\n");
			goto end;
		}
		if(list_is_last(&itr->list, &m_head->head)){
			if(add_m_node(pte_va, base, itr) < 0){
				goto end;
			}
			goto ret;
		}
		itr = list_next_entry(itr, list);
	}
	if(add_tail_m_node(pte_va, base, itr) < 0)
		goto end;
ret:		
	printk(KERN_INFO "make m pte alloc %lx, %lx, %d\n", pte_va, base, current->pid);
	return base;
end:
	// printk(KERN_INFO "no pmd m list va %ld\n",va);
	return MAX_NUM;
}

int make_pte_m_list(unsigned long pmd_va, unsigned long pte_va, pid_t pid)
{
	struct m_head_list *m_head;
	unsigned long num;

	list_for_each_entry(m_head, &usr_m_head, list){
		if(m_head->pid == pid){
			// if(is_add_m_node_va_usr(pte_va, m_head)){
			if((num = get_pmd_num(pmd_va, pte_va, m_head)) >= MAX_NUM)
				return -1;
			
			// if(add_m_node_usr(pte_va, num, m_head) < 0)
			// 	return -ENOMEM;

			// printk(KERN_INFO "make pte %ld, pid %d\n", num | PTE_FLAG_MASK, pid);
			return 1;
			
		}
	}
	// printk(KERN_INFO "no m pid: %d\n", pid);
	return 0;
}
EXPORT_SYMBOL_GPL(make_pte_m_list);

static unsigned long get_pte_num(unsigned long va, struct m_head_list *m_head)
{
	struct m_list *itr;

	list_for_each_entry(itr, &m_head->head, list){
		if(itr->num & PTE_FLAG_MASK && itr->va <= va && va < itr->va + OFFSET_SIZE){
			return make_ds_va((itr->num >> 27) & PT_PGTABLE_MASK, (itr->num >> 18) & PT_PGTABLE_MASK, (itr->num >> 9) & PT_PGTABLE_MASK, ((va - itr->va) / 0x8) & PT_PGTABLE_MASK);
		}
	}
	// printk(KERN_INFO "no pte m list va %ld\n",va);
	return MAX_NUM;
}

static int modify_ds_flag(struct ds_list *ds_node, struct ds_list *new, struct ds_head_list *ds_head)
{
	struct ds_list *next, *prev;

	if(ds_node->base == new->base && ds_node->limit == new->limit){
		ds_node->flag = new->flag;
		if(list_is_first(&ds_node->list, &ds_head->head)){
			next = list_next_entry(ds_node, list);
			ds_node_merge(ds_node, next);
		}
		else if(list_is_last(&ds_node->list, &ds_head->head)){
			prev = list_prev_entry(ds_node, list);
			ds_node_merge(prev, ds_node);
		}
		else{
			next = list_next_entry(ds_node, list);
			prev = list_prev_entry(ds_node, list);
			ds_node_merge(ds_node, next);
			ds_node_merge(prev, ds_node);
		}	
	}else if(ds_node->base == new->base){
		ds_node->base++;
		list_add_tail(&new->list, &ds_node->list);
		if(!list_is_first(&new->list, &ds_head->head)){
			prev = list_prev_entry(new, list);
			ds_node_merge(prev, new);
		}
	}else if(ds_node->limit == new->limit){
		ds_node->limit--;
		list_add(&new->list, &ds_node->list);
		if(!list_is_last(&new->list, &ds_head->head)){
			next = list_next_entry(new, list);
			ds_node_merge(new, next);
		}
	}else{
		if((next = make_ds_node(new->limit, ds_node->limit, ds_node->offset, ds_node->flag)) == NULL)
			return -ENOMEM;
		ds_node->limit = new->base;
		list_add(&new->list, &ds_node->list);
		list_add(&next->list, &new->list);
	}
	return 1;
}


static int modify_ds_offset(struct ds_list *ds_node, struct ds_list *new, struct ds_head_list *ds_head)
{
	struct ds_list *next, *prev;

	if(ds_node->base == new->base && ds_node->limit == new->limit){
		ds_node->offset = new->offset;
		ds_node->flag = new->flag;
		if(list_is_first(&ds_node->list, &ds_head->head)){
			next = list_next_entry(ds_node, list);
			ds_node_merge(ds_node, next);
		}
		else if(list_is_last(&ds_node->list, &ds_head->head)){
			prev = list_prev_entry(ds_node, list);
			ds_node_merge(prev, ds_node);
		}
		else{
			next = list_next_entry(ds_node, list);
			prev = list_prev_entry(ds_node, list);
			ds_node_merge(ds_node, next);
			ds_node_merge(prev, ds_node);
		}	
	}else if(ds_node->base == new->base){
		ds_node->base++;
		list_add_tail(&new->list, &ds_node->list);
		if(!list_is_first(&new->list, &ds_head->head)){
			prev = list_prev_entry(new, list);
			ds_node_merge(prev, new);
		}
	}else if(ds_node->limit == new->limit){
		ds_node->limit--;
		list_add(&new->list, &ds_node->list);
		if(!list_is_last(&new->list, &ds_head->head)){
			next = list_next_entry(new, list);
			ds_node_merge(new, next);
		}
	}else{
		if((next = make_ds_node(new->limit, ds_node->limit, ds_node->offset, ds_node->flag)) == NULL)
			return -ENOMEM;
		ds_node->limit = new->base;
		list_add(&new->list, &ds_node->list);
		list_add(&next->list, &new->list);
	}
	return 1;
}

static bool is_ds_write(struct ds_list *ds_node)
{
	if(ds_node->flag & FLAG_RW)
		return true;
	else
		return false;
}

int make_ds_list_usr(unsigned long va, pte_t pte, pid_t pid)
{
	struct m_head_list *m_head;
	struct ds_head_list *ds_head;
	struct ds_list *dnode, *next, *prev;
	unsigned long pte_value = pte_pfn(pte);
	unsigned long pte_flag = pte_flags(pte);
	unsigned long base;

	if(pid == 0)
		pid = current->pid;

	list_for_each_entry(m_head, &usr_m_head, list){
		if(m_head->pid == pid){
			// printk(KERN_INFO "make ds before hit m %lx %lx %lx %d\n", pte_value, pte_flag, va, pid);
			if((base = get_pte_num(va, m_head)) >= MAX_NUM){
				// printk(KERN_INFO "make ds error va %lx pfn %lx flag %lx\n", va, pte_value, pte_flag);
				return -1;
			}
			// printk(KERN_INFO "make ds hit m %lx %lx %lx %lx %d\n", base, pte_value, pte_flag, va, pid);
			break;
		}
	}

	list_for_each_entry(ds_head, &usr_ds_head, list){
		if(ds_head->pid == pid){		
			if((dnode = make_ds_node(base, base+1, make_ds_offset(base, pte_value), pte_flag)) == NULL)
				return -ENOMEM;
			
			// incert dnode
			if(list_empty(&ds_head->head)){ //no node
				list_add(&dnode->list, &ds_head->head);
				goto end;
			}else{
				list_for_each_entry_reverse(prev, &ds_head->head, list){
					if(prev->base <= dnode->base && dnode->limit <= prev->limit){
						// printk(KERN_INFO "make ds hit ds %lx %lx %lx %lx %d\n", base, pte_value, pte_flag, va, pid);
						if(dnode->offset != prev->offset){
							// modify pte offset
							modify_ds_offset(prev, dnode, ds_head);
							printk(KERN_INFO "modify ds offset %lx %lx %lx %lx\n", base, pte_value, pte_flag, va);
						}
						else if(dnode->flag != prev->flag){
							// modify pte flag 
							if(!is_ds_write(prev) && is_ds_write(dnode)){
								// ds_mkwrite
								// printk(KERN_INFO "make write %lx %lx-%lx", base, prev->base, prev->limit);
								modify_ds_flag(prev, dnode, ds_head);
								printk(KERN_INFO "make write %lx %lx %lx %lx", base, pte_value, pte_flag, va);
							}
							else if(is_ds_write(prev) && !is_ds_write(dnode)){
								// ds_wrprotect
								// printk(KERN_INFO "make wrprotect %lx %lx-%lx", base, prev->base, prev->limit);
								modify_ds_flag(prev, dnode, ds_head);
								printk(KERN_INFO "make wrprotect %lx %lx %lx %lx", base, pte_value, pte_flag, va);
	
							}else{
								printk(KERN_INFO "not modify ds flag %lx  %lx->%lx %lx", base, prev->flag, pte_flag, va);
							}
						}
						goto end;
					}
					else if(dnode->base >= prev->limit){
						printk(KERN_INFO "make ds hit ds %lx %lx %lx %lx %d\n", base, pte_value, pte_flag, va, pid);
						list_add(&dnode->list, &prev->list);
						if(list_is_last(&dnode->list, &ds_head->head)){
							ds_node_merge(prev, dnode);
							goto end;
						}
						next = list_next_entry(dnode, list);
						ds_node_merge(dnode, next);
						ds_node_merge(prev, dnode);
						goto end;
					}
				}
				list_add(&dnode->list, &ds_head->head);
				next = list_next_entry(dnode, list);
				ds_node_merge(dnode, next);
				goto end;
			}
		}
	}
	// printk(KERN_INFO "no ds pid: %d\n", pid);
	return 0;
end:
	// printk(KERN_INFO "make ds base %ld\n", base);
	return 1;
}
EXPORT_SYMBOL_GPL(make_ds_list_usr);


static int make_list_usr_from_pgtable(unsigned long addr, pte_t *ptep)
{
	struct m_head_list *m_head;
	struct ds_head_list *ds_head;
	struct ds_list *dnode, *next, *prev;
	unsigned long pte_value = pte_pfn(*ptep);
	unsigned long pte_flag = pte_flags(*ptep);

	list_for_each_entry(m_head, &usr_m_head, list){
		if(m_head->pid == current->pid){
			if(is_add_m_node_usr(addr & PT_PGTABLE_MASK_NOT, m_head)){
				if(add_m_node_usr((unsigned long)ptep, addr & PT_PGTABLE_MASK_NOT, m_head) < 0)
					return -ENOMEM;
			}
		}
	}


	list_for_each_entry(ds_head, &usr_ds_head, list){
		if(ds_head->pid == current->pid){
			if((dnode = make_ds_node(addr, addr+1, make_ds_offset(addr, pte_value), pte_flag)) == NULL)
				return -ENOMEM;

			// incert dnode
			if(list_empty(&ds_head->head)){ //no node
				list_add(&dnode->list, &ds_head->head);
			}else{
				list_for_each_entry(next, &ds_head->head, list){
					if(dnode->limit <= next->base){
						list_add_tail(&dnode->list, &next->list);
						if(list_is_first(&dnode->list, &ds_head->head)){
							ds_node_merge(dnode, next);
							goto end;
						}
						prev = list_prev_entry(dnode, list);
						break;
					}
					if(list_is_last(&next->list, &ds_head->head)){
						list_add_tail(&dnode->list, &ds_head->head);
						prev = list_prev_entry(dnode, list);
						ds_node_merge(prev, dnode);
						goto end;
					}
				}
				ds_node_merge(dnode, next);
				ds_node_merge(prev, dnode);
				goto end;
			}
		}
	}
end:
	printk(KERN_INFO "make ds base: %ld\n", addr);
	return 0;
	
}

static int make_list_ker_from_pgtable(unsigned long addr, pte_t *ptep)
{
	struct ds_list *dnode, *next, *prev;
	unsigned long pte_value = pte_pfn(*ptep);
	unsigned long pte_flag = pte_flags(*ptep);

	if((dnode = make_ds_node(addr, addr+1, make_ds_offset(addr, pte_value), pte_flag)) == NULL)
		return -ENOMEM;

	if(is_add_m_node_ker(addr & PT_PGTABLE_MASK_NOT))
		if(add_m_node_ker((unsigned long)ptep, addr & PT_PGTABLE_MASK_NOT) < 0)
			return -ENOMEM;
		
	// incert dnode
	if(list_empty(&ker_ds_head)){ //no node
		list_add(&dnode->list, &ker_ds_head);
	}else{
		list_for_each_entry(next, &ker_ds_head, list){
			if(dnode->limit <= next->base){
				list_add_tail(&dnode->list, &next->list);
				if(list_is_first(&dnode->list, &ker_ds_head)){
					ds_node_merge(dnode, next);
					goto end;
				}
				prev = list_prev_entry(dnode, list);
				break;
			}
			if(list_is_last(&next->list, &ker_ds_head)){
				list_add_tail(&dnode->list, &ker_ds_head);
				prev = list_prev_entry(dnode, list);
				ds_node_merge(prev, dnode);
				goto end;
			}
		}
		ds_node_merge(dnode, next);
		ds_node_merge(prev, dnode);
	}
end:
	printk(KERN_INFO "make ds base: %ld\n", addr);
	return 0;
	
}

int make_list_from_pgtable(unsigned long address, pte_t *ptep)
{
	printk(KERN_INFO "va:%ld pteva:%ld",address, (unsigned long)ptep);
	if(address < USER_MAX_ADDRESS)
		return make_list_usr_from_pgtable(address, ptep);
	// return make_list_ker_from_pgtable(address, ptep);
	return 0;
}
EXPORT_SYMBOL_GPL(make_list_from_pgtable);


// static long make_ds_user(void)
// {
// 	pte_t *ptep;
// 	unsigned long pte_value;
// 	unsigned long pte_flag;
	
// 	int num;
// 	int count;
// 	int hit_flag = 0;
		
// 	unsigned long base;
// 	unsigned long limit;
// 	long offset;
// 	unsigned long flag;
	
// 	unsigned long pte_value_pre;
// 	unsigned long pte_flag_pre;

// 	unsigned long pte_num;
// 	unsigned long pte_num_pre = 0;
	
// 	for(unsigned long a=0; a<USER_MAX; a++){
//         	for(unsigned long b=0; b<MAX; b++){
//             		for(unsigned long c=0; c<MAX; c++){
//                 		for(unsigned long d=0; d<MAX; d++){
//                     			if((num = search_pgtable_get_pfn(a, b, c, d, &ptep)) > 0 && num < 4){ //pte hit
// 						pte_value = pte_pfn(*ptep);
// 						pte_flag = pte_flags(*ptep);
						
// 						pte_num = make_ds_va(a, b, c, 0); // first entry num
// 						// if(pte_num_pre == 0)
// 						// 	vaddr = (unsigned long)ptep;
						
// 						if(pte_num_pre != pte_num){
// 							if(make_m_node((unsigned long)ptep, pte_num) == NULL)
// 								goto end;
// 							pte_num_pre = pte_num;
// 						}
						
// 						if(hit_flag == 0){ // miss, first hit
// 							// make ds members
// 							base = make_ds_va(a, b, c, d);
// 							offset = make_ds_offset(base, pte_value);
// 							flag = pte_flag;
							
// 							hit_flag = 1;
// 							pte_value_pre = pte_value; // pte_value_pre initialize
// 							pte_flag_pre = pte_flag; // pte_flag_pre initialize
// 						}else if(pte_value == pte_value_pre + 1 && pte_flag == pte_flag_pre){ // continuous address hit
// 							pte_value_pre = pte_value;
// 						}else{ // last hit, first nit
// 							// add ds_list list
// 							limit = make_ds_va(a, b, c, d);
// 							if(make_ds_node(base, limit, offset, flag) == NULL)
// 								goto end;

// 							// make ds members
// 							base = limit;
// 							offset = make_ds_offset(base, pte_value);
// 							flag = pte_flag;
							
// 							pte_value_pre = pte_value;
// 							pte_flag_pre = pte_flag;
// 						}
//                         			count = num;
//                     			}else if(num == 0){ // error
// 						goto end;
// 					}else{ // pte miss
// 						if(hit_flag > 0){ // last hit, miss
// 							// add ds_list list
// 							limit = make_ds_va(a, b, c, d);
// 							if(make_ds_node(base, limit, offset, flag) == NULL)
// 								goto end;
// 							hit_flag = 0;
// 						}
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

static int get_ptep(pmd_t *pmdp, pid_t pid, unsigned long pte, pte_t **ptepp, int *flag)
{
  	pte_t *ptep = pte_offset_index(pmdp, pte);
	*(ptepp) = ptep;

  	if(pte_none(*ptep) || !pte_present(*ptep)) {
    		// printk(KERN_INFO "pte %lu is not present.\n", pte);
    		return 0;
  	}

	if(*(flag) == 0){
		// if(make_pte_m_list((unsigned long)pmdp, (unsigned long)ptep) < 0){
		// 	printk(KERN_INFO "pte m list failure at get_ptep\n");
		// }
		if(make_pte_m_list((unsigned long)pmdp, (unsigned long)ptep, pid) == 1){
			*(flag) = 1;
		}
	}
		
  	return 1;
}

static int get_pmdp(pud_t *pudp, pid_t pid, unsigned long pmd, pmd_t **pmdpp, int *flag)
{
  	pmd_t *pmdp = pmd_offset_index(pudp, pmd);
	*(pmdpp) = pmdp;

  	if(pmd_none(*pmdp) || !pmd_present(*pmdp) || pmd_large(*pmdp)){
    		// printk(KERN_INFO "pmd %lu is not present.\n", pmd);
    		return 0;
  	}

	if(*(flag) == 0){
		// if(make_pmd_m_list((unsigned long)pudp, (unsigned long)pmdp) < 0){
		// 	printk(KERN_INFO "pmd m list failure at get_pmdp\n");
		// }
		if(make_pmd_m_list((unsigned long)pudp, (unsigned long)pmdp, pid) == 1){
			*(flag) = 1;
		}
	}
	
  // 	if(pmd_large(*pmdp)){
  //   		pte_value = pmd_pfn(*pmdp);
		// pte_flag = pmd_flags(*pmdp);
  //   		return 2;
  // 	}
  	return 1;
}

static int get_pudp(p4d_t *p4dp, pid_t pid, unsigned long pud, pud_t **pudpp, int *flag)
{
  	pud_t *pudp = pud_offset_index(p4dp, pud);
	*(pudpp) = pudp;
	
  	if(pud_none(*pudp) || !pud_present(*pudp) || pud_large(*pudp)){
	    	// printk(KERN_INFO "pud %lu is not present", pud);
	    	return 0;
  	}

	if(*(flag) == 0){
		// if(make_pud_m_list((unsigned long)p4dp, (unsigned long)pudp) < 0){
		// 	printk(KERN_INFO "pud m list failure at get_pudp\n");
		// }
		if(make_pud_m_list((unsigned long)p4dp, (unsigned long)pudp, pid) == 1){
			*(flag) = 1;
		}
	}
	
  // 	if(pud_large(*pudp)){
  //   		pte_value = pud_pfn(*pudp);
		// pte_flag = pud_flags(*pudp);
  //   		return 3;
  // 	}
  	return 1;  
}

static int get_p4dp(pgd_t *pgdp, unsigned long p4d, p4d_t **p4dpp)
{
  	p4d_t *p4dp = p4d_offset_index(pgdp, p4d);
	*(p4dpp) = p4dp;
	
	if(p4d_none(*p4dp) || !p4d_present(*p4dp)){
	    	// printk(KERN_INFO "p4d %lu is not present", pgd);
    		return 0;
  	}
	return 1;
}

static int get_pgdp(struct mm_struct *mm, pid_t pid, unsigned long pgd, p4d_t **p4dpp, int *flag)
{
  	pgd_t *pgdp = pgd_offset_index(mm, pgd);

  	if(pgd_none(*pgdp) || !pgd_present(*pgdp)){
	    	// printk(KERN_INFO "pgd %lu is not present.\n", pgd);
    		return 0;
  	}

	if(get_p4dp(pgdp, pgd, p4dpp) == 0){
		return 0;
	}

	if(*(flag) == 0){
		// if(make_pgd_m_list((unsigned long)pgdp) < 0){
		// 	printk(KERN_INFO "pgd m list failure at get_pgdp\n");
		// }
		if(make_pgd_m_list((unsigned long)pgdp, pid) == 1){
			*(flag) = 1;
		}
	}
	
  	return 1;
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
	int flag_pgd=0;
	int flag_pud=0;
	int flag_pmd=0;
	int flag_pte=0;
	
	// unsigned long pte_num;

	for(unsigned long pgd=0; pgd<USER_MAX; pgd++){
		if(get_pgdp(p->mm, pid, pgd, &p4dp, &flag_pgd) > 0){
			
	        	for(unsigned long pud=0; pud<MAX; pud++){
				if(get_pudp(p4dp, pid, pud, &pudp, &flag_pud) > 0){
					
		            		for(unsigned long pmd=0; pmd<MAX; pmd++){
						if(get_pmdp(pudp, pid, pmd, &pmdp, &flag_pmd) > 0){

							for(unsigned long pte=0; pte<MAX; pte++){
			                    			if(get_ptep(pmdp, pid, pte, &ptep, &flag_pte) > 0){
									
									if(flag == 0){
										vaddr = (unsigned long)ptep;
										flag = 1;
									}
									// make_ds from num
									// pte_num = make_ds_va(pgd, pud, pmd, pte);
									// if(make_usr_ds_list(pte_num, ptep) < 0){
									// 	goto end;

									// make_ds from ptep
									if(make_ds_list_usr((unsigned long)ptep, *ptep, pid) < 0){
										printk(KERN_INFO "pte ds list failure at from_pgtable\n");
										// goto end;
									}
			                    			}
			                		}
							flag_pte = 0;
						}
		            		}
					flag_pmd = 0;
				}
	        	}
			flag_pud = 0;
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

static long make_usr_ds(void)
{
	pte_t *ptep;
	
	int num;
	int count;
	// int flag=0;
		
	unsigned long pte_num;
	for(unsigned long a=0; a<USER_MAX; a++){
        	for(unsigned long b=0; b<MAX; b++){
            		for(unsigned long c=0; c<MAX; c++){
                		for(unsigned long d=0; d<MAX; d++){
                    			if((num = search_pgtable_get_pfn(a, b, c, d, &ptep)) > 0 && num < 4){ //pte hit
						pte_num = make_ds_va(a, b, c, d);
						// if(flag == 0){
						// 	vaddr = (unsigned long)ptep;
						// 	flag = 1;
						// }
						if(make_list_usr_from_pgtable(pte_num, ptep) < 0)
							goto end;
						
                        			count = num;
                    			}else if(num == 0){ // error
						goto end;
					}else{
                        			count = num - 3;
                    			}
                    			num = 0;
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
        	if(--count > 0)
          		break;
        	count = 0;
    	}
end:
	
	return 0;
}

// static long make_ds_kernel(void)
// {
// 	pte_t *ptep;
// 	unsigned long pte_value;
// 	unsigned long pte_flag;
	
// 	int num;
// 	int count;
// 	int ds_flag = 0;
	
// 	unsigned long base;
// 	unsigned long limit;
// 	long offset;
// 	unsigned long flag;
	
// 	unsigned long pte_value_pre;
// 	unsigned long pte_flag_pre;

// 	unsigned long pte_num;
// 	unsigned long pte_num_pre = 0;
	
// 	for(unsigned long a=USER_MAX; a<MAX; a++){
//         	for(unsigned long b=0; b<MAX; b++){
//             		for(unsigned long c=0; c<MAX; c++){
//                 		for(unsigned long d=0; d<MAX; d++){
//                     			if((num = search_pgtable_get_pfn(a, b, c, d, &ptep)) > 0 && num < 4){ //pte hit
// 						pte_value = pte_pfn(*ptep);
// 						pte_flag = pte_flags(*ptep);
						
// 						pte_num = make_ds_va(a, b, c, 0); // first entry num
// 						if(pte_num_pre == 0)
// 							vaddr = (unsigned long)ptep;
						
// 						if(pte_num_pre != pte_num){
// 							if(make_m_node((unsigned long)ptep, pte_num) == NULL)
// 								goto end;
// 							pte_num_pre = pte_num;
// 						}

// 						if(!(ds_flag & HIT_FLAG_MASK)){
// 							// make ds members
// 							base = make_ds_va(a, b, c, d);
// 							offset = make_ds_offset(base, pte_value);
// 							flag = pte_flag;
							
// 							ds_flag |= HIT_FLAG_MASK;
// 							pte_value_pre = pte_value; // pte_value_pre initialize
// 							pte_flag_pre = pte_flag; // pte_flag_pre initialize
// 						}else if(pte_value == pte_value_pre + 1 && pte_flag == pte_flag_pre && !(ds_flag & SAME_FLAG_MASK)){ // continuous address hit
// 							pte_value_pre = pte_value;
// 							if(!(ds_flag & CONTI_FLAG_MASK))
// 								ds_flag |= CONTI_FLAG_MASK;
// 						}else if(pte_value == pte_value_pre && pte_flag == pte_flag_pre && !(ds_flag & CONTI_FLAG_MASK)){ // same address hit
// 							if(!(ds_flag & SAME_FLAG_MASK))
// 								ds_flag |= SAME_FLAG_MASK;
// 						}else{ // last hit, first nit
// 							// add ds_list list
// 							limit = make_ds_va(a, b, c, d);
// 							if(ds_flag & SAME_FLAG_MASK){
// 								flag |= SAME_ADDR_MASK;
// 								ds_flag &= SAME_FLAG_MASK_NOT;
// 							}
// 							if(make_ds_node(base, limit, offset, flag) == NULL)
// 								goto end;

// 							// make ds members
// 							base = limit;
// 							offset = make_ds_offset(base, pte_value);
// 							flag = pte_flag;
							
// 							pte_value_pre = pte_value;
// 							pte_flag_pre = pte_flag;
// 							ds_flag &= CONTI_FLAG_MASK_NOT;
// 						}
//                         			count = num;
//                     			}else if(num == 0){ // error
// 						goto end;
// 					}else{ // pte miss
// 						if(ds_flag & HIT_FLAG_MASK){ // last hit, miss
// 							// add ds_list list
// 							limit = make_ds_va(a, b, c, d);
// 							if(ds_flag & SAME_FLAG_MASK){
// 								flag |= SAME_ADDR_MASK;
// 							}
// 							if(make_ds_node(base, limit, offset, flag) == NULL)
// 								goto end;
// 							ds_flag = 0;
// 						}
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

static long make_ker_ds(void)
{
	pte_t *ptep;
	
	int num;
	int count;

	unsigned long pte_num;
	
	for(unsigned long a=USER_MAX; a<MAX; a++){
        	for(unsigned long b=0; b<MAX; b++){
            		for(unsigned long c=0; c<MAX; c++){
                		for(unsigned long d=0; d<MAX; d++){
                    			if((num = search_pgtable_get_pfn(a, b, c, d, &ptep)) > 0 && num < 4){ //pte hit
						pte_num = make_ds_va(a, b, c, d);
						if(make_list_ker_from_pgtable(pte_num, ptep) < 0)
							goto end;
						
                        			count = num;
                    			}else if(num == 0){ // error
						goto end;
					}else{ // pte miss
                        			count = num - 3;
                    			}
                    			num = 0;
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
        	if(--count > 0)
          		break;
        	count = 0;
    	}
end:
	return 0;
}

SYSCALL_DEFINE0(mycall_ds_make)
{
	long ret1, ret2;
	ret1 = make_usr_ds();
	ret2 = make_ker_ds();
	
	if(ret1 == ret2)
		return 0;
   	return -1;
}

SYSCALL_DEFINE0(mycall_ds_make_user)
{
	long ret;
	ktime_t start, end;

	start = ktime_get();
	ret = make_usr_ds();
	end = ktime_get();

	printk(KERN_INFO "make_ds_usr time: %lld\n", ktime_sub(end, start));
	
	return ret;
}

SYSCALL_DEFINE0(mycall_ds_make_kernel)
{
	long ret;
	ktime_t start, end;

	start = ktime_get();
	ret = make_ker_ds();
	end = ktime_get();

	printk(KERN_INFO "make_ds_ker time: %lld\n", ktime_sub(end, start));
	
	return ret;
}


static int print_usr_ds(pid_t pid)
{
	struct ds_list *itr;
	struct ds_head_list *ds_head;
	int count = 0;

	struct file *file;
	char *filename = "./usr_ds_txt";
	int size;
	char *buf;
        loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)){
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if(!buf)
		return -1;
	memset(buf, '\0', 100);

	list_for_each_entry(ds_head, &usr_ds_head, list){
		if(ds_head->pid == pid){
			printk(KERN_INFO "ds pid: %d\n", pid);
			size = sprintf(buf, "ds pid: %d\n", pid);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			list_for_each_entry(itr, &ds_head->head, list){
				// printk(KERN_INFO "%lx %lx %lx %lx   %lx\n", itr->base, itr->limit, itr->offset, itr->flag, __pa((unsigned long)itr));
				size = sprintf(buf, "%lx %lx %lx %lx   %lx\n", itr->base, itr->limit, itr->offset, itr->flag, __pa((unsigned long)itr));
				kernel_write(file, buf, size, &pos);
				vfs_fsync_range(file, 0, size, 1);
				count++;
			}
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

static int print_ker_ds(void)
{
	struct ds_list *itr;
	int count = 0;

	struct file *file;
	char *filename = "./ker_ds_txt";
	int size;
	char *buf;
        loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)){
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if(!buf)
		return -1;
	memset(buf, '\0', 100);
	
	list_for_each_entry(itr, &ker_ds_head, list){
		// printk(KERN_INFO "%lx %lx %lx %lx   %lx\n", itr->base, itr->limit, itr->offset, itr->flag, __pa((unsigned long)itr));
		size = sprintf(buf, "%lx %lx %lx %lx   %lx\n", itr->base, itr->limit, itr->offset, itr->flag, __pa((unsigned long)itr));
		kernel_write(file, buf, size, &pos);
		vfs_fsync_range(file, 0, size, 1);
		count++;
	}
	printk(KERN_INFO "kern ds count %d\n", count);
	size = sprintf(buf, "kern ds count %d\n", count);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);

	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}

SYSCALL_DEFINE1(mycall_ds_search, pid_t, pid)
{
	print_usr_ds(pid);
	// print_ker_ds();
	return 0;
}

static int print_usr_ds2(pid_t pid)
{
	struct ds_list *itr;
	struct ds_head_list *ds_head;
	int count = 0;

	struct file *file;
	char *filename = "./usr_ds_txt2";
	int size;
	char *buf;
        loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)){
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if(!buf)
		return -1;
	memset(buf, '\0', 100);

	list_for_each_entry(ds_head, &usr_ds_head, list){
		if(ds_head->pid == pid){
			printk(KERN_INFO "ds pid: %d\n", pid);
			size = sprintf(buf, "ds pid: %d\n", pid);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			list_for_each_entry(itr, &ds_head->head, list){
				// printk(KERN_INFO "%lx %lx %lx %lx   %lx\n", itr->base, itr->limit, itr->offset, itr->flag, __pa((unsigned long)itr));
				size = sprintf(buf, "%lx %lx %lx %lx   %lx\n", itr->base, itr->limit, itr->offset, itr->flag, __pa((unsigned long)itr));
				kernel_write(file, buf, size, &pos);
				vfs_fsync_range(file, 0, size, 1);
				count++;
			}
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
	if(IS_ERR(file)){
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if(!buf)
		return -1;
	memset(buf, '\0', 100);

	list_for_each_entry(m_head, &usr_m_head, list){
		if(m_head->pid == pid){
			printk(KERN_INFO "m pid: %d\n", pid);
			size = sprintf(buf, "m pid: %d\n", pid);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			list_for_each_entry(itr, &m_head->head, list){
				// printk(KERN_INFO "%lx %lx   %lx\n",itr->va, itr->num, __pa((unsigned long)itr));
				size = sprintf(buf, "%lx %lx   %lx\n",itr->va, itr->num, __pa((unsigned long)itr));
				kernel_write(file, buf, size, &pos);
				vfs_fsync_range(file, 0, size, 1);
				count++;
			}
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

static int print_ker_m(void)
{
	struct m_list *itr;
	int count=0;
	
	struct file *file;
	char *filename = "./ker_m_txt";
	int size;
	char *buf;
        loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)){
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if(!buf)
		return -1;
	memset(buf, '\0', 100);
	
	list_for_each_entry(itr, &ker_m_head, list){
		// printk(KERN_INFO "%lx %lx   %lx\n",itr->va, itr->num, __pa((unsigned long)itr));
		size = sprintf(buf, "%lx %lx   %lx\n",itr->va, itr->num, __pa((unsigned long)itr));
		kernel_write(file, buf, size, &pos);
		vfs_fsync_range(file, 0, size, 1);
		count++;
	}
	printk(KERN_INFO "kern m count %d\n", count);
	size = sprintf(buf, "kern m count %d\n", count);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);

	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}

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
	if(IS_ERR(file)){
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if(!buf)
		return -1;
	memset(buf, '\0', 100);

	list_for_each_entry(m_head, &usr_m_head, list){
		if(m_head->pid == pid){
			printk(KERN_INFO "m pid: %d\n", pid);
			size = sprintf(buf, "m pid: %d\n", pid);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			list_for_each_entry(itr, &m_head->head, list){
				// printk(KERN_INFO "%lx %lx   %lx\n",itr->va, itr->num, __pa((unsigned long)itr));
				size = sprintf(buf, "%lx %lx   %lx\n",itr->va, itr->num, __pa((unsigned long)itr));
				kernel_write(file, buf, size, &pos);
				vfs_fsync_range(file, 0, size, 1);
				count++;
			}
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
	struct ds_head_list *ds_node;
	struct m_head_list *m_node;

	// target_task = current;
	
	list_for_each_entry(ds_node, &usr_ds_head, list){
		if(ds_node->pid == pid){
			goto end;
		}
	}
	list_for_each_entry(m_node, &usr_m_head, list){
		if(m_node->pid == pid){
			goto end;
		}
	}

	ds_node = kmalloc(sizeof(struct ds_head_list), GFP_KERNEL);
	m_node = kmalloc(sizeof(struct m_head_list), GFP_KERNEL);
	if(!ds_node || !m_node)
		return -1;
	ds_node->pid = pid;
	m_node->pid = pid;
	INIT_LIST_HEAD(&ds_node->head);
	INIT_LIST_HEAD(&m_node->head);
	list_add(&ds_node->list, &usr_ds_head);
	list_add(&m_node->list, &usr_m_head);

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

  	if(pmd_none(*pmdp) || !pmd_present(*pmdp) || pmd_large(*pmdp)){
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
	  
  	if(pud_none(*pudp) || !pud_present(*pudp) || pud_large(*pudp)){
	    	// printk(KERN_INFO "pud %lu is not present", pud);
	    	return 5;
  	}
  	return get_pmd_scan_pmd(pudp, pgd, pud, pmd, pmdp);  
}

static int get_pmd_scan_p4d(pgd_t *pgdp, unsigned long pgd, unsigned long pud, unsigned long pmd, pmd_t **pmdp)
{
  	p4d_t *p4dp = p4d_offset_index(pgdp, pgd);
	
	if(p4d_none(*p4dp) || !p4d_present(*p4dp)){
	    	// printk(KERN_INFO "p4d %lu is not present", pgd);
    		return 6;
  	}
  	return get_pmd_scan_pud(p4dp, pgd, pud, pmd, pmdp);
}

static int get_pmd_scan_pgd(struct mm_struct *mm, unsigned long pgd, unsigned long pud, unsigned long pmd, pmd_t **pmdp)
{
  	pgd_t *pgdp = pgd_offset_index(mm, pgd);

  	if(pgd_none(*pgdp) || !pgd_present(*pgdp)){
	    	// printk(KERN_INFO "pgd %lu is not present.\n", pgd);
    		return 6;
  	}
  	return get_pmd_scan_p4d(pgdp, pgd, pud, pmd, pmdp);
}

static int search_pgtable_get_pmd(unsigned long num, pmd_t **pmdp)
{
  	struct mm_struct *mm = current->mm;
	// struct mm_struct *mm = target_task->mm;

	unsigned long pgd = (num >> 27) & PT_PGTABLE_MASK;
	unsigned long pud = (num >> 18) & PT_PGTABLE_MASK;
	unsigned long pmd = (num >> 9) & PT_PGTABLE_MASK;

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
	// unsigned long flag;
	pte_t *pte = *(ptep);

	if(itr->base <= start && end < itr->limit){
		// recover pgtable from one ds
		// if(itr->flag & SAME_ADDR_MASK){
			// flag = itr->flag & SAME_ADDR_MASK_NOT;
		// 	for(count=start; count <= end; count++){
		// 		set_pte(pte, __pte(((pteval_t)(itr->base - itr->offset) << PAGE_SHIFT) | flag));
		// 		pte++;
		// 	}
		// }else{
		
		for(count=start; count <= end; count++){
			set_pte_recover(pte, __pte(((pteval_t)(count - itr->offset) << PAGE_SHIFT) | itr->flag));
			pte++;
		}
		
		// }		
	}else if(itr->base <= start && itr->limit <= end){
		// first recover
		// if(itr->flag & SAME_ADDR_MASK){
		// 	flag = itr->flag & SAME_ADDR_MASK_NOT;
		// 	for(count=start; count < itr->limit; count++){
		// 		set_pte(pte, __pte(((pteval_t)(itr->base - itr->offset) << PAGE_SHIFT) | flag));
		// 		pte++;
		// 	}
		// }else{
		
		for(count=start; count < itr->limit; count++){
			set_pte_recover(pte, __pte(((pteval_t)(count - itr->offset) << PAGE_SHIFT) | itr->flag));
			pte++;
		}
		
		// }
	}else if(start < itr->base && end < itr->limit){
		// last recover
		// if(itr->flag & SAME_ADDR_MASK){
		// 	flag = itr->flag & SAME_ADDR_MASK_NOT;
		// 	for(count=start; count <= end; count++){
		// 		if(count >= itr->base)
		// 			set_pte(pte, __pte(((pteval_t)(itr->base - itr->offset) << PAGE_SHIFT) | flag));
		// 		pte++;
		// 	}
		// }else{
		
		for(count=start; count <= end; count++){
			if(count >= itr->base)
				set_pte_recover(pte, __pte(((pteval_t)(count - itr->offset) << PAGE_SHIFT) | itr->flag));
			pte++;
		}
		
		// }
	}else if(start < itr->base && itr->limit <= end){
		// second ~ last-1 recover
		// if(itr->flag & SAME_ADDR_MASK){
		// 	flag = itr->flag & SAME_ADDR_MASK_NOT;
		// 	for(count=start; count < itr->limit; count++){
		// 		if(count >= itr->base)
		// 			set_pte(pte, __pte(((pteval_t)(itr->base - itr->offset) << PAGE_SHIFT) | flag));
		// 		pte++;
		// 	}
		// }else{
	
		for(count=start; count < itr->limit; count++){
			if(count >= itr->base)
				set_pte_recover(pte, __pte(((pteval_t)(count - itr->offset) << PAGE_SHIFT) | itr->flag));
			pte++;
		}
		
		// }
	}
	*(ptep) = pte;
}

/*
static void print_pte(pmd_t *pmdp)
{
	unsigned long pte;
	for(pte=0; pte<MAX; pte++){
		pte_t *ptep = pte_offset_index(pmdp, pte);

  		if(!pte_none(*ptep) && pte_present(*ptep)) {
			pte_value = pte_pfn(*ptep);
			pte_flag = pte_flags(*ptep);
			printk(KERN_INFO "%ld %lx %lx", pte,  pte_value, pte_flag);
		}
	}
}
*/

static int update_pgtable_usr(unsigned long va_start, pte_t *pte, struct file *file, loff_t *pos)
{
	struct ds_list *itr;
	struct ds_head_list *ds_head;
	unsigned long va_end;
	int flag = 0;
	
	int size;
	char *buf;
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
	memset(buf, '\0', 100);

	va_end = va_start | PT_PGTABLE_MASK;

	// printk(KERN_INFO "%ld-%ld-%ld-0  %lx %lx\n", (va_start >> 27) & PT_PGTABLE_MASK, (va_start >> 18) & PT_PGTABLE_MASK, (va_start >> 9) & PT_PGTABLE_MASK, va_start, va_end);
	size = sprintf(buf, "%ld-%ld-%ld-0  %lx %lx\n", (va_start >> 27) & PT_PGTABLE_MASK, (va_start >> 18) & PT_PGTABLE_MASK, (va_start >> 9) & PT_PGTABLE_MASK, va_start, va_end);
	kernel_write(file, buf, size, pos);
	vfs_fsync_range(file, 0, size, 1);

	list_for_each_entry(ds_head, &usr_ds_head, list){
		if(ds_head->pid == current->pid){
		// if(ds_head->pid == target_task->pid){
			list_for_each_entry(itr, &ds_head->head, list){
				if(itr->limit <= va_start){
					continue; // not hit yet
				}else if(va_end < itr->base){
					break; // already finished
				}else{ // recover pgtable from ds
					// printk(KERN_INFO "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
					size = sprintf(buf, "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
					kernel_write(file, buf, size, pos);
					vfs_fsync_range(file, 0, size, 1);
					
					dup_pte(&pte, itr, va_start, va_end);
					va_start = itr->limit;
					flag = 1;
				}
			}
			break;
		}
	}
	return flag;
}

static int update_pgtable_ker(unsigned long va_start, pte_t *pte, struct file *file, loff_t *pos)
{
	struct ds_list *itr;
	unsigned long va_end;
	int flag = 0;
	
	int size;
	char *buf;
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
	memset(buf, '\0', 100);

	va_end = va_start | PT_PGTABLE_MASK;

	// printk(KERN_INFO "%ld-%ld-%ld-0  %lx %lx\n", (va_start >> 27) & PT_PGTABLE_MASK, (va_start >> 18) & PT_PGTABLE_MASK, (va_start >> 9) & PT_PGTABLE_MASK, va_start, va_end);
	size = sprintf(buf, "%ld-%ld-%ld-0  %lx %lx\n", (va_start >> 27) & PT_PGTABLE_MASK, (va_start >> 18) & PT_PGTABLE_MASK, (va_start >> 9) & PT_PGTABLE_MASK, va_start, va_end);
	kernel_write(file, buf, size, pos);
	vfs_fsync_range(file, 0, size, 1);

	list_for_each_entry(itr, &ker_ds_head, list){
		if(itr->limit <= va_start){
			continue; // not hit yet
		}else if(va_end < itr->base){
			break; // already finished
		}else{ // recover pgtable from ds
			// printk(KERN_INFO "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
			size = sprintf(buf, "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
			kernel_write(file, buf, size, pos);
			vfs_fsync_range(file, 0, size, 1);
			
			dup_pte(&pte, itr, va_start, va_end);
			va_start = itr->limit;
			flag = 1;
		}
	}
	return flag;
}

static int __recover_pgtable(unsigned long va_start, struct m_list *itr, struct file *file, loff_t *pos)
{
	pmd_t *pmdp;
	pte_t *ptep_old;
	pte_t *ptep_new;
	int num;

	int size;
	char *buf;
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
	memset(buf, '\0', 100);
		
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
		
		if(!ptep_new){
			printk(KERN_INFO "out of memory\n");
			goto err;
		}
		
		if(update_pgtable_usr(va_start, ptep_new, file, pos) == 1){
			pmd_reinstall(current->mm, pmdp, ptep_new, itr);

			// printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
			size = sprintf(buf, "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
			kernel_write(file, buf, size, pos);
			vfs_fsync_range(file, 0, size, 1);

			// printk(KERN_INFO "pte after:  %lx", (unsigned long)__pa(ptep_new));
			size = sprintf(buf, "pte after: %lx\n",(unsigned long)__pa(ptep_new));
			kernel_write(file, buf, size, pos);
			vfs_fsync_range(file, 0, size, 1);
		}else{
			// printk(KERN_INFO "not dup pte\n");
			size = sprintf(buf, "not dup pte\n");
			kernel_write(file, buf, size, pos);
			vfs_fsync_range(file, 0, size, 1);
			pte_free(current->mm, virt_to_page(ptep_new));
		}
	}
	else if(num == 2){ // in kernel
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
		
		if(update_pgtable_ker(va_start, ptep_new, file, pos) == 1){
			pmd_reinstall_kernel(pmdp, ptep_new, itr);
			
			// printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
			size = sprintf(buf, "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
			kernel_write(file, buf, size, pos);
			vfs_fsync_range(file, 0, size, 1);

			// printk(KERN_INFO "pte after:  %lx", (unsigned long)__pa(ptep_new));
			size = sprintf(buf, "pte after: %lx\n",(unsigned long)__pa(ptep_new));
			kernel_write(file, buf, size, pos);
			vfs_fsync_range(file, 0, size, 1);
		}else{
			// printk(KERN_INFO "not dup pte\n");
			size = sprintf(buf, "not dup pte\n");
			kernel_write(file, buf, size, pos);
			vfs_fsync_range(file, 0, size, 1);
			pte_free_kernel(&init_mm, ptep_new);
		}
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
	unsigned long num;
	int count = 0;

	struct file *file;
	char *filename = "./write_log_txt";
        loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)){
		printk("pre_file open err=%ld", PTR_ERR(file));
		goto err;
	}
	
	list_for_each_entry(m_head, &usr_m_head, list){
		if(m_head->pid == current->pid){
		// if(m_head->pid == target_task->pid){
			// printk(KERN_INFO "target_task pid: %d\n",target_task->pid);
			for(unsigned long a=0; a<USER_MAX; a++){
		        	for(unsigned long b=0; b<MAX; b++){
		            		for(unsigned long c=0; c<MAX; c++){
						va_start = make_ds_va(a, b, c, 0);
		
						list_for_each_entry(itr, &m_head->head, list){
							num = itr->num & PT_PGTABLE_MASK_NOT;
							if(num == va_start){
								if((count =__recover_pgtable(va_start, itr, file, &pos)) < 0){
									goto err;
								}
							}else if(va_start < num){
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
		}
	}
	filp_close(file, NULL);
	return 0;
err:	
	filp_close(file, NULL);
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
	// int size;
	// char *buf;
        loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)){
		printk("pre_file open err=%ld", PTR_ERR(file));
		goto err;
	}
	
        // buf = kmalloc(PATH_MAX, GFP_KERNEL);
        // if(!buf)
		// goto err;
	// memset(buf, '\0', 100);

	list_for_each_entry(m_head, &usr_m_head, list){
		if(m_head->pid == current->pid){
		// if(m_head->pid == target_task->pid){
			list_for_each_entry(itr, &m_head->head, list){
				if(itr->num & PTE_FLAG_MASK && itr->va <= va && va < itr->va + OFFSET_SIZE){
					printk(KERN_INFO "pgtable found %lx\n",va);
					if(__recover_pgtable(itr->num & PT_PGTABLE_MASK_NOT, itr, file, &pos) < 0){
						goto err;
					}
					filp_close(file, NULL);
					return 0;
				}
			}
		}
	}
err:
	printk(KERN_INFO "recover pgtable va:%lx error!\n",va);
	filp_close(file, NULL);
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

void delete_ds(struct ds_list *itr, unsigned long start, unsigned long end)
{
	struct ds_list *next;

	if(itr->base < start && end < itr->limit){ // base->start, end->limit
		// printk(KERN_INFO "    %lx %lx", itr->base, itr->limit);
		if((next = make_ds_node(end, itr->limit, itr->offset, itr->flag)) == NULL)
			goto out;
		itr->limit = start;
		list_add(&next->list, &itr->list);
		goto out;
	}

	while(end > itr->base){
		// printk(KERN_INFO "    %lx %lx", itr->base, itr->limit);
		next = list_next_entry(itr, list);
		if(itr->base < start && itr->limit <= end){ // base->start
			itr->limit = start;
		}else if(start <= itr->base && end < itr->limit){ // end->limit
			itr->base = end;
		}else if(start <= itr->base && itr->limit <= end){ // all delete
			list_del(&itr->list);
			kfree(itr);
		}
		itr = next;
	}
out:
	return;
}

void delete_ds_m_free_pte(unsigned long va)
{
	struct ds_list *ds_node;
	struct ds_head_list *ds_head;
	struct m_list *m_node;
	struct m_head_list *m_head;

	unsigned long va_start = 0, va_end = 0;

	list_for_each_entry(m_head, &usr_m_head, list){
		if(m_head->pid == current->pid){
			// printk(KERN_INFO "delete m pte %lx", va);
			list_for_each_entry(m_node, &m_head->head, list){
				if(m_node->num & PTE_FLAG_MASK && m_node->va <= va && va < m_node->va + OFFSET_SIZE){
					va_start = m_node->num & PT_PGTABLE_MASK_NOT;
					va_end = va_start + PT_PGTABLE_SIZE;
					list_del(&m_node->list);
					kfree(m_node);
					// printk(KERN_INFO "delete m pte %lx %lx-%lx", va, va_start, va_end);
					break;
				}
			}
			break;
		}
	}
	
	list_for_each_entry(ds_head, &usr_ds_head, list){
		if(ds_head->pid == current->pid){
			list_for_each_entry(ds_node, &ds_head->head, list){
				if(ds_node->limit <= va_start){
					continue;
				}else if(va_end <= ds_node->base){
					break;
				}else{
					delete_ds(ds_node, va_start, va_end);
					// printk(KERN_INFO "delete user ds");
					break;					
				}
			}
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

	unsigned long num;
	list_for_each_entry(m_head, &usr_m_head, list){
		if(m_head->pid == current->pid){
			// printk(KERN_INFO "delete m pmd %lx", va);
			list_for_each_entry(m_node, &m_head->head, list){
				if(m_node->num & PMD_FLAG_MASK && m_node->va <= va && va < m_node->va + OFFSET_SIZE){
					num = m_node->num;
					list_del(&m_node->list);
					kfree(m_node);
					// printk(KERN_INFO "delete m pmd %lx %lx", va, num);
					break;
				}
			}
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

	unsigned long num;
	list_for_each_entry(m_head, &usr_m_head, list){
		if(m_head->pid == current->pid){
			// printk(KERN_INFO "delete m pud %lx", va);
			list_for_each_entry(m_node, &m_head->head, list){
				if(m_node->num & PUD_FLAG_MASK && m_node->va <= va && va < m_node->va + OFFSET_SIZE){
					num = m_node->num;
					list_del(&m_node->list);
					kfree(m_node);
					// printk(KERN_INFO "delete m pud %lx %lx", va, num);
					break;
				}
			}
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

	list_for_each_entry(m_head, &usr_m_head, list){
		if(m_head->pid == current->pid){
			// printk(KERN_INFO "delete m pgd %lx", va);
			list_for_each_entry(m_node, &m_head->head, list){
				if(m_node->num & PGD_FLAG_MASK){
					list_del(&m_node->list);
					kfree(m_node);
					// printk(KERN_INFO "delete m pgd %lx %lx", va, PGD_FLAG_MASK);
					break;
				}
			}
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(delete_m_free_pgd);

// remake now
static long delete_ds_all(void)
{
	struct ds_list *itr;
	struct ds_head_list *ds_head;
	int count=0;

	while((&usr_ds_head)->next != &usr_ds_head){
		ds_head = list_first_entry(&usr_ds_head, typeof(*ds_head), list);
		while((&ds_head->head)->next != &ds_head->head){
			itr = list_first_entry(&ds_head->head, typeof(*itr), list);
			list_del(&itr->list);
			kfree(itr);
		}
		list_for_each_entry(itr, &ds_head->head, list){
			count++;
		}
		printk(KERN_INFO "delete user ds count %d, pid %d\n", count, ds_head->pid);

		list_del(&ds_head->list);
		kfree(ds_head);
		count = 0;
	}

	list_for_each_entry(ds_head, &usr_ds_head, list){
		count++;
	}
	printk(KERN_INFO "delete user ds head count %d\n", count);

	// while((&ker_ds_head)->next != &ker_ds_head){
	// 	itr = list_first_entry(&ker_ds_head, typeof(*itr), list);
	// 	list_del(&itr->list);
	// 	kfree(itr);
	// }

	// list_for_each_entry(itr, &ker_ds_head, list){
	// 	count++;
	// }
	// printk(KERN_INFO "delete kern ds count %d\n", count);

	return 0;
}

SYSCALL_DEFINE0(mycall_ds_delete)
{
	return delete_ds_all();
}

static long delete_m_all(void)
{
	struct m_list *itr;
	struct m_head_list *m_head;
	int count=0;

	while((&usr_m_head)->next != &usr_m_head){
		m_head = list_first_entry(&usr_m_head, typeof(*m_head), list);
		while((&m_head->head)->next != &m_head->head){
			itr = list_first_entry(&m_head->head, typeof(*itr), list);
			list_del(&itr->list);
			kfree(itr);
		}
		list_for_each_entry(itr, &m_head->head, list){
			count++;
		}
		printk(KERN_INFO "delete user m count %d, pid %d\n", count, m_head->pid);

		list_del(&m_head->list);
		kfree(m_head);
		count = 0;
	}

	list_for_each_entry(m_head, &usr_m_head, list){
		count++;
	}
	printk(KERN_INFO "delete user m head count %d\n", count);

	// while((&ker_m_head)->next != &ker_m_head){
	// 	itr = list_first_entry(&ker_m_head, typeof(*itr), list);
	// 	list_del(&itr->list);
	// 	kfree(itr);
	// }
	// list_for_each_entry(itr, &ker_m_head, list){
	// 	count++;
	// }
	// printk(KERN_INFO "delete kern m count %d\n", count);

	return 0;
}

SYSCALL_DEFINE0(mycall_m_delete)
{
	return delete_m_all();
}

// static int do_ds_mkwrite(struct ds_list *ds_node, unsigned long num, struct ds_head_list *ds_head)
// {
// 	struct ds_list *new, *next, *prev;
	
// 	if(ds_node->base == num){
// 		if((new = make_ds_node(ds_node->base, ds_node->base + 1, ds_node->offset, ds_node->flag | _PAGE_RW)) == NULL)
// 			return -ENOMEM;
// 		ds_node->base++;
// 		list_add_tail(&new->list, &ds_node->list);
// 		if(!list_is_first(&new->list, &ds_head->head)){
// 			prev = list_prev_entry(new, list);
// 			ds_node_merge(prev, new);
// 		}
// 	}else if(ds_node->limit - 1 == num){
// 		if((new = make_ds_node(ds_node->limit - 1, ds_node->limit, ds_node->offset, ds_node->flag | _PAGE_RW)) == NULL)
// 			return -ENOMEM;
// 		ds_node->limit--;
// 		list_add(&new->list, &ds_node->list);
// 		if(!list_is_last(&new->list, &ds_head->head)){
// 			next = list_next_entry(new, list);
// 			ds_node_merge(new, next);
// 		}
// 	}else{
// 		if((new = make_ds_node(num, num + 1, ds_node->offset, ds_node->flag | _PAGE_RW)) == NULL)
// 			return -ENOMEM;
// 		if((next = make_ds_node(num + 1, ds_node->limit, ds_node->offset, ds_node->flag)) == NULL)
// 			return -ENOMEM;
// 		ds_node->limit = num;
// 		list_add(&new->list, &ds_node->list);
// 		list_add(&next->list, &new->list);
// 	}
// 	return 1;
// }

// static bool is_ds_write(struct ds_list *ds_node)
// {
// 	if(ds_node->flag & _PAGE_RW)
// 		return true;
// 	else
// 		return false;
// }

// int ds_mkwrite(pte_t pte)
// {
// 	struct ds_list *ds_node;
// 	struct ds_head_list *ds_head;
// 	struct m_list *m_node;
// 	struct m_head_list *m_head;

// 	unsigned long va = (unsigned long)&pte;
// 	unsigned long num = 0;
// 	int ret = 0;

// 	list_for_each_entry(m_head, &usr_m_head, list){
// 		if(m_head->pid == current->pid){
			// printk(KERN_INFO "mkwrite: pte %lx, va %lx", pte_pfn(pte), va);
// 			list_for_each_entry(m_node, &m_head->head, list){
// 				if(m_node->num & PTE_FLAG_MASK && m_node->va <= va && va < m_node->va + OFFSET_SIZE){
// 					num = make_ds_va((m_node->num >> 27) & PT_PGTABLE_MASK, (m_node->num >> 18) & PT_PGTABLE_MASK, (m_node->num >> 9) & PT_PGTABLE_MASK, ((va - m_node->va) / 0x8) & PT_PGTABLE_MASK);
// 					// printk(KERN_INFO "make mkwritre %lx %lx %lx-%lx", num, va, m_node->num, m_node->va);
// 					break;
// 				}
// 			}
// 			break;
// 		}
// 	}
	
// 	list_for_each_entry(ds_head, &usr_ds_head, list){
// 		if(ds_head->pid == current->pid){
// 			list_for_each_entry(ds_node, &ds_head->head, list){
// 				if(ds_node->limit <= num){
// 					continue;
// 				}else if(num < ds_node->base){
// 					break;
// 				}else{
// 					if(!is_ds_write(ds_node)){
// 						printk(KERN_INFO "make writre %lx %lx-%lx", num, ds_node->base, ds_node->limit);
// 						ret = do_ds_mkwrite(ds_node, num, ds_head);
// 						printk(KERN_INFO "make writre %lx finish", num);
// 					}
// 					break;					
// 				}
// 			}
// 			break;
// 		}
// 	}
// 	return ret;
// }
// EXPORT_SYMBOL_GPL(ds_mkwrite);

// static int do_ds_wrprotect(struct ds_list *ds_node, unsigned long num, struct ds_head_list *ds_head)
// {
// 	struct ds_list *new, *next, *prev;
	
// 	if(ds_node->base == num){
// 		if((new = make_ds_node(ds_node->base, ds_node->base + 1, ds_node->offset, ds_node->flag & _PAGE_RW_NOT)) == NULL)
// 			return -ENOMEM;
// 		ds_node->base++;
// 		list_add_tail(&new->list, &ds_node->list);
// 		if(!list_is_first(&new->list, &ds_head->head)){
// 			prev = list_prev_entry(new, list);
// 			ds_node_merge(prev, new);
// 		}
// 	}else if(ds_node->limit - 1 == num){
// 		if((new = make_ds_node(ds_node->limit - 1, ds_node->limit, ds_node->offset, ds_node->flag & _PAGE_RW_NOT)) == NULL)
// 			return -ENOMEM;
// 		ds_node->limit--;
// 		list_add(&new->list, &ds_node->list);
// 		if(!list_is_last(&new->list, &ds_head->head)){
// 			next = list_next_entry(new, list);
// 			ds_node_merge(new, next);
// 		}
// 	}else{
// 		if((new = make_ds_node(num, num + 1, ds_node->offset, ds_node->flag & _PAGE_RW_NOT)) == NULL)
// 			return -ENOMEM;
// 		if((next = make_ds_node(num + 1, ds_node->limit, ds_node->offset, ds_node->flag)) == NULL)
// 			return -ENOMEM;
// 		ds_node->limit = num;
// 		list_add(&new->list, &ds_node->list);
// 		list_add(&next->list, &new->list);
// 	}
// 	return 1;
// }

// int ds_wrprotect(pte_t pte)
// {
// 	struct ds_list *ds_node;
// 	struct ds_head_list *ds_head;
// 	struct m_list *m_node;
// 	struct m_head_list *m_head;

// 	unsigned long va = (unsigned long)&pte;
// 	unsigned long num = 0;
// 	int ret = 0;

// 	list_for_each_entry(m_head, &usr_m_head, list){
// 		if(m_head->pid == current->pid){
			// printk(KERN_INFO "wrprotect: pte %lx, va %lx", pte_pfn(pte), va);
// 			list_for_each_entry(m_node, &m_head->head, list){
// 				if(m_node->num & PTE_FLAG_MASK && m_node->va <= va && va < m_node->va + OFFSET_SIZE){
// 					num = make_ds_va((m_node->num >> 27) & PT_PGTABLE_MASK, (m_node->num >> 18) & PT_PGTABLE_MASK, (m_node->num >> 9) & PT_PGTABLE_MASK, ((va - m_node->va) / 0x8) & PT_PGTABLE_MASK);
// 					printk(KERN_INFO "make wrprotect %lx %lx %lx-%lx", num, va, m_node->num, m_node->va);
// 					break;
// 				}
// 			}
// 			break;
// 		}
// 	}
	
// 	list_for_each_entry(ds_head, &usr_ds_head, list){
// 		if(ds_head->pid == current->pid){
// 			list_for_each_entry(ds_node, &ds_head->head, list){
// 				if(ds_node->limit <= num){
// 					continue;
// 				}else if(num < ds_node->base){
// 					break;
// 				}else{
// 					if(is_ds_write(ds_node)){
// 						printk(KERN_INFO "make wrprotect %lx %lx-%lx", num, ds_node->base, ds_node->limit);
// 						ret = do_ds_wrprotect(ds_node, num, ds_head);
// 						printk(KERN_INFO "make wrprotect %lx finish", num);
// 					}
// 					break;					
// 				}
// 			}
// 			break;
// 		}
// 	}
// 	return ret;
// }
// EXPORT_SYMBOL_GPL(ds_wrprotect);

extern long make_user_pgtable2(struct task_struct *p);

bool check_parent_is_target(pid_t ppid, pid_t pid)
{
	struct ds_head_list *ds_node;

	// printk(KERN_INFO "parent pid %d, child pid %d\n", ppid, pid);
	
	list_for_each_entry(ds_node, &usr_ds_head, list){
		if(ds_node->pid == ppid){
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
	// register_pid(p->pid);
	// make_ds_list_usr_from_pgtable(p);
	// make_user_pgtable2(p);
}
EXPORT_SYMBOL_GPL(register_child);
