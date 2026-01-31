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

static int pt_replacement(struct mm_struct *mm, struct page *pte_page, pte_t *replica);

static int recover_pt_from_kwrite(struct page *pte_page, struct m_head_struct *mhead, pte_t *old_pt)
{
	pte_t *replica;

	replica = pte_realloc(mhead->mm);
	if (!replica) {
		printk(KERN_INFO "KERN RECOVERY ERROR: malloc replica failed\n");
		return 0;
	}
	printk(KERN_INFO "replica %lx %lx %d\n", pte_page->m_log->base, (unsigned long)__pa(replica), mhead->pid);
	if (restore_replica(pte_page->m_log->base & PT_OFFSET_MASK_NOT, replica, pte_page) < 0) {
		printk(KERN_INFO "KERN RECOVERY ERROR: restore replica failed\n");
		pte_free(mhead->mm, virt_to_page(replica));
		return 0;
	} 
	
	if (pt_replacement(mhead->mm, pte_page, replica) < 0) {
		printk(KERN_INFO "KERN RECOVERY ERROR: page table replacement failed\n");
		pte_free(mhead->mm, virt_to_page(replica));
		return 0;
	}

	WRITE_ONCE(pte_page->m_log->replica, replica);
	smp_wmb();
	WRITE_ONCE(pte_page->m_log->old_pt, old_pt);
	return 1;
}

static int recover_pt_from_kread(struct page *pte_page, struct m_head_struct *mhead, pte_t *old_pt)
{
	pte_t *replica;

	/* lazy replacement */

	replica = pte_realloc(mhead->mm);
	if (!replica) {
		printk(KERN_INFO "KERN RECOVERY ERROR: malloc replica failed\n");
		return 0;
	}
	printk(KERN_INFO "replica %lx %lx %d\n", pte_page->m_log->base, (unsigned long)__pa(replica), mhead->pid);
	if (restore_replica(pte_page->m_log->base & PT_OFFSET_MASK_NOT, replica, pte_page) < 0) {
		printk(KERN_INFO "KERN RECOVERY ERROR: restore replica failed\n");
		pte_free(mhead->mm, virt_to_page(replica));
		return 0;
	} 
	
	if (pt_replacement(mhead->mm, pte_page, replica) < 0){
		printk(KERN_INFO "KERN RECOVERY ERROR: page table replacement failed\n");
		pte_free(mhead->mm, virt_to_page(replica));
		return 0;
	}

	WRITE_ONCE(pte_page->m_log->replica, replica);
	smp_wmb();
	WRITE_ONCE(pte_page->m_log->old_pt, old_pt);
	return 1;
}

static int recover_pt_from_ptwalk(struct page *pte_page, struct m_head_struct *mhead, pte_t *old_pt)
{
	pte_t *replica; 

	/* lazy replacement */

	replica = pte_realloc(mhead->mm);
	if (!replica) {
		printk(KERN_INFO "USER RECOVERY ERROR: malloc replica failed\n");
		return 0;
	}
	printk(KERN_INFO "replica %lx %lx %d\n", pte_page->m_log->base, (unsigned long)__pa(replica), mhead->pid);
	if (restore_replica(pte_page->m_log->base & PT_OFFSET_MASK_NOT, replica, pte_page) < 0) {
		printk(KERN_INFO "USER RECOVERY ERROR: restore replica failed\n");
		pte_free(mhead->mm, virt_to_page(replica));
		return 0;
	}
		
	if (pt_replacement(mhead->mm, pte_page, replica) < 0){
		printk(KERN_INFO "USER RECOVERY ERROR: page table replacement failed\n");
		pte_free(mhead->mm, virt_to_page(replica));
		return 0;
	}

	WRITE_ONCE(pte_page->m_log->replica, replica);
	smp_wmb();
	WRITE_ONCE(pte_page->m_log->old_pt, old_pt);
	return 1;
}

int handle_damaged_pte(unsigned long vaddr)
{
	struct page *pte_page;
	struct m_head_struct *mhead;
	int ret = 0;
	int state;

	list_for_each_entry(mhead, &user_head, list) {
		if (mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(vaddr & PAGE_MASK));
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK)) // out of target for pt surgery
				return 0;

			state = atomic_cmpxchg(&pte_page->m_log->pt_state, PT_PAGE_SAFE, PT_PAGE_RECOVERING);
			if (state == PT_PAGE_RECOVERING) {
				// wait to recovery 
				// write??
				while(atomic_read(&pte_page->m_log->pt_state) == PT_PAGE_RECOVERING) {
					cpu_relax();
				}
				printk(KERN_INFO "pt page recovering %lx %d\n", pte_page->m_log->base, mhead->pid);
				return 1;
			}
			else if (state == PT_PAGE_SAFE) {
				enum pt_op_type op = READ_ONCE(current->pt_op);
				switch (op) {
				case PT_OP_WRITE:
					printk(KERN_INFO "recover pt form kwrite %lx %ld %d\n", pte_page->m_log->base, (vaddr & PAGE_OFFSET_MASK)/0x8, mhead->pid);
					ret = recover_pt_from_kwrite(pte_page, mhead, (pte_t *)(vaddr & PAGE_MASK));
					break;
				case PT_OP_READ:
					printk(KERN_INFO "recover pt form kread %lx %ld %d\n", pte_page->m_log->base, (vaddr & PAGE_OFFSET_MASK)/0x8, mhead->pid);
					ret = recover_pt_from_kread(pte_page, mhead, (pte_t *)(vaddr & PAGE_MASK));
					break;
				case PT_OP_NONE:
					printk(KERN_INFO "recover pt form ptwalk %lx %ld %d\n", pte_page->m_log->base, (vaddr & PAGE_OFFSET_MASK)/0x8, mhead->pid);
					ret = recover_pt_from_ptwalk(pte_page, mhead, (pte_t *)(vaddr & PAGE_MASK));
					break;
				}

				if (ret) { /* recovery successful */
					atomic_cmpxchg(&pte_page->m_log->pt_state, PT_PAGE_RECOVERING, PT_PAGE_RECOVERED);
				} else { /* recovery failed */
					atomic_cmpxchg(&pte_page->m_log->pt_state, PT_PAGE_RECOVERING, PT_PAGE_SAFE);
				}
			}
			else if (state == PT_PAGE_RECOVERED) {
				printk(KERN_INFO "pt page recovered %lx %d\n", pte_page->m_log->base, mhead->pid);
				return 1;
			}
			break;
		}
	}
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

void ensure_pte_wrprotect_safe(pte_t *ptep)
{
	struct page *pte_page;
	struct m_head_struct *mhead;
	
	if (!ptep)  // avoid NULL pointer dereference
		return;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (pte_page->m_log && (pte_page->m_log->base & PTE_FLAG_MASK))
				goto under_pt_surgery;
		}
	}

	clear_bit(_PAGE_BIT_RW, (unsigned long *)&ptep->pte);
	return; // out of target for pt_surgery

under_pt_surgery:
	if (pte_in_corrupted_pt(pte_page, ptep))
		goto wrprotect_redirection;

	WRITE_ONCE(current->pt_op, PT_OP_WRITE);
	clear_bit(_PAGE_BIT_RW, (unsigned long *)&ptep->pte); /* this code induces a memory error in real environments. */
#ifdef EMULATE_EMES_FOR_PTE
	unsigned long count;
	get_random_bytes(&count, sizeof(count));
	if (count % FAULT_RATIO == 0) {
		if(handle_damaged_pte((unsigned long)ptep) > 0) /* this code induces a memory error in emulating environments. */
			goto wrprotect_redirection;
	}
#endif
	WRITE_ONCE(current->pt_op, PT_OP_NONE);
	make_pte_ds_log_usr(pte_page, ptep, *ptep); // sync. DS-log
	return;

wrprotect_redirection:
	printk(KERN_INFO "redirect pte wrprotect\n");
	WRITE_ONCE(current->pt_op, PT_OP_NONE);
	redirect_pte_wrprotect(pte_page, ptep);
	return; // pte redirection 	
}
EXPORT_SYMBOL_GPL(ensure_pte_wrprotect_safe);

void ensure_pte_write_safe(pte_t *ptep, pte_t pte)
{
	struct page *pte_page;
	struct m_head_struct *mhead;
	
	if (!ptep)  // avoid NULL pointer dereference
		return;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (pte_page->m_log && (pte_page->m_log->base & PTE_FLAG_MASK))
				goto under_pt_surgery;
		}
	}

	WRITE_ONCE(*ptep, pte);
	return; // out of target for pt_surgery

under_pt_surgery:
	if (pte_in_corrupted_pt(pte_page, ptep))
		goto write_redirection;

	WRITE_ONCE(current->pt_op, PT_OP_WRITE);
	WRITE_ONCE(*ptep, pte); /* this code induces a memory error in real environments. */
#ifdef EMULATE_EMES_FOR_PTE
	unsigned long count;
	get_random_bytes(&count, sizeof(count));
	if (count % FAULT_RATIO == 0) {
		if(handle_damaged_pte((unsigned long)ptep) > 0) /* this code induces a memory error in emulating environments. */
			goto write_redirection;
	}
#endif
	WRITE_ONCE(current->pt_op, PT_OP_NONE);
	make_pte_ds_log_usr(pte_page, ptep, pte); // sync. DS-log
	return;

write_redirection:
	printk(KERN_INFO "redirect pte write\n");
	WRITE_ONCE(current->pt_op, PT_OP_NONE);
	redirect_pte_write(pte_page, ptep, pte);
	return; // pte redirection 	
}
EXPORT_SYMBOL_GPL(ensure_pte_write_safe);

pte_t ensure_pte_read_safe(pte_t *ptep)
{
	struct page *pte_page;
	struct m_head_struct *mhead;
	pte_t entry;

	if(!ptep) // avoid NULL pointer dereference
		return native_make_pte(0);

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (pte_page->m_log && (pte_page->m_log->base & PTE_FLAG_MASK))
				goto under_pt_surgery;
		}
	}

	return *ptep;

under_pt_surgery:
	if (pte_in_corrupted_pt(pte_page, ptep))
		goto read_redirection; // pte is in corrupted pt

	WRITE_ONCE(current->pt_op, PT_OP_READ);
	entry = *ptep; /* this code induces a memory error in real environments. */
#ifdef EMULATE_EMES_FOR_PTE
	unsigned long count;
	get_random_bytes(&count, sizeof(count));
	if (count % FAULT_RATIO == 0) {
		if(handle_damaged_pte((unsigned long)ptep) > 0) /* this code induces a memory error in emulating environments. */
			goto read_redirection;
	}
#endif
	WRITE_ONCE(current->pt_op, PT_OP_NONE);
	return entry;

read_redirection:
	printk(KERN_INFO "redirect pte read\n");
	WRITE_ONCE(current->pt_op, PT_OP_NONE);
	return redirect_pte_read(pte_page, ptep);
}
EXPORT_SYMBOL_GPL(ensure_pte_read_safe);

static int update_pmdp(struct mm_struct *mm, struct page *pte_page, pte_t *replica, pmd_t *pmdp)
{
	printk(KERN_INFO "pmd before: %lx %lx %d\n", pte_page->m_log->base, (unsigned long)pmd_val(*pmdp), current->tgid);
	if (restore_page(pte_page, virt_to_page(replica)) < 0) {
		return -1;	
	}
	pmd_reinstall(mm, pmdp, replica);
	printk(KERN_INFO "pmd after:  %lx %lx %d\n",pte_page->m_log->base, (unsigned long)pmd_val(*pmdp), current->tgid);
	return 0;
}

static int pt_replacement(struct mm_struct *mm, struct page *pte_page, pte_t *replica)
{
	pmd_t *pmdp;
	spinlock_t *ptl;
	unsigned long base = pte_page->m_log->base & PT_OFFSET_MASK_NOT;
	unsigned long addr = 0;
	int ret = 0;
	
	if(get_pmdp_for_recover_pgtable(mm, base, &pmdp) < 0) {
		printk(KERN_INFO "PAGE TABLE RECOVERY ERROR: pmdp is NULL\n");
		ret = -1;
		goto end;
	}

	ptl = ptlock_ptr(pmd_to_page(pmdp));
	spin_lock(ptl);
	if (update_pmdp(mm, pte_page, replica, pmdp) < 0) {
		spin_unlock(ptl);
		printk(KERN_INFO "PAGE TABLE RECOVERY ERROR: failed to update pmdp\n");
		ret = -1;
		goto end;
	}
	spin_unlock(ptl);

	addr = base << PAGE_OFFSET_SHIFT;
	flush_tlb_mm_range(mm, addr, addr + PMD_SIZE, PAGE_OFFSET_SHIFT, false);

	// exit_m_log(pte_page->m_log);
end:
	return ret;
}

void block_pt_acquire_under_recovery(pte_t *ptep)
{
	struct page *pte_page;
	struct m_head_struct *mhead;
	// int count = 0;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
				return;

			// count = dec_preempt_before_schedule();
			while(atomic_read(&pte_page->m_log->pt_state) == PT_PAGE_RECOVERING) {
				cpu_relax();
			}
			// inc_preempt_after_schedule(count);
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(block_pt_acquire_under_recovery);

void inc_writer_ref_count(pte_t *ptep)
{
	struct page *pte_page;
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK))
				return;

			page_ref_inc(pte_page);
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(inc_writer_ref_count);

void dec_writer_ref_count(pte_t *ptep)
{
	struct page *pte_page; 
	struct m_head_struct *mhead;

	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == current->tgid) {
			pte_page = virt_to_page((pte_t *)(((unsigned long)ptep) & PAGE_MASK));
			if (!pte_page->m_log || !(pte_page->m_log->base & PTE_FLAG_MASK)) 
				return;

			page_ref_dec(pte_page);
			break;
		}
	}
	return;
}
EXPORT_SYMBOL_GPL(dec_writer_ref_count);
