#include <linux/syscalls.h>
#include <linux/export.h>
#include <asm/current.h>
#include <asm/page.h>
#include "pt_surgery.h"

#ifdef EMULATE_EMES_FOR_PTE
extern unsigned int user_recovery_cnt;
extern unsigned int kern_recovery_cnt;
extern unsigned int user_recovery_success_cnt;
extern unsigned int kern_recovery_success_cnt;
#endif

LIST_HEAD(user_head);
DEFINE_SPINLOCK(user_head_lock);

static int __make_pte_m_log(pmd_t *pmd, pte_t *pte, pid_t pid)
{
	struct page *pmd_page;
	struct page *pte_page;
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == pid) {
			pmd_page = virt_to_page((pmd_t *)(((unsigned long)pmd) & PAGE_MASK)); /* get pmd page */
			pte_page = virt_to_page((pte_t *)(((unsigned long)pte) & PAGE_MASK)); /* get pte page */
			if (!pmd_page->m_log || !(pmd_page->m_log->base & PMD_FLAG_MASK))
				break;

			if (!pte_page->m_log) 
				if (init_page_for_pt_surgery(pte_page) < 0)
					return -1;

			init_m_log(pte_page->m_log, make_ds_base_from_pmd((unsigned long)pmd, pmd_page->m_log->base));	/* update m_log in pte page */
		}
	}
	return 0;
}

int make_pte_m_log(pmd_t *pmd, pte_t *pte) 
{
	return __make_pte_m_log(pmd, pte, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pte_m_log);

static int __make_pmd_m_log(pud_t *pud, pmd_t *pmd, pid_t pid)
{
	struct page *pud_page;
	struct page *pmd_page;
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == pid) {
			pud_page = virt_to_page((pud_t *)(((unsigned long)pud) & PAGE_MASK)); /* get pud page */
			pmd_page = virt_to_page((pmd_t *)(((unsigned long)pmd) & PAGE_MASK)); /* get pmd page */
			if (!pud_page->m_log || !(pud_page->m_log->base & PUD_FLAG_MASK))
				break;

			if (!pmd_page->m_log) 
				if (init_page_for_pt_surgery(pmd_page) < 0)
					return -1;

			init_m_log(pmd_page->m_log, make_ds_base_from_pud((unsigned long)pud, pud_page->m_log->base));	/* update m_log in pmd page */
		}
	}
	return 0;
}

int make_pmd_m_log(pud_t *pud, pmd_t *pmd)
{
	return __make_pmd_m_log(pud, pmd, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pmd_m_log);

static int __make_pud_m_log(p4d_t *p4d, pud_t *pud, pid_t pid)
{
	struct page *p4d_page;
	struct page *pud_page;
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == pid) {
			p4d_page = virt_to_page((p4d_t *)(((unsigned long)p4d) & PAGE_MASK)); /* get p4d page */
			pud_page = virt_to_page((pud_t *)(((unsigned long)pud) & PAGE_MASK)); /* get pud page */
			if(!p4d_page->m_log || !(p4d_page->m_log->base & PGD_FLAG_MASK))
				break;

			if (!pud_page->m_log) 
				if (init_page_for_pt_surgery(pud_page) < 0)
					return -1;

			init_m_log(pud_page->m_log, make_ds_base_from_p4d((unsigned long)p4d));	/* update m_log in pud page */
		}
	}
	return 0;
}

int make_pud_m_log(p4d_t *p4d, pud_t *pud)
{
	return __make_pud_m_log(p4d, pud, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pud_m_log);

static int __make_pgd_m_log(pgd_t *pgd, pid_t pid)
{
	struct page *pgd_page;
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == pid) {
			pgd_page = virt_to_page((pgd_t *)(((unsigned long)pgd) & PAGE_MASK));	/* get pgd page */
			if (!pgd_page->m_log) 
				if (init_page_for_pt_surgery(pgd_page) < 0)
					return -1;

			init_m_log(pgd_page->m_log, PGD_FLAG_MASK);	/* update m_log in pgd page */
		}
	}
	return 0;
}

int make_pgd_m_log(pgd_t *pgd)
{
	return __make_pgd_m_log(pgd, current->tgid);
}
EXPORT_SYMBOL_GPL(make_pgd_m_log);

void destroy_pte_m_log(struct page *pte_page)
{
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
				return;
			
			delete_ds_log_all(pte_page);
			exit_m_log(pte_page->m_log);
			exit_page_for_pt_surgery(pte_page);
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(destroy_pte_m_log);

void destroy_pmd_m_log(struct page *pmd_page)
{
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			if (!pmd_page->m_log || !(pmd_page->m_log->base & PMD_FLAG_MASK))
				return;

			exit_m_log(pmd_page->m_log);
			exit_page_for_pt_surgery(pmd_page);
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(destroy_pmd_m_log);

void destroy_pud_m_log(struct page *pud_page)
{
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list){
		if(mhead->pid == current->tgid){
			if (!pud_page->m_log || !(pud_page->m_log->base & PUD_FLAG_MASK))
				return;

			exit_m_log(pud_page->m_log);
			exit_page_for_pt_surgery(pud_page);
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(destroy_pud_m_log);

void destroy_pgd_m_log(struct page *pgd_page)
{
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			if (!pgd_page->m_log || !(pgd_page->m_log->base & PGD_FLAG_MASK))
				return;

			if (!is_available_pgd(mhead->mm)) {
				exit_m_log(pgd_page->m_log);
				exit_page_for_pt_surgery(pgd_page);

				delete_m_head_struct_node(mhead);
				printk(KERN_INFO "exit pt surgery %d\n", current->tgid);
				#ifdef EMULATE_EMES_FOR_PTE
				printk(KERN_INFO "user recovery cnt: %u, user recovery success cnt: %u\n", user_recovery_cnt, user_recovery_success_cnt);
				printk(KERN_INFO "kern recovery cnt: %u, kern recovery success cnt: %u\n", kern_recovery_cnt, kern_recovery_success_cnt);
				#endif
			}
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(destroy_pgd_m_log);

static int get_pmdp_and_make_pte_m_log(pud_t *pudp, pid_t pid, unsigned long pmd, pmd_t **pmdpp)
{
  	pmd_t *pmdp = pmd_offset_index(pudp, pmd);
	*(pmdpp) = pmdp;

  	if(pmd_none(*pmdp) || !pmd_present(*pmdp) || pmd_large(*pmdp)){
    	return -1;
  	}

	__make_pte_m_log(pmdp, (pte_t *)pmd_page_vaddr(*pmdp), pid);
	
  	return 0;
}

static int get_pudp_and_make_pmd_m_log(p4d_t *p4dp, pid_t pid, unsigned long pud, pud_t **pudpp)
{
  	pud_t *pudp = pud_offset_index(p4dp, pud);
	*(pudpp) = pudp;
	
  	if(pud_none(*pudp) || !pud_present(*pudp) || pud_large(*pudp)){
	    return -1;
  	}

	__make_pmd_m_log(pudp, (pmd_t *)pud_pgtable(*pudp), pid);
	
  	return 0;  
}

static int get_p4dp_and_make_pud_m_log(pgd_t *pgdp, pid_t pid, unsigned long p4d, p4d_t **p4dpp)
{
  	p4d_t *p4dp = p4d_offset_index(pgdp, p4d);
	*(p4dpp) = p4dp;
	
	if(p4d_none(*p4dp) || !p4d_present(*p4dp)){
    	return -1;
  	}
	
	__make_pud_m_log(p4dp, (pud_t *)p4d_pgtable(*p4dp), pid);
	
	return 0;
}

static int get_pgdp_and_make_pud_m_log(struct mm_struct *mm, pid_t pid, unsigned long pgd, p4d_t **p4dpp)
{
  	pgd_t *pgdp = pgd_offset_index(mm, pgd);

  	if(pgd_none(*pgdp) || !pgd_present(*pgdp)){
    	return -1;
  	}

	if(get_p4dp_and_make_pud_m_log(pgdp, pid, pgd, p4dpp) < 0){
		return -1;
	}

  	return 0;
}

static long init_pt_surgery(struct task_struct *p)
{
	struct m_head_struct *mhead;
	struct mm_struct *mm = p->mm;
	struct page *pte_page;
	pid_t pid = p->tgid;
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep;
	
	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == pid) {
			printk(KERN_INFO "PT SURGERY have already registered %d\n",pid);
			return 0;
		}
	}

	if (add_m_head_struct_node(pid, mm, p) < 0) {
		printk(KERN_INFO "PT SURGERY INIT ERROR: cannot register pid %d\n",pid);
		goto err;
	}

	__make_pgd_m_log(mm->pgd, pid);
	for(unsigned long pgd=0; pgd < PGD_USER_MAX; pgd++) {
		if(get_pgdp_and_make_pud_m_log(mm, pid, pgd, &p4dp) == 0) {
			for(unsigned long pud=0; pud < PGD_KERN_MAX; pud++) {
				if(get_pudp_and_make_pmd_m_log(p4dp, pid, pud, &pudp) == 0) {
					for(unsigned long pmd=0; pmd < PGD_KERN_MAX; pmd++) {
						if(get_pmdp_and_make_pte_m_log(pudp, pid, pmd, &pmdp) == 0) {
							for(unsigned long pte=0; pte < PGD_KERN_MAX; pte++) {
			                	if(get_ptep(pmdp, pte, &ptep) == 0) {
									pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
									if(make_pte_ds_log_usr(pte_page, ptep, *ptep) < 0) {
										printk(KERN_INFO "PT SURGERY INIT ERROR: make pte ds log usr failure\n");
										goto err;
									}
			                    }
			            	}
						}
		        	}
				}
	    	}
		}
    }
	printk(KERN_INFO "init pt surgery %d\n",pid);

#ifdef EMULATE_EMES_FOR_PTE
	user_recovery_cnt = 0;
	kern_recovery_cnt = 0;
	user_recovery_success_cnt = 0;
	kern_recovery_success_cnt = 0;
#endif
	return 0;
err:
	return -1;
}

static void exit_pt_surgery(struct task_struct *p)
{
	struct m_head_struct *mhead;
	struct mm_struct *mm = NULL;
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == p->tgid) {
			mm = mhead->mm;
			break;
		}
	}

	if (!mm)
		return;
	
	for(unsigned long pgd=0; pgd < PGD_USER_MAX; pgd++) {
		if(get_pgdp(mm, pgd, &p4dp) == 0) {
			for(unsigned long pud=0; pud < PGD_KERN_MAX; pud++) {
				if(get_pudp(p4dp, pud, &pudp) == 0) {
					for(unsigned long pmd=0; pmd < PGD_KERN_MAX; pmd++) {
						if(get_pmdp(pudp, pmd, &pmdp) == 0) {
							destroy_pte_m_log(pmd_page(*pmdp));
						}
					}
					destroy_pmd_m_log(pud_page(*pudp));
				}
			}
			destroy_pud_m_log(p4d_page(*p4dp));
		}
	}
	destroy_pgd_m_log(virt_to_page((unsigned long)mm->pgd));
	printk(KERN_INFO "exit pt surgery %d\n", p->tgid);
#ifdef EMULATE_EMES_FOR_PTE
	printk(KERN_INFO "user recovery cnt: %u, user recovery success cnt: %u\n", user_recovery_cnt, user_recovery_success_cnt);
	printk(KERN_INFO "kern recovery cnt: %u, kern recovery success cnt: %u\n", kern_recovery_cnt, kern_recovery_success_cnt);
#endif
}

SYSCALL_DEFINE0(pt_surgery_register_pid)
{
	long ret = 0;
	if ((ret = init_pt_surgery(current)) < 0)
		exit_pt_surgery(current);
	return ret;
}

void pt_surgery_register_child(pid_t ppid, pid_t tgid, pid_t pid, struct task_struct *p)
{
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == tgid) { /* add thread info */
			add_thread_pt_op(mhead, pid);
			printk(KERN_INFO "make thread parent %d, process %d, thread %d\n", ppid, tgid, pid);
			return;
		}
	}

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == ppid) { /* register child process */
			if (init_pt_surgery(p) < 0)
				exit_pt_surgery(p);
			printk(KERN_INFO "make process parent %d, process %d, thread %d\n", ppid, tgid, pid);
			return;
		}
	}
	return;	
}
EXPORT_SYMBOL_GPL(pt_surgery_register_child);
