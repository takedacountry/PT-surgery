#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <asm/current.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/page_types.h>
#include <asm/pgtable_types.h>
#include <asm/paravirt.h>
#include <asm-generic/pgalloc.h>
#include <asm-generic/barrier.h>
#include <asm-generic/memory_model.h>

#define USER_MAX 0x100
#define MAX 0x200
#define SAME_ADDR_SHIFT 16
#define SAME_ADDR_MASK (_AT(long, 1) << SAME_ADDR_SHIFT)
#define SAME_ADDR_MASK_NOT (~(SAME_ADDR_MASK))
#define HIT_FLAG_SHIFT 0
#define CONTI_FLAG_SHIFT 1
#define SAME_FLAG_SHIFT 2
#define HIT_FLAG_MASK (_AT(int, 1) << HIT_FLAG_SHIFT)
#define CONTI_FLAG_MASK (_AT(int, 1) << CONTI_FLAG_SHIFT)
#define SAME_FLAG_MASK (_AT(int, 1) << SAME_FLAG_SHIFT)
#define HIT_FLAG_MASK_NOT (~(HIT_FLAG_MASK))
#define CONTI_FLAG_MASK_NOT (~(CONTI_FLAG_MASK))
#define SAME_FLAG_MASK_NOT (~(SAME_FLAG_MASK))

static unsigned long pte_value;
static unsigned long pte_flag;

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


static int get_pfn_scan_pte(pmd_t *pmdp, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte)
{
  	pte_t *ptep = pte_offset_index(pmdp, pte);

  	if(pte_none(*ptep) || !pte_present(*ptep)) {
    		// printk(KERN_INFO "pte %lu is not present.\n", pte);
    		return 4;
  	}
	pte_value = pte_pfn(*ptep);
	pte_flag = pte_flags(*ptep);
	
  	return 1;
}

static int get_pfn_scan_pmd(pud_t *pudp, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte)
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
  	return get_pfn_scan_pte(pmdp, pgd, pud, pmd, pte);
}

static int get_pfn_scan_pud(p4d_t *p4dp, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte)
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
  	return get_pfn_scan_pmd(pudp, pgd, pud, pmd, pte);  
}

static int get_pfn_scan_p4d(pgd_t *pgdp, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte)
{
  	p4d_t *p4dp = p4d_offset_index(pgdp, pgd);
	
	if(p4d_none(*p4dp) || !p4d_present(*p4dp)){
	    	// printk(KERN_INFO "p4d %lu is not present", pgd);
    		return 7;
  	}
  	return get_pfn_scan_pud(p4dp, pgd, pud, pmd, pte);
}

static int get_pfn_scan_pgd(struct mm_struct *mm, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte)
{
  	pgd_t *pgdp = pgd_offset_index(mm, pgd);

  	if(pgd_none(*pgdp) || !pgd_present(*pgdp)){
	    	// printk(KERN_INFO "pgd %lu is not present.\n", pgd);
    		return 7;
  	}
  	return get_pfn_scan_p4d(pgdp, pgd, pud, pmd, pte);
}

static int search_pgtable_get_pfn(unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte)
{
  	struct mm_struct *mm = current->mm;

  	if(pgd<0 || 512<=pgd || pud<0 || 512<=pud || pmd<0 || 512<=pmd || pte<0 || 512<=pte) {
    		printk(KERN_INFO "error: The numbers are not appropriate.\n");
    		return 0;
  	}
  	return get_pfn_scan_pgd(mm, pgd, pud, pmd, pte);
}

struct ds_pgtable{
	unsigned long base;
	unsigned long limit;
	long offset;
	unsigned long flag;
	struct list_head list;
};

static LIST_HEAD(ds_pgtable_head);
		
static long make_ds_va(unsigned long a, unsigned long b, unsigned long c, unsigned long d)
{
	unsigned long va = a << 27 | b << 18 | c << 9 | d;
	return va;	
}

static long make_ds_offset(long base)
{
	long offset = base - pte_value;
	return offset;
}

static int make_ds_list(unsigned long base, unsigned long limit, long offset, unsigned long flag)
{
	struct ds_pgtable *list = kmalloc(sizeof(struct ds_pgtable), GFP_KERNEL);
	if(!list)
		return -ENOMEM;

	list->base = base;
	list->limit = limit;
	list->offset = offset;
	list->flag = flag;
	list_add_tail(&list->list, &ds_pgtable_head);
	return 0;
}

static long make_ds_user(void)
{
	int num;
	int count;
	int hit_flag = 0;
		
	unsigned long base;
	unsigned long limit;
	long offset;
	unsigned long flag;
	
	unsigned long pte_value_pre;
	unsigned long pte_flag_pre;
	
	// struct file *file;
	// char *filename = "./user_pgtable";
	// int size;
	// char *buf;
 //        loff_t pos = 0;

	// file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	// if(IS_ERR(file)){
	// 	printk("pre_file open err=%ld", PTR_ERR(file));
	// 	goto end;
	// }
	
 //        buf = kmalloc(PATH_MAX, GFP_KERNEL);
 //        if(!buf)
	// 	goto end;
	// memset(buf, '\0', 100);

	for(unsigned long a=0; a<USER_MAX; a++){
        	for(unsigned long b=0; b<MAX; b++){
            		for(unsigned long c=0; c<MAX; c++){
                		for(unsigned long d=0; d<MAX; d++){
                    			if((num = search_pgtable_get_pfn(a, b, c, d)) > 0 && num < 4){ //pte hit
						if(hit_flag == 0){ // miss, first hit
							// make ds members
							base = make_ds_va(a, b, c, d);
							offset = make_ds_offset(base);
							flag = pte_flag;
							
							hit_flag = 1;
							pte_value_pre = pte_value; // pte_value_pre initialize
							pte_flag_pre = pte_flag; // pte_flag_pre initialize
						}else if(pte_value == pte_value_pre + 1 && pte_flag == pte_flag_pre){ // continuous address hit
							pte_value_pre = pte_value;
						}else{ // last hit, first nit
							// add ds_pgtable list
							limit = make_ds_va(a, b, c, d);
							if(make_ds_list(base, limit, offset, flag) < 0)
								goto end;

							// make ds members
							base = limit;
							offset = make_ds_offset(base);
							flag = pte_flag;
							
							pte_value_pre = pte_value;
							pte_flag_pre = pte_flag;
						}
                        			count = num;
                    			}else if(num == 0){ // error
						goto end;
					}else{ // pte miss
						if(hit_flag > 0){ // last hit, miss
							// add ds_pgtable list
							limit = make_ds_va(a, b, c, d);
							if(make_ds_list(base, limit, offset, flag) < 0)
								goto end;
							hit_flag = 0;
						}
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
	// kfree(buf);
	// filp_close(file, NULL);
	
	return 0;
}

static long make_ds_kernel(void)
{
	int num;
	int count;
	int ds_flag = 0;
	
	unsigned long base;
	unsigned long limit;
	long offset;
	unsigned long flag;
	
	unsigned long pte_value_pre;
	unsigned long pte_flag_pre;

	// struct file *file;
	// char *filename = "./kernel_pgtable";
	// int size;
	// char *buf;
 //        loff_t pos = 0;

	// file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	// if(IS_ERR(file)){
	// 	printk("pre_file open err=%ld", PTR_ERR(file));
	// 	goto end;
	// }
	
 //        buf = kmalloc(PATH_MAX, GFP_KERNEL);
 //        if(!buf)
	// 	goto end;
	// memset(buf, '\0', 100);
	
	
	for(unsigned long a=USER_MAX; a<MAX; a++){
        	for(unsigned long b=0; b<MAX; b++){
            		for(unsigned long c=0; c<MAX; c++){
                		for(unsigned long d=0; d<MAX; d++){
                    			if((num = search_pgtable_get_pfn(a, b, c, d)) > 0 && num < 4){ //pte hit
						/*
						if(pte_value == 0x100056){
							pte_number = make_ds_va(a, b, c, d);
							if(ds_flag & HIT_FLAG_MASK){ // last hit, first_hit 0x100056
								limit = make_ds_va(a, b, c, d);
								if(ds_flag & SAME_FLAG_MASK){
									flag |= SAME_ADDR_MASK;
									ds_flag &= SAME_FLAG_MASK_NOT;
								}
								if(make_ds_list(base, limit, offset, flag) < 0)
									goto end;
								ds_flag &= HIT_FLAG_MASK_NOT;
							}
							if(!(ds_flag & SAME_FLAG_MASK)){ //first hit 0x100056
								base = pte_number;
								offset = make_ds_offset(base);
								flag = pte_flag;
								pte_flag_pre = pte_flag;
								ds_flag |= SAME_FLAG_MASK;
								pte_number_pre = base;
							}else if(pte_flag_pre == pte_flag && pte_number - pte_number_pre == 0x10){ // second hit ~ last hit 0x100056
								pte_number = make_ds_va(a, b, c, d);
								if(pte_number - pte_number_pre == 0x10){ // continue
									ds_flag |= SAME16_FLAG_MASK;
									pte_number_pre = pte_number;
								}else{ // finish
									limit = pte_number;
									if(ds_flag & SAME_FLAG_MASK){
										flag |= SAME_ADDR_MASK;
										ds_flag &= SAME_FLAG_MASK_NOT;
									}
									if(ds_flag & SAME16_FLAG_MASK){
										flag |= SAME16_ADDR_MASK;
										ds_flag &= SAME16_FLAG_MASK_NOT;
									}
									if(make_ds_list(base, limit, offset, flag) < 0)
										goto end;
									
								
							}

						}else 
      						*/

						if(!(ds_flag & HIT_FLAG_MASK)){
							// make ds members
							base = make_ds_va(a, b, c, d);
							offset = make_ds_offset(base);
							flag = pte_flag;
							
							ds_flag |= HIT_FLAG_MASK;
							pte_value_pre = pte_value; // pte_value_pre initialize
							pte_flag_pre = pte_flag; // pte_flag_pre initialize
						}else if(pte_value == pte_value_pre + 1 && pte_flag == pte_flag_pre && !(ds_flag & SAME_FLAG_MASK)){ // continuous address hit
							pte_value_pre = pte_value;
							if(!(ds_flag & CONTI_FLAG_MASK))
								ds_flag |= CONTI_FLAG_MASK;
						}else if(pte_value == pte_value_pre && pte_flag == pte_flag_pre && !(ds_flag & CONTI_FLAG_MASK)){ // same address hit
							if(!(ds_flag & SAME_FLAG_MASK))
								ds_flag |= SAME_FLAG_MASK;
						}else{ // last hit, first nit
							// add ds_pgtable list
							limit = make_ds_va(a, b, c, d);
							if(ds_flag & SAME_FLAG_MASK){
								flag |= SAME_ADDR_MASK;
								ds_flag &= SAME_FLAG_MASK_NOT;
							}
							if(make_ds_list(base, limit, offset, flag) < 0)
								goto end;

							// make ds members
							base = limit;
							offset = make_ds_offset(base);
							flag = pte_flag;
							
							pte_value_pre = pte_value;
							pte_flag_pre = pte_flag;
							ds_flag &= CONTI_FLAG_MASK_NOT;
						}
                        			count = num;
                    			}else if(num == 0){ // error
						goto end;
					}else{ // pte miss
						if(ds_flag & HIT_FLAG_MASK){ // last hit, miss
							// add ds_pgtable list
							limit = make_ds_va(a, b, c, d);
							if(ds_flag & SAME_FLAG_MASK){
								flag |= SAME_ADDR_MASK;
							}
							if(make_ds_list(base, limit, offset, flag) < 0)
								goto end;
							ds_flag = 0;
						}
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
	// kfree(buf);
	// filp_close(file, NULL);
	
	return 0;
}


SYSCALL_DEFINE0(mycall_ds_make)
{
	long ret1, ret2;
	ret1 = make_ds_user();
	ret2 = make_ds_kernel();
	
	if(ret1 == ret2)
		return 0;
   	return -1;
}

SYSCALL_DEFINE0(mycall_ds_make_user)
{
	long ret;
	ktime_t start, end;

	start = ktime_get();
	ret = make_ds_user();
	end = ktime_get();

	printk(KERN_INFO "ds_make_user time: %lld\n", ktime_sub(end, start));
	
	return ret;
}

SYSCALL_DEFINE0(mycall_ds_make_kernel)
{
	long ret;
	ktime_t start, end;

	start = ktime_get();
	ret = make_ds_kernel();
	end = ktime_get();

	printk(KERN_INFO "ds_make_kernel time: %lld\n", ktime_sub(end, start));
	
	return ret;
}

SYSCALL_DEFINE0(mycall_ds_search)
{
	struct ds_pgtable *itr;
	int count = 0;
	int flag = 0;
	list_for_each_entry(itr, &ds_pgtable_head, list){
		if(flag == 0 && itr->base >= 0x800000000){
			printk(KERN_INFO "user ds count %d\n", count);
			count = 0;
			flag = 1;
		}
		// if(itr->limit != itr->base + 1){
			// printk(KERN_INFO "%d %lx %lx %lx %lx", count, itr->base, itr->limit, itr->offset, itr->flag);
		// }
		count++;
	}
	printk(KERN_INFO "kernel ds count %d\n", count);
	return 0;
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

static int search_pgtable_get_pmd(unsigned long pgd, unsigned long pud, unsigned long pmd, pmd_t **pmdp)
{
  	struct mm_struct *mm = current->mm;

  	if(pgd<0 || 512<=pgd || pud<0 || 512<=pud || pmd<0 || 512<=pmd) {
    		printk(KERN_INFO "error: The numbers are not appropriate.\n");
    		return 0;
  	}
  	return get_pmd_scan_pgd(mm, pgd, pud, pmd, pmdp);
}

static void pte_realloc_pmd_populate(struct mm_struct *mm, pmd_t *pmd, struct page *pte)
{
	unsigned long pfn = page_to_pfn(pte);

	paravirt_alloc_pte(mm, pfn);
	set_pmd(pmd, __pmd(((pteval_t)pfn << PAGE_SHIFT) | pmd_flags(*pmd)));
}

static void pte_realloc_pmd_install(struct mm_struct *mm, pmd_t *pmd, pgtable_t *pte, spinlock_t **ptlp)
{
	spinlock_t *ptl = pmd_lock(mm, pmd);
	*(ptlp) = ptl;
	
	if(!pmd_none(*pmd) && pmd_present(*pmd)){
		// mm_inc_nr_ptes(mm);
		smp_wmb();
		pte_realloc_pmd_populate(mm, pmd, *pte);
		*pte = NULL;
	}
	// spin_unlock(ptl);
}


static int pte_realloc(struct mm_struct *mm, pmd_t *pmd, spinlock_t **ptlp)
{
	pgtable_t new = pte_alloc_one(mm); 
	if (!new)
		return 1;
	
	pte_realloc_pmd_install(mm, pmd, &new, ptlp);
	if (new)
		pte_free(mm, new);
	return 0;
}

// user only
static pte_t *pte_realloc_offset_head(struct mm_struct *mm, pmd_t *pmdp, spinlock_t **ptlp)
{
	return pte_realloc(mm, pmdp, ptlp) ? NULL : pte_offset_index(pmdp, 0);
}

static void pte_realloc_kernel_pmd_populate(struct mm_struct *mm, pmd_t *pmd, pte_t *pte, struct file *file, loff_t *pos)
{
	int size;
	char *buf;
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if(!buf)
		return;
	memset(buf, '\0', 100);
	
	paravirt_alloc_pte(mm, __pa(pte) >> PAGE_SHIFT);

	size = sprintf(buf, "paravirt alloc pte\n");
	kernel_write(file, buf, size, pos);
	vfs_fsync_range(file, 0, size, 1);
	
	set_pmd(pmd, __pmd(__pa(pte) | pmd_flags(*pmd)));

	size = sprintf(buf, "set pmd\n");
	kernel_write(file, buf, size, pos);
	vfs_fsync_range(file, 0, size, 1);
}

static int pte_realloc_kernel(pmd_t *pmd, struct file *file, loff_t *pos)
{
	int size;
	char *buf;
	
	// spinlock_t *ptl;
	pte_t *new = pte_alloc_one_kernel(&init_mm);
	if (!new)
		return 1;

        buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if(!buf)
		return 1;
	memset(buf, '\0', 100);

	size = sprintf(buf, "pte alloc one kernel\n");
	kernel_write(file, buf, size, pos);
	vfs_fsync_range(file, 0, size, 1);
	
	
	// ptl = pmd_lock(mm, pmd);
	// *(ptlp) = ptl;
	spin_lock(&init_mm.page_table_lock);

	size = sprintf(buf, "spinlock\n");
	kernel_write(file, buf, size, pos);
	vfs_fsync_range(file, 0, size, 1);
	
	if (!pmd_none(*pmd) && pmd_present(*pmd)) {
		smp_wmb(); /* See comment in pmd_install() */
		pte_realloc_kernel_pmd_populate(&init_mm, pmd, new, file, pos);
		new = NULL;
	}
	
	size = sprintf(buf, "pmd populate\n");
	kernel_write(file, buf, size, pos);
	vfs_fsync_range(file, 0, size, 1);
	
	if (new)
		pte_free_kernel(&init_mm, new);
	return 0;
}

// kernel only
static pte_t *pte_realloc_kernel_offset_head(pmd_t *pmdp, struct file *file, loff_t *pos)
{
	return pte_realloc_kernel(pmdp, file, pos) ? NULL : pte_offset_index(pmdp, 0);
}

static void dup_pte(pte_t **ptep, struct ds_pgtable *itr, unsigned long start, unsigned long end)
{
	unsigned long count;
	unsigned long flag;
	pte_t *pte = *(ptep);

	if(itr->base <= start && end < itr->limit){
		// recover pgtable from one ds
		if(itr->flag & SAME_ADDR_MASK){
			flag = itr->flag & SAME_ADDR_MASK_NOT;
			for(count=start; count <= end; count++){
				set_pte(pte, __pte(((pteval_t)(itr->base - itr->offset) << PAGE_SHIFT) | flag));
				pte++;
			}
		}else{
			for(count=start; count <= end; count++){
				set_pte(pte, __pte(((pteval_t)(count - itr->offset) << PAGE_SHIFT) | itr->flag));
				pte++;
			}
		}		
	}else if(itr->base <= start && itr->limit <= end){
		// first recover
		if(itr->flag & SAME_ADDR_MASK){
			flag = itr->flag & SAME_ADDR_MASK_NOT;
			for(count=start; count < itr->limit; count++){
				set_pte(pte, __pte(((pteval_t)(itr->base - itr->offset) << PAGE_SHIFT) | flag));
				pte++;
			}
		}else{
			for(count=start; count < itr->limit; count++){
				set_pte(pte, __pte(((pteval_t)(count - itr->offset) << PAGE_SHIFT) | itr->flag));
				pte++;
			}
		}
	}else if(start < itr->base && end < itr->limit){
		// last recover
		if(itr->flag & SAME_ADDR_MASK){
			flag = itr->flag & SAME_ADDR_MASK_NOT;
			for(count=start; count <= end; count++){
				if(count >= itr->base)
					set_pte(pte, __pte(((pteval_t)(itr->base - itr->offset) << PAGE_SHIFT) | flag));
				pte++;
			}
		}else{
			for(count=start; count <= end; count++){
				if(count >= itr->base)
					set_pte(pte, __pte(((pteval_t)(count - itr->offset) << PAGE_SHIFT) | itr->flag));
				pte++;
			}
		}
	}else if(start < itr->base && itr->limit <= end){
		// second ~ last-1 recover
		if(itr->flag & SAME_ADDR_MASK){
			flag = itr->flag & SAME_ADDR_MASK_NOT;
			for(count=start; count < itr->limit; count++){
				if(count >= itr->base)
					set_pte(pte, __pte(((pteval_t)(itr->base - itr->offset) << PAGE_SHIFT) | flag));
				pte++;
			}
		}else{
			for(count=start; count < itr->limit; count++){
				if(count >= itr->base)
					set_pte(pte, __pte(((pteval_t)(count - itr->offset) << PAGE_SHIFT) | itr->flag));
				pte++;
			}
		}
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

static long recover_pgtable(void)
{
	pmd_t *pmdp;
	pte_t *ptep_old;
	pte_t *ptep_new;
	
	unsigned long va_start;
	unsigned long va_end;
	spinlock_t *ptl;
	
	int num;
	int count;
	struct ds_pgtable *itr;

	struct file *file;
	char *filename = "./write_log_txt";
	int size;
	char *buf;
        loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)){
		printk("pre_file open err=%ld", PTR_ERR(file));
		goto end;
	}
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if(!buf)
		goto end;
	memset(buf, '\0', 100);
	
	for(unsigned long a=0; a<MAX; a++){
        	for(unsigned long b=0; b<MAX; b++){
            		for(unsigned long c=0; c<MAX; c++){
				if((num = search_pgtable_get_pmd(a, b, c, &pmdp)) == 1){ // in user
					// pte_alloc
					ptep_old = pte_offset_index(pmdp, 0);
					// printk(KERN_INFO "pmd before: %lx",(unsigned long)pmd_val(*pmdp));
					// printk(KERN_INFO "pte before: %lx", (unsigned long)__pa(ptep_old));
					ptep_new = pte_realloc_offset_head(current->mm, pmdp, &ptl);
					// printk(KERN_INFO "pmd after:  %lx",(unsigned long)pmd_val(*pmdp));
					if(!ptep_new){
						spin_unlock(ptl);
						goto end;
					}
					// printk(KERN_INFO "pte after:  %lx", (unsigned long)__pa(ptep_new));
					
					// list_for_each_entry ds
					va_start = make_ds_va(a, b, c, 0);
					va_end = make_ds_va(a, b, c, 511);

					// printk(KERN_INFO "%ld-%ld-%ld-0  %lx %lx", a, b, c, va_start, va_end);
					
					list_for_each_entry(itr, &ds_pgtable_head, list){
						if(itr->limit <= va_start){
							continue; // not hit yet
						}else if(va_end < itr->base){
							break; // already finished
						}else{ // recover pgtable from ds
							// printk(KERN_INFO "    %lx %lx %lx %lx", itr->base, itr->limit, itr->offset, itr->flag);
							dup_pte(&ptep_new, itr, va_start, va_end);
							va_start = itr->limit;
						}
					}
					spin_unlock(ptl);
					// pte_free(current->mm, virt_to_page(ptep_old));
					// print_pte(pmdp);
				}
				else if(num == 2){ // in kernel
					goto end;
					
					ptep_old = pte_offset_index(pmdp, 0);
					
					// printk(KERN_INFO "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
					size = sprintf(buf, "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
					// printk(KERN_INFO "pte before: %lx\n", (unsigned long)__pa(ptep_old));
					size = sprintf(buf, "pte before: %lx\n", (unsigned long)__pa(ptep_old));
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
					
					ptep_new = pte_realloc_kernel_offset_head(pmdp, file, &pos);
					
					// printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
					size = sprintf(buf, "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
					
					if(!ptep_new){
						spin_unlock(&init_mm.page_table_lock);
						// spin_unlock(ptl);
						goto end;
					}
					
					// printk(KERN_INFO "pte after:  %lx", (unsigned long)__pa(ptep_new));
					size = sprintf(buf, "pte after: %lx\n",(unsigned long)__pa(ptep_new));
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
					
					// list_for_each_entry ds
					va_start = make_ds_va(a, b, c, 0);
					va_end = make_ds_va(a, b, c, 511);

					// printk(KERN_INFO "%ld-%ld-%ld-0  %lx %lx\n", a, b, c, va_start, va_end);
					size = sprintf(buf, "%ld-%ld-%ld-0  %lx %lx\n", a, b, c, va_start, va_end);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
					
					list_for_each_entry(itr, &ds_pgtable_head, list){
						if(itr->limit <= va_start){
							continue; // not hit yet
						}else if(va_end < itr->base){
							break; // already finished
						}else{ // recover pgtable from ds
							// printk(KERN_INFO "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
							size = sprintf(buf, "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
							kernel_write(file, buf, size, &pos);
							vfs_fsync_range(file, 0, size, 1);
							
							dup_pte(&ptep_new, itr, va_start, va_end);
							va_start = itr->limit;
						}
					}
					pte_free_kernel(&init_mm, ptep_old);
					spin_unlock(&init_mm.page_table_lock);
					// spin_unlock(ptl);
					// print_pte(pmdp);
					
				}
				else if(num == 0){
					goto end;
				}
				else{
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
end:	
	kfree(buf);
	filp_close(file, NULL);
	return 0;	
}


SYSCALL_DEFINE0(mycall_recover_pgtable)
{
	int ret = -1;
	printk(KERN_INFO "start recover pgtable\n");
	ret = recover_pgtable();
	printk(KERN_INFO "end recover pgtable\n");
	return ret;
}

static long delete_ds(void)
{
	struct ds_pgtable *itr;

	while((&ds_pgtable_head)->next != &ds_pgtable_head){
		itr = list_first_entry(&ds_pgtable_head, typeof(*itr), list);
		list_del(&itr->list);
		kfree(itr);
	}

	return 0;
}

SYSCALL_DEFINE0(mycall_ds_delete)
{
	return delete_ds();
}
