#include <linux/kernel.h>
#include <linux/syscalls.h> //
// #include <linux/types.h>
#include <linux/fs.h>
// #include <linux/mm.h>
// #include <linux/list.h> //
// #include <linux/slab.h> //
#include <linux/err.h> //
#include <asm/current.h> //
#include "ds.h" //
// #include "ds_struct.h" //
// #include <asm/pgtable.h> //

// static int get_pmd_scan_pmd(pud_t *pudp, unsigned long pgd, unsigned long pud, unsigned long pmd, pmd_t **pmdpp)
// {
//   	pmd_t *pmdp = pmd_offset_index(pudp, pmd);
// 	*(pmdpp) = pmdp;

//   	if(pmd_none(*pmdp) || !pmd_present(*pmdp) || pmd_large(*pmdp)) {
//     		// printk(KERN_INFO "pmd %lu is not present.\n", pmd);
//     		return 4;
//   	}
// 	if(pgd < 0x100) // in user space
// 		return 1;
	
//   	return 2; // in kernel space
// }

// static int get_pmd_scan_pud(p4d_t *p4dp, unsigned long pgd, unsigned long pud, unsigned long pmd, pmd_t **pmdp)
// {
//   	pud_t *pudp = pud_offset_index(p4dp, pud);
	  
//   	if(pud_none(*pudp) || !pud_present(*pudp) || pud_large(*pudp)) {
// 	    	// printk(KERN_INFO "pud %lu is not present", pud);
// 	    	return 5;
//   	}
//   	return get_pmd_scan_pmd(pudp, pgd, pud, pmd, pmdp);  
// }

// static int get_pmd_scan_p4d(pgd_t *pgdp, unsigned long pgd, unsigned long pud, unsigned long pmd, pmd_t **pmdp)
// {
//   	p4d_t *p4dp = p4d_offset_index(pgdp, pgd);
	
// 	if(p4d_none(*p4dp) || !p4d_present(*p4dp)) {
// 	    	// printk(KERN_INFO "p4d %lu is not present", pgd);
//     		return 6;
//   	}
//   	return get_pmd_scan_pud(p4dp, pgd, pud, pmd, pmdp);
// }

// static int get_pmd_scan_pgd(struct mm_struct *mm, unsigned long pgd, unsigned long pud, unsigned long pmd, pmd_t **pmdp)
// {
//   	pgd_t *pgdp = pgd_offset_index(mm, pgd);

//   	if(pgd_none(*pgdp) || !pgd_present(*pgdp)) {
// 	    	// printk(KERN_INFO "pgd %lu is not present.\n", pgd);
//     		return 6;
//   	}
//   	return get_pmd_scan_p4d(pgdp, pgd, pud, pmd, pmdp);
// }

// static int search_pgtable_get_pmd(unsigned long base, pmd_t **pmdp)
// {
//   	struct mm_struct *mm = current->mm;
// 	// struct mm_struct *mm = target_task->mm;

// 	unsigned long pgd = (base >> 27) & PT_PGTABLE_MASK;
// 	unsigned long pud = (base >> 18) & PT_PGTABLE_MASK;
// 	unsigned long pmd = (base >> 9) & PT_PGTABLE_MASK;

//   	if(pgd<0 || 512<=pgd || pud<0 || 512<=pud || pmd<0 || 512<=pmd) {
//     		printk(KERN_INFO "error: The numbers are not appropriate.\n");
//     		return 0;
//   	}
//   	return get_pmd_scan_pgd(mm, pgd, pud, pmd, pmdp);
// }

// static int __recover_pgtable(unsigned long va_start, struct m_list *mnode, struct file *file, loff_t *pos)
// {
// 	pmd_t *pmdp;
// 	pte_t *ptep_old;
// 	pte_t *ptep_new;
// 	int num;

// 	int size;
// 	char *buf;
	
//     buf = kmalloc(PATH_MAX, GFP_KERNEL);
// 	memset(buf, '\0', 256);
		
// 	if((num = search_pgtable_get_pmd(va_start, &pmdp)) == 1){ // in user
// 		ptep_old = pte_offset_index(pmdp, 0);

// 		// printk(KERN_INFO "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
// 		size = sprintf(buf, "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
// 		kernel_write(file, buf, size, pos);
// 		vfs_fsync_range(file, 0, size, 1);
// 		// printk(KERN_INFO "pte before: %lx\n", (unsigned long)__pa(ptep_old));
// 		size = sprintf(buf, "pte before: %lx\n", (unsigned long)__pa(ptep_old));
// 		kernel_write(file, buf, size, pos);
// 		vfs_fsync_range(file, 0, size, 1);
		
// 		ptep_new = pte_realloc(current->mm);
// 		// ptep_new = pte_realloc(target_task->mm);
		
// 		if(!ptep_new){
// 			printk(KERN_INFO "out of memory\n");
// 			goto err;
// 		}
		
// 		update_dup_pgtable(va_start, ptep_new, mnode);
// 		member_write_lock(mnode);
// 		pmd_reinstall_lock(current->mm, pmdp, ptep_new);
// 		modify_m_va(mnode, (unsigned long)ptep_new);
// 		member_write_unlock(mnode);
// 		// pmd_reinstall_lock(target_task->mm, pmdp, ptep_new);

// 		// printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
// 		size = sprintf(buf, "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
// 		kernel_write(file, buf, size, pos);
// 		vfs_fsync_range(file, 0, size, 1);

// 		// printk(KERN_INFO "pte after:  %lx", (unsigned long)__pa(ptep_new));
// 		size = sprintf(buf, "pte after: %lx\n",(unsigned long)__pa(ptep_new));
// 		kernel_write(file, buf, size, pos);
// 		vfs_fsync_range(file, 0, size, 1);
// 	}
// 	else if(num == 2) { // in kernel
// 		// ptep_old = pte_offset_index(pmdp, 0);
		
// 		// // printk(KERN_INFO "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
// 		// size = sprintf(buf, "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
// 		// kernel_write(file, buf, size, pos);
// 		// vfs_fsync_range(file, 0, size, 1);
// 		// // printk(KERN_INFO "pte before: %lx\n", (unsigned long)__pa(ptep_old));
// 		// size = sprintf(buf, "pte before: %lx\n", (unsigned long)__pa(ptep_old));
// 		// kernel_write(file, buf, size, pos);
// 		// vfs_fsync_range(file, 0, size, 1);
		
// 		// ptep_new = pte_realloc_kernel();
		
// 		// if(!ptep_new){
// 		// 	printk(KERN_INFO "out of memory\n");
// 		// 	goto err;
// 		// }
		
// 		// update_dup_pgtable(va_start, ptep_new, mnode);
// 		// pmd_reinstall_kernel(pmdp, ptep_new);
// 		// modify_m_va(mnode, (unsigned long)ptep_new);
		
// 		// // printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
// 		// size = sprintf(buf, "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
// 		// kernel_write(file, buf, size, pos);
// 		// vfs_fsync_range(file, 0, size, 1);

// 		// // printk(KERN_INFO "pte after:  %lx", (unsigned long)__pa(ptep_new));
// 		// size = sprintf(buf, "pte after: %lx\n",(unsigned long)__pa(ptep_new));
// 		// kernel_write(file, buf, size, pos);
// 		// vfs_fsync_range(file, 0, size, 1);
// 	}
// 	else if(num == 0){
// 		goto err;
// 	}
// 	else{
// 		return num - 3;
// 	}
// 	return 0;
// err:
// 	return -1;
// }

static long recover_all_pgtable(void)
{
	// struct m_list *itr;
	// struct m_head_list *m_head;
	
	// unsigned long va_start;
	// unsigned long base;
	// int count = 0;
	int ret = 0;

// 	struct file *file;
// 	char *filename = "./write_log_txt";
//     loff_t pos = 0;

// 	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
// 	if(IS_ERR(file)) {
// 		printk("pre_file open err=%ld", PTR_ERR(file));
// 		return -1;
// 	}
	
// 	read_lock(&user_head_lock);
// 	list_for_each_entry(m_head, &user_head, list) {
// 		if(m_head->pid == current->tgid) {
// 		// if(m_head->pid == target_task->pid){
// 			m_list_read_lock(m_head);
// 			// printk(KERN_INFO "target_task pid: %d\n",target_task->pid);
// 			for(unsigned long a=0; a<USER_MAX; a++) {
// 		        for(unsigned long b=0; b<MAX; b++) {
// 		        	for(unsigned long c=0; c<MAX; c++) {
// 						va_start = make_ds_va(a, b, c, 0);
		
// 						list_for_each_entry(itr, &m_head->head, list) {
// 							member_read_lock(itr);
// 							base = itr->base & PT_PGTABLE_MASK_NOT;
// 							member_read_unlock(itr);
// 							if(base == va_start) {
// 								if((count = __recover_pgtable(va_start, itr, file, &pos)) < 0) {
// 									m_list_read_unlock(m_head);
// 									ret = -1;
// 									goto end;
// 								}
// 							}else if(va_start < base){
// 								break;
// 							}
// 						}
// 						if(--count > 0)
// 							break;
// 						count = 0;
// 					}
// 					if(--count > 0)
// 						break;
// 					count = 0;
// 				}
// 				if(--count > 0)
// 					break;
// 				count = 0;
// 			}
// 			m_list_read_unlock(m_head);
// 			break;
// 		}
// 	}
// end:
// 	read_unlock(&user_head_lock);
// 	filp_close(file, NULL);
	return ret;
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
	// struct m_list *itr;
	// struct m_head_list *m_head;

	// unsigned long base = va >> OFFSET_SHIFT;
	int ret = 0;
	
// 	struct file *file;
// 	char *filename = "./write_log_txt";
//     loff_t pos = 0;

// 	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
// 	if(IS_ERR(file)) {
// 		printk("pre_file open err=%ld", PTR_ERR(file));
// 		return -1;
// 	}
	
// 	read_lock(&user_head_lock);
// 	list_for_each_entry(m_head, &user_head, list) {
// 		if(m_head->pid == current->tgid) {
// 		// if(m_head->pid == target_task->pid) {
// 			m_list_read_lock(m_head);
// 			list_for_each_entry(itr, &m_head->head, list) {
// 				member_read_lock(itr);
// 				if(itr->base & PTE_FLAG_MASK && (itr->base & PT_PGTABLE_MASK_NOT) == (base & PT_PGTABLE_MASK_NOT)) {
// 					printk(KERN_INFO "recover pte found %lx %lx\n", itr->base, itr->va);
// 					member_read_unlock(itr);
// 					if(__recover_pgtable(base & PT_PGTABLE_MASK_NOT, itr, file, &pos) < 0) {
// 						m_list_read_unlock(m_head);
// 						printk(KERN_INFO "recover pte va:%lx error!\n",va);
// 						ret = -1;
// 						goto end;
// 					}
// 					break;
// 				}
// 				member_read_unlock(itr);
// 			}
// 			m_list_read_unlock(m_head);
// 			break;
// 		}
// 	}
// end:
// 	read_unlock(&user_head_lock);
// 	filp_close(file, NULL);
	return ret;
}

SYSCALL_DEFINE1(mycall_recover_pgtable, unsigned long, va)
{
	int ret = -1;
	printk(KERN_INFO "start recover pgtable %lx\n",va);
	ret = recover_pgtable(va);
	printk(KERN_INFO "end recover pgtable %lx\n",va);
	return ret;
}