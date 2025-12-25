#include <linux/syscalls.h>
#include <linux/export.h>
#include <linux/page_ref.h>
#include <asm/tlbflush.h>
#include <asm/current.h> 
#include <asm/page.h>
#include "pt_surgery.h"

#ifdef EMULATE_EMES_FOR_PTE
#include <linux/random.h>
#endif

static int active_replacement(struct mm_struct *mm, struct page *pte_page);

static int recover_broken_pgtable_kernel(struct page *pte_page, struct m_head_struct *mhead, unsigned long target_base)
{
	pte_t *ptep_new;
	int ret = 0;

	spin_lock(&pte_page->m_log->recovery_lock);
	if (!pte_page->m_log->replica) {
		ptep_new = pte_realloc(mhead->mm);
		if (!ptep_new) {
			spin_unlock(&pte_page->m_log->recovery_lock);	
			printk(KERN_INFO "KERN RECOVERY ERROR: malloc replica failed\n");
			ret = -1;
			goto end;
		}
		printk(KERN_INFO "replica %lx %lx %d in kernel\n", pte_page->m_log->base, (unsigned long)__pa(ptep_new), mhead->pid);
		if (restore_replica(target_base & PT_OFFSET_MASK_NOT, ptep_new, pte_page) < 0) {
			spin_unlock(&pte_page->m_log->recovery_lock);
			printk(KERN_INFO "KERN RECOVERY ERROR: restore replica failed\n");
			delete_broken_pte_log_all(pte_page->m_log);
			pte_free(mhead->mm, virt_to_page(ptep_new));
			ret = -1;
			goto end;
		} 
		
		pte_page->m_log->replica = ptep_new;
		spin_unlock(&pte_page->m_log->recovery_lock);

		/* goto lazy replacement in dec_page_ref_count */

		goto end;
	}
	spin_unlock(&pte_page->m_log->recovery_lock);
end:
	return ret;
}

static int recover_broken_pgtable_user(struct page *pte_page, struct m_head_struct *mhead, unsigned long target_base)
{
	pte_t *ptep_new; 
	int ret = 0;

	spin_lock(&pte_page->m_log->recovery_lock);
	if (!pte_page->m_log->replica) {
		ptep_new = pte_realloc(mhead->mm);
		if (!ptep_new) {
			spin_unlock(&pte_page->m_log->recovery_lock);
			printk(KERN_INFO "USER RECOVERY ERROR: malloc replica failed\n");
			ret = -1;
			goto end;
		}
		printk(KERN_INFO "replica %lx %lx %d in user\n", pte_page->m_log->base, (unsigned long)__pa(ptep_new), mhead->pid);
		if (restore_replica(target_base & PT_OFFSET_MASK_NOT, ptep_new, pte_page) < 0) {
			spin_unlock(&pte_page->m_log->recovery_lock);
			printk(KERN_INFO "USER RECOVERY ERROR: restore replica failed\n");
			delete_broken_pte_log_all(pte_page->m_log);
			pte_free(mhead->mm, virt_to_page(ptep_new));
			ret = -1;
			goto end;
		}
			
		pte_page->m_log->replica = ptep_new;
		spin_unlock(&pte_page->m_log->recovery_lock);

		/* goto active replacement */
		if (active_replacement(mhead->mm, pte_page) < 0){
			delete_broken_pte_log_all(pte_page->m_log);
			spin_lock(&pte_page->m_log->recovery_lock);
			pte_free(mhead->mm, virt_to_page(pte_page->m_log->replica));
			pte_page->m_log->replica = NULL;
			spin_unlock(&pte_page->m_log->recovery_lock);
			ret = -1;
		}
		goto end;
	}
	spin_unlock(&pte_page->m_log->recovery_lock);
end:
	return ret;
}

int handle_damaged_pte(unsigned long vaddr)
{
	struct page *pte_page;
	struct pte_access_log *anode;
	struct m_head_struct *mhead;
	unsigned long target_base = 0;
	int ret = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(vaddr & PAGE_MASK));
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
				return 0;

			target_base = make_ds_base_from_pte(vaddr, pte_page->m_log->base);
			if (is_broken_pte_log_node(target_base, pte_page->m_log) > 0)
				return 0;
			
			if (add_broken_pte_log_node(target_base, pte_page->m_log) < 0) 
				return 0;
			printk(KERN_INFO "register broken pte %lx %lx %d\n", pte_page->m_log->base, target_base, mhead->pid);
			
			list_for_each_entry(anode, &pte_page->m_log->access_head, list) {
				if (anode->pid == current->pid) {
					ret = recover_broken_pgtable_kernel(pte_page, mhead, target_base);
					goto end;
				}
			}
			ret = recover_broken_pgtable_user(pte_page, mhead, target_base);
			break;
		}
	}
end:
	return ret;
}

static int get_pmdp_for_recover_pgtable(struct mm_struct *mm, unsigned long base, pmd_t **pmdpp)
{
	p4d_t *p4dp;
	pud_t *pudp;

	unsigned long pgd = (base >> 27) & PT_OFFSET_MASK;
	unsigned long pud = (base >> 18) & PT_OFFSET_MASK;
	unsigned long pmd = (base >> 9) & PT_OFFSET_MASK;

  	if (get_pgdp(mm, pgd, &p4dp) == 0) {
		if (get_pudp(p4dp, pud, &pudp) == 0) {
			if (get_pmdp(pudp, pmd, pmdpp) == 0) {
				return 0;
			}
		}
	}
	return -1;
}

static unsigned long get_pte_vaddr_from_user_vaddr(struct mm_struct *mm, unsigned long uvaddr)
{
	pmd_t *pmdp;

	if(get_pmdp_for_recover_pgtable(mm, uvaddr >> PAGE_OFFSET_SHIFT, &pmdp) < 0)
		return 0;

	return (unsigned long)pte_offset_index(pmdp, 0);
}

SYSCALL_DEFINE1(pt_surgery_handle_damaged_pte, unsigned long, uvaddr)
{
	unsigned long kvaddr;
	if ((kvaddr = get_pte_vaddr_from_user_vaddr(current->mm, uvaddr)) == 0)
		return -1;
	return handle_damaged_pte(kvaddr);
}

int check_pte_is_broken_for_pte_write(pte_t *ptep)
{
	struct page *pte_page;
	struct broken_pte_log *bnode;
	struct m_head_struct *mhead;
	unsigned long target_base;
	pte_t entry;
	
	if (!ptep) { // NULL pointer
		return -1;
	}

	entry = *ptep; // check EMEs and register broken pte

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
#ifdef EMULATE_EMES_FOR_PTE
			unsigned long count;
			get_random_bytes(&count, sizeof(count));
			if (count % FAULT_RATIO == 0) {
				handle_damaged_pte((unsigned long)ptep);
			}
#endif
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
				break;

			spin_lock(&pte_page->m_log->recovery_lock);
			if (pte_page->m_log->replica) {
				target_base = make_ds_base_from_pte((unsigned long)ptep, pte_page->m_log->base);
				spin_lock(&pte_page->m_log->broken_lock);
				list_for_each_entry(bnode, &pte_page->m_log->broken_head, list) {
					if (target_base == bnode->base) {
						spin_unlock(&pte_page->m_log->broken_lock);
						spin_unlock(&pte_page->m_log->recovery_lock);
						return 1; // pte is broken
					}
				}
				spin_unlock(&pte_page->m_log->broken_lock);
				spin_unlock(&pte_page->m_log->recovery_lock);
				return 0; // pte is healthy
			}
			spin_unlock(&pte_page->m_log->recovery_lock);
			return 0; // PT is healthy
		}
	}
	return -1; // pte is not managed by ds_log
}
EXPORT_SYMBOL_GPL(check_pte_is_broken_for_pte_write);

pte_t check_pte_is_broken_for_pte_read(pte_t *ptep)
{
	struct page *pte_page;
	struct broken_pte_log *bnode;
	struct m_head_struct *mhead;
	unsigned long target_base;
	unsigned long offset;
	pte_t *pte;
	pte_t entry;

	if(!ptep) { // NULL pointer
		return native_make_pte(0);
	}

	entry = *ptep; // check EMEs and register broken pte

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
#ifdef EMULATE_EMES_FOR_PTE
			unsigned long count;
			get_random_bytes(&count, sizeof(count));
			if (count % FAULT_RATIO == 0) {
				handle_damaged_pte((unsigned long)ptep);
			}
#endif
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
				break;

			spin_lock(&pte_page->m_log->recovery_lock);
			if (pte_page->m_log->replica) {
				target_base = make_ds_base_from_pte((unsigned long)ptep, pte_page->m_log->base);
				spin_lock(&pte_page->m_log->broken_lock);
				list_for_each_entry(bnode, &pte_page->m_log->broken_head, list) {
					if (target_base == bnode->base) {
						offset = target_base & PT_OFFSET_MASK;
						pte = (pte_t *)pte_page->m_log->replica + offset;
						entry = *pte;
						break; // pte is broken
					}
				}
				spin_unlock(&pte_page->m_log->broken_lock);
			}
			spin_unlock(&pte_page->m_log->recovery_lock);
			break;
		}
	}
	return entry; // pte is safety or not managed by ds_log
}
EXPORT_SYMBOL_GPL(check_pte_is_broken_for_pte_read);

static int update_pmdp(struct mm_struct *mm, struct page *pte_page, pmd_t *pmdp)
{
	printk(KERN_INFO "pmd before: %lx %lx %d\n", pte_page->m_log->base, (unsigned long)pmd_val(*pmdp), current->tgid);

	spin_lock(&pte_page->m_log->recovery_lock);
	if (restore_page(pte_page, virt_to_page(pte_page->m_log->replica)) < 0) {
		spin_unlock(&pte_page->m_log->recovery_lock);
		return -1;	
	}
	pmd_reinstall(mm, pmdp, pte_page->m_log->replica);
	spin_unlock(&pte_page->m_log->recovery_lock);

	printk(KERN_INFO "pmd after:  %lx %lx %d\n",pte_page->m_log->base, (unsigned long)pmd_val(*pmdp), current->tgid);
	return 0;
}

static int active_replacement(struct mm_struct *mm, struct page *pte_page)
{
	pmd_t *pmdp;
	spinlock_t *ptl;
	unsigned long base = pte_page->m_log->base & PT_OFFSET_MASK_NOT;
	unsigned long addr = 0;
	int ret = 0;
	
	if(get_pmdp_for_recover_pgtable(mm, base, &pmdp) < 0) {
		printk(KERN_INFO "USER RECOVERY ERROR: pmdp is NULL\n");
		ret = -1;
		goto end;
	}

	ptl = ptlock_ptr(pmd_to_page(pmdp));
	spin_lock(ptl);
	for(;;) {
		if (page_count(pte_page) == 1) {
			if (update_pmdp(mm, pte_page, pmdp) < 0) {
				spin_unlock(ptl);
				printk(KERN_INFO "USER RECOVERY ERROR: failed to update pmdp\n");
				ret = -1;
				goto end;
			}
			break;
		}
		schedule();
	}
	spin_unlock(ptl);

	addr = base << PAGE_OFFSET_SHIFT;
	flush_tlb_mm_range(mm, addr, addr + PMD_SIZE, PAGE_OFFSET_SHIFT, false);

	exit_m_log(pte_page->m_log);
end:
	return ret;
}

static int lazy_replacement(struct mm_struct *mm, struct page *pte_page)
{
	pmd_t *pmdp;
	spinlock_t *ptl;
	unsigned long base = pte_page->m_log->base & PT_OFFSET_MASK_NOT;
	unsigned long addr = 0;
	int ret = 0;
	
	if (get_pmdp_for_recover_pgtable(mm, base, &pmdp) < 0) {
		printk(KERN_INFO "KERN RECOVERY ERROR: pmdp is NULL\n");
		ret = -1;
		goto end;
	}

	ptl = ptlock_ptr(pmd_to_page(pmdp));
	spin_lock(ptl);
	for(;;) {
		if (page_count(pte_page) == 1) {
			if (update_pmdp(mm, pte_page, pmdp) < 0) {
				spin_unlock(ptl);
				printk(KERN_INFO "KERN RECOVERY ERROR: failed to update pmdp\n");
				ret = -1;
				goto end;
			}
			break;
		}
		schedule();
	}
	spin_unlock(ptl);

	addr = base << PAGE_OFFSET_SHIFT;
	flush_tlb_mm_range(mm, addr, addr + PMD_SIZE, PAGE_OFFSET_SHIFT, false);

	exit_m_log(pte_page->m_log);
end:
	return ret;
}

void inc_page_ref_count_read(pte_t *ptep)
{
	struct page *pte_page;
	struct m_head_struct *mhead;
	int count = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
				goto end;

			count = dec_preempt_before_schedule();
			for (;;) {
				if (!pte_page->m_log->replica) {
					break;
				}
				schedule();
			}
			inc_preempt_after_schedule(count);

			page_ref_inc(pte_page);
			add_pte_access_log_node(current->pid, 0, pte_page->m_log);
			break;
		}
	}
end:
	return;
}
EXPORT_SYMBOL_GPL(inc_page_ref_count_read);

void inc_page_ref_count_write(pte_t *ptep)
{
	struct page *pte_page;
	struct m_head_struct *mhead;
	int count = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
				goto end;

			count = dec_preempt_before_schedule();
			for (;;) {
				if (!pte_page->m_log->replica) {
					break;
				}
				schedule();
			}
			inc_preempt_after_schedule(count);

			page_ref_inc(pte_page);
			add_pte_access_log_node(current->pid, 1, pte_page->m_log);
			break;
		}
	}
end:
	return;
}
EXPORT_SYMBOL_GPL(inc_page_ref_count_write);

void dec_page_ref_count(pte_t *ptep)
{
	struct page *pte_page; 
	struct m_head_struct *mhead;
	int ref_count = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK)) 
				goto end;

			ref_count = page_ref_dec_return(pte_page);
			delete_pte_access_log_node(current->pid, pte_page->m_log);
			if (pte_page->m_log->replica && ref_count == 1) {
				if (lazy_replacement(mhead->mm, pte_page) < 0) {
					delete_broken_pte_log_all(pte_page->m_log);
					spin_lock(&pte_page->m_log->recovery_lock);
					pte_free(mhead->mm, virt_to_page(pte_page->m_log->replica));
					pte_page->m_log->replica = NULL;
					spin_unlock(&pte_page->m_log->recovery_lock);
				}
			}
			break;
		}
	}
end:
	return;
}
EXPORT_SYMBOL_GPL(dec_page_ref_count);
