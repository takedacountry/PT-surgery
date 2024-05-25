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

static long make_ds_va(unsigned long a, unsigned long b, unsigned long c, unsigned long d)
{
	unsigned long va = a << 27 | b << 18 | c << 9 | d;
	return va;	
}

// static long make_user_pgtable(void)
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
// 	char *filename = "./user_pgtable";
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

// 	for(unsigned long a=0; a<USER_MAX; a++){
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
// 	printk(KERN_INFO "user PT count: %d", entry_count);
	
// 	size = sprintf(buf, "user PT count: %d", entry_count);
// 	kernel_write(file, buf, size, &pos);
// 	vfs_fsync_range(file, 0, size, 1);
	
// 	kfree(buf);
// 	filp_close(file, NULL);
	
// 	return 0;
// }

// static long make_user_pgtable2(void)
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
// 	char *filename = "./user_pgtable2";
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

// 	for(unsigned long a=0; a<USER_MAX; a++){
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
// 	printk(KERN_INFO "user PT count: %d", entry_count);
	
// 	size = sprintf(buf, "user PT count: %d", entry_count);
// 	kernel_write(file, buf, size, &pos);
// 	vfs_fsync_range(file, 0, size, 1);
	
// 	kfree(buf);
// 	filp_close(file, NULL);
	
// 	return 0;
// }

static int get_ptep(pmd_t *pmdp, unsigned long pte, pte_t **ptepp)
{
  	pte_t *ptep = pte_offset_index(pmdp, pte);
	*(ptepp) = ptep;

  	if(pte_none(*ptep) || !pte_present(*ptep)) {
    		// printk(KERN_INFO "pte %lu is not present.\n", pte);
    		return 0;
  	}
		
  	return 1;
}

static int get_pmdp(pud_t *pudp, unsigned long pmd, pmd_t **pmdpp)
{
  	pmd_t *pmdp = pmd_offset_index(pudp, pmd);
	*(pmdpp) = pmdp;

  	if(pmd_none(*pmdp) || !pmd_present(*pmdp) || pmd_large(*pmdp)){
    		// printk(KERN_INFO "pmd %lu is not present.\n", pmd);
    		return 0;
  	}

	return 1;
}

static int get_pudp(p4d_t *p4dp, unsigned long pud, pud_t **pudpp)
{
  	pud_t *pudp = pud_offset_index(p4dp, pud);
	*(pudpp) = pudp;
	  
  	if(pud_none(*pudp) || !pud_present(*pudp) || pud_large(*pudp)){
	    	// printk(KERN_INFO "pud %lu is not present", pud);
	    	return 0;
  	}

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

static int get_pgdp(struct mm_struct *mm, unsigned long pgd, p4d_t **p4dpp)
{
  	pgd_t *pgdp = pgd_offset_index(mm, pgd);

  	if(pgd_none(*pgdp) || !pgd_present(*pgdp)){
	    	// printk(KERN_INFO "pgd %lu is not present.\n", pgd);
    		return 0;
  	}

	if(get_p4dp(pgdp, pgd, p4dpp) > 0)
		return 1;
	
	return 0;
}


static long make_user_pgtable(void)
{
	// pgd_t *pgdp;
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep;
	
	int entry_count = 0;

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

	for(unsigned long pgd=0; pgd<USER_MAX; pgd++){
		if(get_pgdp(current->mm, pgd, &p4dp) > 0){
			
			// size = sprintf(buf, "%ld-0-0-0 %lx  %lx %lx  %lx\n", pgd, make_ds_va(pgd,0,0,0), p4d_pfn(*p4dp), p4d_flags(*p4dp), (unsigned long)p4dp);
			// kernel_write(file, buf, size, &pos);
			// vfs_fsync_range(file, 0, size, 1);
			
			for(unsigned long pud=0; pud<MAX; pud++){
				if(get_pudp(p4dp, pud, &pudp) > 0){

					// size = sprintf(buf, "    %ld-%ld-0-0 %lx  %lx %lx  %lx\n", pgd, pud, make_ds_va(pgd,pud,0,0), pud_pfn(*pudp), pud_flags(*pudp), (unsigned long)pudp);
					// kernel_write(file, buf, size, &pos);
					// vfs_fsync_range(file, 0, size, 1);
			
		            		for(unsigned long pmd=0; pmd<MAX; pmd++){
						if(get_pmdp(pudp, pmd, &pmdp) > 0){
							entry_count++;

							// size = sprintf(buf, "        %ld-%ld-%ld-0 %lx  %lx %lx  %lx\n", pgd, pud, pmd, make_ds_va(pgd,pud,pmd,0), pmd_pfn(*pmdp), pmd_flags(*pmdp), (unsigned long)pmdp);
							// kernel_write(file, buf, size, &pos);
							// vfs_fsync_range(file, 0, size, 1);

							for(unsigned long pte=0; pte<MAX; pte++){
			                    			if(get_ptep(pmdp, pte, &ptep) > 0){

									// size = sprintf(buf, "            %ld-%ld-%ld-%ld %lx  %lx %lx  %lx\n", pgd, pud, pmd, pte, make_ds_va(pgd,pud,pmd,pte), pte_pfn(*ptep), pte_flags(*ptep), (unsigned long)ptep);
									// kernel_write(file, buf, size, &pos);
									// vfs_fsync_range(file, 0, size, 1);
									
			                    			}
			                		}
						}
		            		}
				}
	        	}
		}
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
	memset(buf, '\0', 100);

	for(unsigned long pgd=0; pgd<USER_MAX; pgd++){
		if(get_pgdp(current->mm, pgd, &p4dp) > 0){
			
			size = sprintf(buf, "%ld-0-0-0 %lx  %lx %lx  %lx\n", pgd, make_ds_va(pgd,0,0,0), p4d_pfn(*p4dp), p4d_flags(*p4dp), (unsigned long)p4dp);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			for(unsigned long pud=0; pud<MAX; pud++){
				if(get_pudp(p4dp, pud, &pudp) > 0){

					size = sprintf(buf, "    %ld-%ld-0-0 %lx  %lx %lx  %lx\n", pgd, pud, make_ds_va(pgd,pud,0,0), pud_pfn(*pudp), pud_flags(*pudp), (unsigned long)pudp);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
			
		            		for(unsigned long pmd=0; pmd<MAX; pmd++){
						if(get_pmdp(pudp, pmd, &pmdp) > 0){
							entry_count++;

							size = sprintf(buf, "        %ld-%ld-%ld-0 %lx  %lx %lx  %lx\n", pgd, pud, pmd, make_ds_va(pgd,pud,pmd,0), pmd_pfn(*pmdp), pmd_flags(*pmdp), (unsigned long)pmdp);
							kernel_write(file, buf, size, &pos);
							vfs_fsync_range(file, 0, size, 1);

							for(unsigned long pte=0; pte<MAX; pte++){
			                    			if(get_ptep(pmdp, pte, &ptep) > 0){

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
end:
	printk(KERN_INFO "user PT count: %d", entry_count);
	
	size = sprintf(buf, "user PT count: %d", entry_count);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);
	
	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}

static long make_kernel_pgtable(void)
{
	pte_t *ptep;
	
	int num;
	int count;
	int entry_count = 0;

	unsigned long pte_value;
	unsigned long pte_flag;
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
                    			if((num = search_pgtable_get_pfn(a, b, c, d, &ptep)) > 0 && num < 4){ //pte hit
						pte_num = make_ds_va(a, b, c, 0); // first entry num
						if(pte_num_pre != pte_num){
							entry_count++;
							pte_num_pre = pte_num;
						}
						pte_value = pte_pfn(*ptep);
						pte_flag = pte_flags(*ptep);

						size = sprintf(buf, "%ld-%ld-%ld-%ld  %lx %lx  %lx\n", a, b, c, d, pte_value, pte_flag, (unsigned long)ptep);
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
	pte_t *ptep;
	
	int num;
	int count;
	int entry_count = 0;

	unsigned long pte_value;
	unsigned long pte_flag;
	unsigned long pte_num;
	unsigned long pte_num_pre = 0;

	struct file *file;
	char *filename = "./kernel_pgtable2";
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
                    			if((num = search_pgtable_get_pfn(a, b, c, d, &ptep)) > 0 && num < 4){ //pte hit
						pte_num = make_ds_va(a, b, c, 0); // first entry num
						if(pte_num_pre != pte_num){
							entry_count++;
							pte_num_pre = pte_num;
						}
						pte_value = pte_pfn(*ptep);
						pte_flag = pte_flags(*ptep);

						size = sprintf(buf, "%ld-%ld-%ld-%ld  %lx %lx  %lx\n", a, b, c, d, pte_value, pte_flag, (unsigned long)ptep);
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
