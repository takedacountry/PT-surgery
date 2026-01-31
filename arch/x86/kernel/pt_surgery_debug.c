#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <asm/current.h>
#include "pt_surgery.h"


static long print_user_pgtable(struct task_struct *p)
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

	for(unsigned long pgd=0; pgd<PGD_USER_MAX; pgd++) {
		if(get_pgdp(p->mm, pgd, &p4dp) == 0) {
			size = sprintf(buf, "%ld-0-0-0 %lx  %lx %lx  %lx\n", pgd, make_ds_base(pgd, 0, 0, 0), p4d_pfn(*p4dp), p4d_flags(*p4dp), (unsigned long)p4dp);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);

			for(unsigned long pud=0; pud<PGD_KERN_MAX; pud++) {
				if(get_pudp(p4dp, pud, &pudp) == 0) {
					size = sprintf(buf, "    %ld-%ld-0-0 %lx  %lx %lx  %lx\n", pgd, pud, make_ds_base(pgd, pud, 0, 0), pud_pfn(*pudp), pud_flags(*pudp), (unsigned long)pudp);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
			
		            for(unsigned long pmd=0; pmd<PGD_KERN_MAX; pmd++) {
						if(get_pmdp(pudp, pmd, &pmdp) == 0) {
							entry_count++;
							size = sprintf(buf, "        %ld-%ld-%ld-0 %lx  %lx %lx  %lx\n", pgd, pud, pmd, make_ds_base(pgd, pud, pmd, 0), pmd_pfn(*pmdp), pmd_flags(*pmdp), (unsigned long)pmdp);
							kernel_write(file, buf, size, &pos);
							vfs_fsync_range(file, 0, size, 1);

							for(unsigned long pte=0; pte<PGD_KERN_MAX; pte++) {
			                    if(get_ptep(pmdp, pte, &ptep) == 0) {
									size = sprintf(buf, "            %ld-%ld-%ld-%ld %lx  %lx %lx  %lx\n", pgd, pud, pmd, pte, make_ds_base(pgd, pud, pmd, pte), pte_pfn(*ptep), pte_flags(*ptep), (unsigned long)ptep);
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

static long print_ds_log_usr(struct task_struct *p)
{
	// pgd_t *pgdp;
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;
	// pte_t *ptep;

	struct page *pgd_page, *pud_page, *pmd_page, *pte_page;
	struct ds_log *dnode;

	int ds_count = 0;
	
	struct file *file;
	char *filename = "./usr_ds_txt";
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
	memset(buf, '\0', 256);

	size = sprintf(buf, "pid: %d\n", p->pid);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);

	pgd_page = virt_to_page(pgd_offset_index(p->mm, 0));
	if (!pgd_page->m_log || !(pgd_page->m_log->base & PGD_FLAG_MASK))
		return 0;
	size = sprintf(buf, "pgd: %lx\n", pgd_page->m_log->base);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);

	for(unsigned long pgd=0; pgd<PGD_USER_MAX; pgd++) {
		if(get_pgdp(p->mm, pgd, &p4dp) == 0) {
			pud_page = virt_to_page(pud_offset_index(p4dp, 0));
			if (!pud_page->m_log || !(pud_page->m_log->base & PUD_FLAG_MASK))
				return 0;
			size = sprintf(buf, "  pud: %lx\n", pud_page->m_log->base);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			for(unsigned long pud=0; pud<PGD_KERN_MAX; pud++) {
				if(get_pudp(p4dp, pud, &pudp) == 0) {
					pmd_page = virt_to_page(pmd_offset_index(pudp, 0));
					if (!pmd_page->m_log || !(pmd_page->m_log->base & PMD_FLAG_MASK))
						return 0;
					size = sprintf(buf, "    pmd: %lx\n",pmd_page->m_log->base);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
			
            		for(unsigned long pmd=0; pmd<PGD_KERN_MAX; pmd++) {
						if(get_pmdp(pudp, pmd, &pmdp) == 0) {
							pte_page = virt_to_page(pte_offset_index(pmdp, 0));
							if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
								return 0;
							size = sprintf(buf, "      pte: %lx\n", pte_page->m_log->base);
							kernel_write(file, buf, size, &pos);
							vfs_fsync_range(file, 0, size, 1);

							// spin_lock(&pte_page->ds_lock);
							list_for_each_entry(dnode, &pte_page->ds_head, list) {
								size = sprintf(buf, "        %lx %lx %lx %lx\n", dnode->base, dnode->limit, dnode->offset, dnode->flag);
								kernel_write(file, buf, size, &pos);
								vfs_fsync_range(file, 0, size, 1);
								ds_count++;
							}
							// spin_unlock(&pte_page->ds_lock);
						}
		        	}
				}
	        }
		}
    }
	size = sprintf(buf, "ds count: %d", ds_count);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);
end:	
	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}

static long count_ds_log_usr(void)
{
	struct m_head_struct *mhead;
	// pgd_t *pgdp;
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;
	// pte_t *ptep;

	struct page *pte_page;
	struct ds_log *dnode;
	
	list_for_each_entry(mhead, &user_head, list) {
		int ds_num = 0;
		int m_num = 0;
		int pte_num = 0;
		
		m_num++;
		for(unsigned long pgd=0; pgd<PGD_USER_MAX; pgd++) {
			if(get_pgdp(mhead->mm, pgd, &p4dp) == 0) {
				m_num++;
				for(unsigned long pud=0; pud<PGD_KERN_MAX; pud++) {
					if(get_pudp(p4dp, pud, &pudp) == 0) {
						m_num++;
						for(unsigned long pmd=0; pmd<PGD_KERN_MAX; pmd++) {
							if(get_pmdp(pudp, pmd, &pmdp) == 0) {
								pte_page = virt_to_page(pte_offset_index(pmdp, 0));
								m_num++;
								pte_num++;
								// spin_lock(&pte_page->ds_lock);
								list_for_each_entry(dnode, &pte_page->ds_head, list) {
									ds_num++;
								}
								// spin_unlock(&pte_page->ds_lock);
							}
						}
					}
				}
			}
		}
		printk(KERN_INFO "pid %d m count %d, pte count %d, ds count %d\n", mhead->pid, m_num, pte_num, ds_num);
	}

	return 0;
}

SYSCALL_DEFINE0(pt_surgery_print_user_pgtable)
{
	return print_user_pgtable(current);
}

SYSCALL_DEFINE0(pt_surgery_print_ds_log)
{
	return print_ds_log_usr(current);
}

SYSCALL_DEFINE0(pt_surgery_count_ds_log)
{
	return count_ds_log_usr();
}
