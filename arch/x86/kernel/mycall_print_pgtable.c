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
			size = sprintf(buf, "%ld-0-0-0 %lx  %lx %lx  %lx\n", pgd, make_ds_base(pgd, 0, 0, 0), p4d_pfn(*p4dp), p4d_flags(*p4dp), (unsigned long)p4dp);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);

			for(unsigned long pud=0; pud<MAX; pud++) {
				if(get_pudp(p4dp, pud, &pudp) == 0) {
					size = sprintf(buf, "    %ld-%ld-0-0 %lx  %lx %lx  %lx\n", pgd, pud, make_ds_base(pgd, pud, 0, 0), pud_pfn(*pudp), pud_flags(*pudp), (unsigned long)pudp);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
			
		            for(unsigned long pmd=0; pmd<MAX; pmd++) {
						if(get_pmdp(pudp, pmd, &pmdp) == 0) {
							entry_count++;
							size = sprintf(buf, "        %ld-%ld-%ld-0 %lx  %lx %lx  %lx\n", pgd, pud, pmd, make_ds_base(pgd, pud, pmd, 0), pmd_pfn(*pmdp), pmd_flags(*pmdp), (unsigned long)pmdp);
							kernel_write(file, buf, size, &pos);
							vfs_fsync_range(file, 0, size, 1);

							for(unsigned long pte=0; pte<MAX; pte++) {
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
			size = sprintf(buf, "%ld-0-0-0 %lx  %lx %lx  %lx\n", pgd, make_ds_base(pgd,0,0,0), p4d_pfn(*p4dp), p4d_flags(*p4dp), (unsigned long)p4dp);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			for(unsigned long pud=0; pud<MAX; pud++) {
				if(get_pudp(p4dp, pud, &pudp) == 0) {
					size = sprintf(buf, "    %ld-%ld-0-0 %lx  %lx %lx  %lx\n", pgd, pud, make_ds_base(pgd,pud,0,0), pud_pfn(*pudp), pud_flags(*pudp), (unsigned long)pudp);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
			
            		for(unsigned long pmd=0; pmd<MAX; pmd++) {
						if(get_pmdp(pudp, pmd, &pmdp) == 0) {
							entry_count++;
							size = sprintf(buf, "        %ld-%ld-%ld-0 %lx  %lx %lx  %lx\n", pgd, pud, pmd, make_ds_base(pgd,pud,pmd,0), pmd_pfn(*pmdp), pmd_flags(*pmdp), (unsigned long)pmdp);
							kernel_write(file, buf, size, &pos);
							vfs_fsync_range(file, 0, size, 1);

							for(unsigned long pte=0; pte<MAX; pte++) {
	                   			if(get_ptep(pmdp, pte, &ptep) == 0) {
									size = sprintf(buf, "            %ld-%ld-%ld-%ld %lx  %lx %lx  %lx\n", pgd, pud, pmd, pte, make_ds_base(pgd,pud,pmd,pte), pte_pfn(*ptep), pte_flags(*ptep), (unsigned long)ptep);
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


long print_pgtable(struct mm_struct *mm, pid_t pid)
{
	// pgd_t *pgdp;
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep;
	
	int entry_count = 0;

	struct file *file;
	char *filename = "/usr/src/linux-6.1.35/result_kthread";
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

	size = sprintf(buf, "pid: %d\n", pid);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);

	for(unsigned long pgd=0; pgd<USER_MAX; pgd++) {
		if(get_pgdp(mm, pgd, &p4dp) == 0) {
			size = sprintf(buf, "%ld-0-0-0 %lx  %lx %lx  %lx\n", pgd, make_ds_base(pgd,0,0,0), p4d_pfn(*p4dp), p4d_flags(*p4dp), (unsigned long)p4dp);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			for(unsigned long pud=0; pud<MAX; pud++) {
				if(get_pudp(p4dp, pud, &pudp) == 0) {
					size = sprintf(buf, "    %ld-%ld-0-0 %lx  %lx %lx  %lx\n", pgd, pud, make_ds_base(pgd,pud,0,0), pud_pfn(*pudp), pud_flags(*pudp), (unsigned long)pudp);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
			
            		for(unsigned long pmd=0; pmd<MAX; pmd++) {
						if(get_pmdp(pudp, pmd, &pmdp) == 0) {
							entry_count++;
							size = sprintf(buf, "        %ld-%ld-%ld-0 %lx  %lx %lx  %lx\n", pgd, pud, pmd, make_ds_base(pgd,pud,pmd,0), pmd_pfn(*pmdp), pmd_flags(*pmdp), (unsigned long)pmdp);
							kernel_write(file, buf, size, &pos);
							vfs_fsync_range(file, 0, size, 1);

							for(unsigned long pte=0; pte<MAX; pte++) {
	                   			if(get_ptep(pmdp, pte, &ptep) == 0) {
									size = sprintf(buf, "            %ld-%ld-%ld-%ld %lx  %lx %lx  %lx\n", pgd, pud, pmd, pte, make_ds_base(pgd,pud,pmd,pte), pte_pfn(*ptep), pte_flags(*ptep), (unsigned long)ptep);
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
EXPORT_SYMBOL_GPL(print_pgtable);

static int print_usr_ds(struct task_struct *p)
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

	for(unsigned long pgd=0; pgd<USER_MAX; pgd++) {
		if(get_pgdp(p->mm, pgd, &p4dp) == 0) {
			pud_page = virt_to_page(pud_offset_index(p4dp, 0));
			if (!pud_page->m_log || !(pud_page->m_log->base & PUD_FLAG_MASK))
				return 0;
			size = sprintf(buf, "  pud: %lx\n", pud_page->m_log->base);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			for(unsigned long pud=0; pud<MAX; pud++) {
				if(get_pudp(p4dp, pud, &pudp) == 0) {
					pmd_page = virt_to_page(pmd_offset_index(pudp, 0));
					if (!pmd_page->m_log || !(pmd_page->m_log->base & PMD_FLAG_MASK))
						return 0;
					size = sprintf(buf, "    pmd: %lx\n",pmd_page->m_log->base);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
			
            		for(unsigned long pmd=0; pmd<MAX; pmd++) {
						if(get_pmdp(pudp, pmd, &pmdp) == 0) {
							pte_page = virt_to_page(pte_offset_index(pmdp, 0));
							if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
								return 0;
							size = sprintf(buf, "      pte: %lx\n", pte_page->m_log->base);
							kernel_write(file, buf, size, &pos);
							vfs_fsync_range(file, 0, size, 1);

							spin_lock(&pte_page->ds_lock);
							list_for_each_entry(dnode, &pte_page->ds_head, list) {
								size = sprintf(buf, "        %lx %lx %lx %lx\n", dnode->base, dnode->limit, dnode->offset, dnode->flag);
								kernel_write(file, buf, size, &pos);
								vfs_fsync_range(file, 0, size, 1);
								ds_count++;
							}
							spin_unlock(&pte_page->ds_lock);
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

static int print_usr_ds2(struct task_struct *p)
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
	char *filename = "./usr_ds_txt2";
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

	for(unsigned long pgd=0; pgd<USER_MAX; pgd++) {
		if(get_pgdp(p->mm, pgd, &p4dp) == 0) {
			pud_page = virt_to_page(pud_offset_index(p4dp, 0));
			if (!pud_page->m_log || !(pud_page->m_log->base & PUD_FLAG_MASK))
				return 0;
			size = sprintf(buf, "  pud: %lx\n", pud_page->m_log->base);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			for(unsigned long pud=0; pud<MAX; pud++) {
				if(get_pudp(p4dp, pud, &pudp) == 0) {
					pmd_page = virt_to_page(pmd_offset_index(pudp, 0));
					if (!pmd_page->m_log || !(pmd_page->m_log->base & PMD_FLAG_MASK))
						return 0;
					size = sprintf(buf, "    pmd: %lx\n",pmd_page->m_log->base);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
			
            		for(unsigned long pmd=0; pmd<MAX; pmd++) {
						if(get_pmdp(pudp, pmd, &pmdp) == 0) {
							pte_page = virt_to_page(pte_offset_index(pmdp, 0));
							if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
								return 0;
							size = sprintf(buf, "      pte: %lx\n", pte_page->m_log->base);
							kernel_write(file, buf, size, &pos);
							vfs_fsync_range(file, 0, size, 1);

							spin_lock(&pte_page->ds_lock);
							list_for_each_entry(dnode, &pte_page->ds_head, list) {
								size = sprintf(buf, "        %lx %lx %lx %lx\n", dnode->base, dnode->limit, dnode->offset, dnode->flag);
								kernel_write(file, buf, size, &pos);
								vfs_fsync_range(file, 0, size, 1);
								ds_count++;
							}
							spin_unlock(&pte_page->ds_lock);
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

static int print_usr_m(struct task_struct *p)
{
	// pgd_t *pgdp;
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;
	// pte_t *ptep;

	struct page *pgd_page, *pud_page, *pmd_page, *pte_page;
	
	struct file *file;
	char *filename = "./usr_m_txt";
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

	for(unsigned long pgd=0; pgd<USER_MAX; pgd++) {
		if(get_pgdp(p->mm, pgd, &p4dp) == 0) {
			pud_page = virt_to_page(pud_offset_index(p4dp, 0));
			if (!pud_page->m_log || !(pud_page->m_log->base & PUD_FLAG_MASK))
				return 0;
			size = sprintf(buf, "  pud: %lx\n", pud_page->m_log->base);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			for(unsigned long pud=0; pud<MAX; pud++) {
				if(get_pudp(p4dp, pud, &pudp) == 0) {
					pmd_page = virt_to_page(pmd_offset_index(pudp, 0));
					if (!pmd_page->m_log || !(pmd_page->m_log->base & PMD_FLAG_MASK))
						return 0;
					size = sprintf(buf, "    pmd: %lx\n",pmd_page->m_log->base);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
			
            		for(unsigned long pmd=0; pmd<MAX; pmd++) {
						if(get_pmdp(pudp, pmd, &pmdp) == 0) {
							pte_page = virt_to_page(pte_offset_index(pmdp, 0));
							if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
								return 0;
							size = sprintf(buf, "      pte: %lx\n", pte_page->m_log->base);
							kernel_write(file, buf, size, &pos);
							vfs_fsync_range(file, 0, size, 1);
						}
		        	}
				}
	        }
		}
    }

end:	
	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}

static int print_usr_m2(struct task_struct *p)
{
	// pgd_t *pgdp;
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;
	// pte_t *ptep;

	struct page *pgd_page, *pud_page, *pmd_page, *pte_page;
	
	struct file *file;
	char *filename = "./usr_m_txt2";
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

	for(unsigned long pgd=0; pgd<USER_MAX; pgd++) {
		if(get_pgdp(p->mm, pgd, &p4dp) == 0) {
			pud_page = virt_to_page(pud_offset_index(p4dp, 0));
			if (!pud_page->m_log || !(pud_page->m_log->base & PUD_FLAG_MASK))
				return 0;
			size = sprintf(buf, "  pud: %lx\n", pud_page->m_log->base);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			for(unsigned long pud=0; pud<MAX; pud++) {
				if(get_pudp(p4dp, pud, &pudp) == 0) {
					pmd_page = virt_to_page(pmd_offset_index(pudp, 0));
					if (!pmd_page->m_log || !(pmd_page->m_log->base & PMD_FLAG_MASK))
						return 0;
					size = sprintf(buf, "    pmd: %lx\n",pmd_page->m_log->base);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
			
            		for(unsigned long pmd=0; pmd<MAX; pmd++) {
						if(get_pmdp(pudp, pmd, &pmdp) == 0) {
							pte_page = virt_to_page(pte_offset_index(pmdp, 0));
							if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
								return 0;
							size = sprintf(buf, "      pte: %lx\n", pte_page->m_log->base);
							kernel_write(file, buf, size, &pos);
							vfs_fsync_range(file, 0, size, 1);
						}
		        	}
				}
	        }
		}
    }

end:	
	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}

static void count_up_m_ds(void)
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
		for(unsigned long pgd=0; pgd<USER_MAX; pgd++) {
			if(get_pgdp(mhead->mm, pgd, &p4dp) == 0) {
				m_num++;
				for(unsigned long pud=0; pud<MAX; pud++) {
					if(get_pudp(p4dp, pud, &pudp) == 0) {
						m_num++;
						for(unsigned long pmd=0; pmd<MAX; pmd++) {
							if(get_pmdp(pudp, pmd, &pmdp) == 0) {
								pte_page = virt_to_page(pte_offset_index(pmdp, 0));
								m_num++;
								pte_num++;
								spin_lock(&pte_page->ds_lock);
								list_for_each_entry(dnode, &pte_page->ds_head, list) {
									ds_num++;
								}
								spin_unlock(&pte_page->ds_lock);
							}
						}
					}
				}
			}
		}
		printk(KERN_INFO "pid %d m count %d, pte count %d, ds count %d\n", mhead->pid, m_num, pte_num, ds_num);
	}

	return;
}

SYSCALL_DEFINE0(mycall_print_user_pgtable){
	// return print_user_pgtable(target_task);
	return print_user_pgtable(current);
}

SYSCALL_DEFINE0(mycall_print_user_pgtable2){
	// return print_user_pgtable2(target_task);
	return print_user_pgtable2(current);
}

SYSCALL_DEFINE0(mycall_ds_search)
{
	print_usr_ds(current);
	// print_ker_ds();
	return 0;
}

SYSCALL_DEFINE0(mycall_ds_search2)
{
	print_usr_ds2(current);
	return 0;
}

SYSCALL_DEFINE0(mycall_m_search)
{
	print_usr_m(current);
	// print_ker_m();
	return 0;
}

SYSCALL_DEFINE0(mycall_m_search2)
{
	print_usr_m2(current);
	return 0;
}

SYSCALL_DEFINE0(mycall_m_ds_count)
{
	// count_up_m_ds(current);
	count_up_m_ds();
	return 0;
}
