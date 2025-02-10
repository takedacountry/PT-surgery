#include <linux/kernel.h>
#include <linux/syscalls.h>
// #include <linux/types.h>
#include <linux/fs.h>
// #include <linux/slab.h>
// #include <linux/printk.h>
#include <asm/current.h>
// #include <asm/pgtable.h>
#include "ds.h"
// #include "ds_struct.h"


// extern struct task_struct *target_task;

static int get_ptep(pmd_t *pmdp, unsigned long pte, pte_t **ptepp)
{
  	pte_t *ptep = pte_offset_index(pmdp, pte);
	*(ptepp) = ptep;

  	if(pte_none(*ptep) || !pte_present(*ptep)) {
    	// printk(KERN_INFO "pte %lu is not present.\n", pte);
    	return -1;
  	}
		
  	return 0;
}

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

	if(get_p4dp(pgdp, pgd, p4dpp) < 0)
		return -1;
	
	return 0;
}


long print_user_pgtable(struct task_struct *p)
{
	// pgd_t *pgdp;
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep;
	
	int entry_count = 0;

	struct file *file;
	char *filename = "./user_pgtable";
	int size;
	char *buf;
    loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)) {
		printk("pre_file open err=%ld", PTR_ERR(file));
		goto end;
	}
	
    buf = kmalloc(PATH_MAX, GFP_KERNEL);
    if(!buf)
		goto end;
	memset(buf, '\0', 1024);

	size = sprintf(buf, "pid: %d\n", p->pid);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);

	for(unsigned long pgd=0; pgd<USER_MAX; pgd++) {
		if(get_pgdp(p->mm, pgd, &p4dp) == 0) {
			size = sprintf(buf, "%ld-0-0-0 %lx  %lx %lx  %lx\n", pgd, make_ds_va(pgd, 0, 0, 0), p4d_pfn(*p4dp), p4d_flags(*p4dp), (unsigned long)p4dp);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);

			for(unsigned long pud=0; pud<MAX; pud++) {
				if(get_pudp(p4dp, pud, &pudp) == 0) {
					size = sprintf(buf, "    %ld-%ld-0-0 %lx  %lx %lx  %lx\n", pgd, pud, make_ds_va(pgd, pud, 0, 0), pud_pfn(*pudp), pud_flags(*pudp), (unsigned long)pudp);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
			
		            for(unsigned long pmd=0; pmd<MAX; pmd++) {
						if(get_pmdp(pudp, pmd, &pmdp) == 0) {
							entry_count++;
							size = sprintf(buf, "        %ld-%ld-%ld-0 %lx  %lx %lx  %lx\n", pgd, pud, pmd, make_ds_va(pgd, pud, pmd, 0), pmd_pfn(*pmdp), pmd_flags(*pmdp), (unsigned long)pmdp);
							kernel_write(file, buf, size, &pos);
							vfs_fsync_range(file, 0, size, 1);

							for(unsigned long pte=0; pte<MAX; pte++) {
			                    if(get_ptep(pmdp, pte, &ptep) == 0) {
									size = sprintf(buf, "            %ld-%ld-%ld-%ld %lx  %lx %lx  %lx\n", pgd, pud, pmd, pte, make_ds_va(pgd, pud, pmd, pte), pte_pfn(*ptep), pte_flags(*ptep), (unsigned long)ptep);
									kernel_write(file, buf, size, &pos);
									vfs_fsync_range(file, 0, size, 1);
			                    }
			            	}
						}
		        	}
				}
	        }
		}
    }
	printk(KERN_INFO "user PT count: %d", entry_count);
	
	size = sprintf(buf, "user PT count: %d", entry_count);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);
end:	
	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}
EXPORT_SYMBOL_GPL(print_user_pgtable);

long print_user_pgtable2(struct task_struct *p)
{
	// pgd_t *pgdp;
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep;
	
	int entry_count = 0;

	struct file *file;
	char *filename = "./user_pgtable2";
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
	memset(buf, '\0', 1024);

	size = sprintf(buf, "pid: %d\n", p->pid);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);

	for(unsigned long pgd=0; pgd<USER_MAX; pgd++) {
		if(get_pgdp(p->mm, pgd, &p4dp) == 0) {
			size = sprintf(buf, "%ld-0-0-0 %lx  %lx %lx  %lx\n", pgd, make_ds_va(pgd,0,0,0), p4d_pfn(*p4dp), p4d_flags(*p4dp), (unsigned long)p4dp);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			for(unsigned long pud=0; pud<MAX; pud++) {
				if(get_pudp(p4dp, pud, &pudp) == 0) {
					size = sprintf(buf, "    %ld-%ld-0-0 %lx  %lx %lx  %lx\n", pgd, pud, make_ds_va(pgd,pud,0,0), pud_pfn(*pudp), pud_flags(*pudp), (unsigned long)pudp);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
			
            		for(unsigned long pmd=0; pmd<MAX; pmd++) {
						if(get_pmdp(pudp, pmd, &pmdp) == 0) {
							entry_count++;
							size = sprintf(buf, "        %ld-%ld-%ld-0 %lx  %lx %lx  %lx\n", pgd, pud, pmd, make_ds_va(pgd,pud,pmd,0), pmd_pfn(*pmdp), pmd_flags(*pmdp), (unsigned long)pmdp);
							kernel_write(file, buf, size, &pos);
							vfs_fsync_range(file, 0, size, 1);

							for(unsigned long pte=0; pte<MAX; pte++) {
	                   			if(get_ptep(pmdp, pte, &ptep) == 0) {
									size = sprintf(buf, "            %ld-%ld-%ld-%ld %lx  %lx %lx  %lx\n", pgd, pud, pmd, pte, make_ds_va(pgd,pud,pmd,pte), pte_pfn(*ptep), pte_flags(*ptep), (unsigned long)ptep);
									kernel_write(file, buf, size, &pos);
									vfs_fsync_range(file, 0, size, 1);
			                    }
			            	}
						}
		        	}
				}
	        }
		}
    }
	printk(KERN_INFO "user PT count: %d", entry_count);
	
	size = sprintf(buf, "user PT count: %d", entry_count);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);
end:	
	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}
EXPORT_SYMBOL_GPL(print_user_pgtable2);

// static long make_kernel_pgtable(void)
// {
// 	pte_t *ptep;
	
// 	int num;
// 	int count;
// 	int entry_count = 0;

// 	unsigned long pte_value;
// 	unsigned long pte_flag;
// 	unsigned long pte_num;
// 	unsigned long pte_num_pre = 0;

// 	struct file *file;
// 	char *filename = "./kernel_pgtable";
// 	int size;
// 	char *buf;
//         loff_t pos = 0;

// 	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
// 	if(IS_ERR(file)){
// 		printk("pre_file open err=%ld", PTR_ERR(file));
// 		goto end;
// 	}
	
//         buf = kmalloc(PATH_MAX, GFP_KERNEL);
//         if(!buf)
// 		goto end;
// 	memset(buf, '\0', 100);
	
	
// 	for(unsigned long a=USER_MAX; a<MAX; a++){
//         	for(unsigned long b=0; b<MAX; b++){
//             		for(unsigned long c=0; c<MAX; c++){
//                 		for(unsigned long d=0; d<MAX; d++){
//                     			if((num = search_pgtable_get_pfn(a, b, c, d, &ptep)) > 0 && num < 4){ //pte hit
// 						pte_num = make_ds_va(a, b, c, 0); // first entry num
// 						if(pte_num_pre != pte_num){
// 							entry_count++;
// 							pte_num_pre = pte_num;
// 						}
// 						pte_value = pte_pfn(*ptep);
// 						pte_flag = pte_flags(*ptep);

// 						size = sprintf(buf, "%ld-%ld-%ld-%ld  %lx %lx  %lx\n", a, b, c, d, pte_value, pte_flag, (unsigned long)ptep);
// 						kernel_write(file, buf, size, &pos);
// 						vfs_fsync_range(file, 0, size, 1);
						
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
// 	printk(KERN_INFO "kernel PT count: %d\n", entry_count);
	
// 	size = sprintf(buf, "kernel PT count: %d\n",entry_count);
// 	kernel_write(file, buf, size, &pos);
// 	vfs_fsync_range(file, 0, size, 1);
	
// 	kfree(buf);
// 	filp_close(file, NULL);
	
// 	return 0;
// }

// static long make_kernel_pgtable2(void)
// {
// 	pte_t *ptep;
	
// 	int num;
// 	int count;
// 	int entry_count = 0;

// 	unsigned long pte_value;
// 	unsigned long pte_flag;
// 	unsigned long pte_num;
// 	unsigned long pte_num_pre = 0;

// 	struct file *file;
// 	char *filename = "./kernel_pgtable2";
// 	int size;
// 	char *buf;
//         loff_t pos = 0;

// 	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
// 	if(IS_ERR(file)){
// 		printk("pre_file open err=%ld", PTR_ERR(file));
// 		goto end;
// 	}
	
//         buf = kmalloc(PATH_MAX, GFP_KERNEL);
//         if(!buf)
// 		goto end;
// 	memset(buf, '\0', 100);
	
	
// 	for(unsigned long a=USER_MAX; a<MAX; a++){
//         	for(unsigned long b=0; b<MAX; b++){
//             		for(unsigned long c=0; c<MAX; c++){
//                 		for(unsigned long d=0; d<MAX; d++){
//                     			if((num = search_pgtable_get_pfn(a, b, c, d, &ptep)) > 0 && num < 4){ //pte hit
// 						pte_num = make_ds_va(a, b, c, 0); // first entry num
// 						if(pte_num_pre != pte_num){
// 							entry_count++;
// 							pte_num_pre = pte_num;
// 						}
// 						pte_value = pte_pfn(*ptep);
// 						pte_flag = pte_flags(*ptep);

// 						size = sprintf(buf, "%ld-%ld-%ld-%ld  %lx %lx  %lx\n", a, b, c, d, pte_value, pte_flag, (unsigned long)ptep);
// 						kernel_write(file, buf, size, &pos);
// 						vfs_fsync_range(file, 0, size, 1);
						
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
// 	printk(KERN_INFO "kernel PT count: %d\n", entry_count);
	
// 	size = sprintf(buf, "kernel PT count: %d\n",entry_count);
// 	kernel_write(file, buf, size, &pos);
// 	vfs_fsync_range(file, 0, size, 1);
	
// 	kfree(buf);
// 	filp_close(file, NULL);
	
// 	return 0;
// }

SYSCALL_DEFINE0(mycall_print_user_pgtable){
	// return print_user_pgtable(target_task);
	return print_user_pgtable(current);
}

SYSCALL_DEFINE0(mycall_print_kernel_pgtable){
	long ret = 0;
	// ret = make_kernel_pgtable();
	return ret;
}

SYSCALL_DEFINE0(mycall_print_user_pgtable2){
	// return print_user_pgtable2(target_task);
	return print_user_pgtable2(current);
}

SYSCALL_DEFINE0(mycall_print_kernel_pgtable2){
	long ret = 0;
	// ret = make_kernel_pgtable2();
	return ret;
}
