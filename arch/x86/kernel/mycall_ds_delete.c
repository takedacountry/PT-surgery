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

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			if (pte_page->base & PTE_FLAG_MASK) {
				if (pte_page->dup_pt) {
					pte_free(mhead->mm, virt_to_page(pte_page->dup_pt));
					printk(KERN_INFO "delete dup PT\n");
					pte_page->dup_pt = NULL;
				}

				delete_ds_all(pte_page);

				// printk(KERN_INFO "delete m pte %lx pid %d %d\n", pte_page->base, current->pid, current->tgid);
				pte_page->base = 0;
			}
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(delete_pte_ds_log);

void delete_pmd_ds_log(struct page *pmd_page)
{
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			if (pmd_page->base & PMD_FLAG_MASK) {
				// printk(KERN_INFO "delete m pmd %lx pid %d %d\n", pmd_page->base, current->pid, current->tgid);
				pmd_page->base = 0;
			}
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(delete_pmd_ds_log);

void delete_pud_ds_log(struct page *pud_page)
{
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list){
		if(mhead->pid == current->tgid){
			if (pud_page->base & PUD_FLAG_MASK) {
				// printk(KERN_INFO "delete m pud %lx pid %d %d\n", pud_page->base, current->pid, current->tgid);
				pud_page->base = 0;
			}
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(delete_pud_ds_log);

extern bool is_available_pgd(struct mm_struct *mm);

void delete_pgd_ds_log(struct page *pgd_page)
{
	struct m_head_struct *mhead;
#ifdef CONFIG_RECOVERY_COUNT
	struct recovery_count *rcount;
#endif

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			if (pgd_page->base & PGD_FLAG_MASK) {
				if (!is_available_pgd(mhead->mm)) {
					// printk(KERN_INFO "delete m pgd %lx pid %d %d\n", PGD_FLAG_MASK, current->pid, current->tgid);
					pgd_page->base = 0;

					delete_broken_pte_all(mhead);
					mhead->pid = 0;
            		mhead->mm = NULL;
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
			}
			break;
		}
	}

	return;
}
EXPORT_SYMBOL_GPL(delete_pgd_ds_log);

static long delete_m_all(void)
{
	struct m_head_struct *mhead, *tmp; 

	list_for_each_entry_safe(mhead, tmp, &user_head, list) {
		delete_broken_pte_all(mhead);
		list_del(&mhead->list);
		kfree(mhead);
	}
	printk(KERN_INFO "delete user all\n");
	return 0;
}

SYSCALL_DEFINE0(mycall_ds_m_delete)
{
	return delete_m_all();
}