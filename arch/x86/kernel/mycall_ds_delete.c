#include <linux/syscalls.h>
// #include <linux/types.h>
// #include <linux/mm.h>
// #include <linux/printk.h>
// #include <linux/list.h>
// #include <linux/slab.h>
#include <asm/current.h>
#include "ds.h"
// #include "ds_struct.h"
// #include <asm-generic/pgalloc.h>

void delete_pte_ds_log(struct page *pte_page)
{
	struct m_head_struct *mhead;
	unsigned long base = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
				return;
			
			base = pte_page->m_log->base;
			delete_ds_all(pte_page);
			exit_m_log(pte_page->m_log);
			exit_page_for_pt_surgery(pte_page);
			// printk(KERN_INFO "delete m pte %lx pid %d\n", base, current->tgid);
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(delete_pte_ds_log);

void delete_pmd_ds_log(struct page *pmd_page)
{
	struct m_head_struct *mhead;
	unsigned long base = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			if (!pmd_page->m_log || !(pmd_page->m_log->base & PMD_FLAG_MASK))
				return;

			base = pmd_page->m_log->base;
			exit_m_log(pmd_page->m_log);
			exit_page_for_pt_surgery(pmd_page);
			// printk(KERN_INFO "delete m pmd %lx pid %d\n", base, current->tgid);
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(delete_pmd_ds_log);

void delete_pud_ds_log(struct page *pud_page)
{
	struct m_head_struct *mhead;
	unsigned long base = 0;

	list_for_each_entry(mhead, &user_head, list){
		if(mhead->pid == current->tgid){
			if (!pud_page->m_log || !(pud_page->m_log->base & PUD_FLAG_MASK))
				return;

			base = pud_page->m_log->base;
			exit_m_log(pud_page->m_log);
			exit_page_for_pt_surgery(pud_page);
			// printk(KERN_INFO "delete m pud %lx pid %d\n", base, current->tgid);
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(delete_pud_ds_log);

void delete_pgd_ds_log(struct page *pgd_page)
{
	struct m_head_struct *mhead;
#ifdef CONFIG_RECOVERY_COUNT
	struct recovery_count *rcount;
#endif

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			if (!pgd_page->m_log || !(pgd_page->m_log->base & PGD_FLAG_MASK))
				return;

			if (!is_available_pgd(mhead->mm)) {
				exit_m_log(pgd_page->m_log);
				exit_page_for_pt_surgery(pgd_page);
				// printk(KERN_INFO "delete m pgd %lx pid %d\n", PGD_FLAG_MASK, current->tgid);

				mhead->pid = 0;
				mhead->mm = NULL;
				// mhead->kinfo = NULL;
				list_del(&mhead->list);
				kfree(mhead);
				printk(KERN_INFO "delete m head %d %d\n", current->pid, current->tgid);

#ifdef CONFIG_RECOVERY_COUNT
				list_for_each_entry(rcount, &count_head, list) {
					if (rcount->pid == current->tgid) {
						printk(KERN_INFO "kern recovery count %d: %ld / %ld\n", rcount->pid, rcount->ksuccount, rcount->kcount);
						printk(KERN_INFO "user recovery count %d: %d / %d\n", rcount->pid, rcount->usuccount, rcount->ucount);
						rcount->pid = 0;
						rcount->kcount = 0;
						rcount->ksuccount = 0;
						rcount->ucount = 0;
						rcount->usuccount = 0;
						list_del(&rcount->list);
						kfree(rcount);
						break;
					}
				}
#endif
			}
			break;
		}
	}

	return;
}
EXPORT_SYMBOL_GPL(delete_pgd_ds_log);
