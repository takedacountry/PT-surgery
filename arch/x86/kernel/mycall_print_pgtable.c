#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/current.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/page_types.h>
#include <asm/string_64.h>
#include <asm/paravirt.h>
#include <asm-generic/pgalloc.h>

#define USER_MAX 0x100
#define MAX 0x200

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

static long make_ds_va(unsigned long a, unsigned long b, unsigned long c, unsigned long d)
{
	unsigned long va = a << 27 | b << 18 | c << 9 | d;
	return va;	
}

static long make_user_pgtable(void)
{
	int num;
	int count;
	int entry_count = 0;
		
	unsigned long pte_num;
	unsigned long pte_num_pre = 0;
	
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
						pte_num = make_ds_va(a, b, c, d); // first entry num
						if(pte_num_pre != pte_num >> 9){
							entry_count++;
							pte_num_pre = pte_num >> 9;
						}
						// entry_count++;
						
						// size = sprintf(buf, "%ld-%ld-%ld-%ld  %lx %lx\n", a, b, c, d, pte_value, pte_flag);
						// kernel_write(file, buf, size, &pos);
						// vfs_fsync_range(file, 0, size, 1);
						
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
	printk(KERN_INFO "user PT count: %d", entry_count);
	
	// size = sprintf(buf, "user PT count: %d", entry_count);
	// kernel_write(file, buf, size, &pos);
	// vfs_fsync_range(file, 0, size, 1);
	
	// kfree(buf);
	// filp_close(file, NULL);
	
	return 0;
}

static long make_user_pgtable2(void)
{
	int num;
	int count;
	int entry_count = 0;
		
	unsigned long pte_num;
	unsigned long pte_num_pre = 0;
	
	// struct file *file;
	// char *filename = "./user_pgtable2";
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
						pte_num = make_ds_va(a, b, c, d); // first entry num
						if(pte_num_pre != pte_num >> 9){
							entry_count++;
							pte_num_pre = pte_num >> 9;
						}
						// entry_count++;
						
						// size = sprintf(buf, "%ld-%ld-%ld-%ld  %lx %lx\n", a, b, c, d, pte_value, pte_flag);
						// kernel_write(file, buf, size, &pos);
						// vfs_fsync_range(file, 0, size, 1);
						
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
	printk(KERN_INFO "user PT count: %d", entry_count);
	
	// size = sprintf(buf, "user PT count: %d", entry_count);
	// kernel_write(file, buf, size, &pos);
	// vfs_fsync_range(file, 0, size, 1);
	
	// kfree(buf);
	// filp_close(file, NULL);
	
	return 0;
}

static long make_kernel_pgtable(void)
{
	int num;
	int count;
	int entry_count = 0;
	
	unsigned long pte_num;
	unsigned long pte_num_pre = 0;

	struct file *file;
	char *filename = "./kernel_pgtable";
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
	
	
	for(unsigned long a=USER_MAX; a<MAX; a++){
        	for(unsigned long b=0; b<MAX; b++){
            		for(unsigned long c=0; c<MAX; c++){
                		for(unsigned long d=0; d<MAX; d++){
                    			if((num = search_pgtable_get_pfn(a, b, c, d)) > 0 && num < 4){ //pte hit
						pte_num = make_ds_va(a, b, c, d); // first entry num
						if(pte_num_pre != pte_num >> 9){
							entry_count++;
							pte_num_pre = pte_num >> 9;
						}

						size = sprintf(buf, "%ld-%ld-%ld-%ld  %lx %lx\n", a, b, c, d, pte_value, pte_flag);
						kernel_write(file, buf, size, &pos);
						vfs_fsync_range(file, 0, size, 1);
						
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
	printk(KERN_INFO "kernel PT count: %d\n", entry_count);
	
	size = sprintf(buf, "kernel PT count: %d\n",entry_count);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);
	
	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}

static long make_kernel_pgtable2(void)
{
	int num;
	int count;
	int entry_count = 0;
	
	unsigned long pte_num;
	unsigned long pte_num_pre = 0;

	// struct file *file;
	// char *filename = "./kernel_pgtable2";
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
						pte_num = make_ds_va(a, b, c, d); // first entry num
						if(pte_num_pre != pte_num >> 9){
							entry_count++;
							pte_num_pre = pte_num >> 9;
						}

						// size = sprintf(buf, "%ld-%ld-%ld-%ld  %lx %lx\n", a, b, c, d, pte_value, pte_flag);
						// kernel_write(file, buf, size, &pos);
						// vfs_fsync_range(file, 0, size, 1);
						
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
	printk(KERN_INFO "kernel PT count: %d\n", entry_count);
	
	// size = sprintf(buf, "kernel PT count: %d\n",entry_count);
	// kernel_write(file, buf, size, &pos);
	// vfs_fsync_range(file, 0, size, 1);
	
	// kfree(buf);
	// filp_close(file, NULL);
	
	return 0;
}

SYSCALL_DEFINE0(mycall_print_user_pgtable){
	return make_user_pgtable();
}

SYSCALL_DEFINE0(mycall_print_kernel_pgtable){
	return make_kernel_pgtable();
}

SYSCALL_DEFINE0(mycall_print_user_pgtable2){
	return make_user_pgtable2();
}

SYSCALL_DEFINE0(mycall_print_kernel_pgtable2){
	return make_kernel_pgtable2();
}
