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


// void delete_ds(struct ds_list *itr, unsigned long start, unsigned long end)
// {
// 	struct ds_list *next;

// 	if(itr->base < start && end < itr->limit){ // base->start, end->limit
// 		// printk(KERN_INFO "    %lx %lx", itr->base, itr->limit);
// 		if((next = make_ds_node(end, itr->limit, itr->offset, itr->flag)) == NULL)
// 			goto out;
// 		itr->limit = start;
// 		list_add(&next->list, &itr->list);
// 		goto out;
// 	}

// 	while(end > itr->base){
// 		// printk(KERN_INFO "    %lx %lx", itr->base, itr->limit);
// 		next = list_next_entry(itr, list);
// 		if(itr->base < start && itr->limit <= end){ // base->start
// 			itr->limit = start;
// 		}else if(start <= itr->base && end < itr->limit){ // end->limit
// 			itr->base = end;
// 		}else if(start <= itr->base && itr->limit <= end){ // all delete
// 			list_del(&itr->list);
// 			kfree(itr);
// 		}
// 		itr = next;
// 	}
// out:
// 	return;
// }

void delete_m_free_pte(unsigned long va)
{
	// struct ds_list *ds_node;
	struct m_list *m_node;
	struct m_head_list *m_head;

	unsigned long va_start = 0, va_end = 0;

	read_lock(&user_head_lock);
	list_for_each_entry(m_head, &user_head, list) {
		if(m_head->pid == current->tgid) {
			// printk(KERN_INFO "delete m pte %lx", va);
			m_list_write_lock(m_head);
			list_for_each_entry(m_node, &m_head->head, list) {
				member_write_lock(m_node);
				if(m_node->base & PTE_FLAG_MASK && m_node->va <= va && va < m_node->va + OFFSET_SIZE) {
					va_start = m_node->base & PT_PGTABLE_MASK_NOT;
					va_end = va_start + PT_PGTABLE_SIZE;

					if(m_node->dup_pte) {
						pte_free(m_head->mm, virt_to_page(m_node->dup_pte));
						printk(KERN_INFO "delete dup PT\n");
						m_node->dup_pte = NULL;
					}

					member_write_unlock(m_node);
	
					delete_ds_all(m_node);
					delete_broken_pte_all(m_node);

					list_del(&m_node->list);
					kfree(m_node);
					printk(KERN_INFO "delete m pte %lx %lx-%lx pid %d %d\n", va, va_start, va_end, current->pid, current->tgid);
					break;
				}
				member_write_unlock(m_node);
			}
			m_list_write_unlock(m_head);
			break;
		}
	}
	read_unlock(&user_head_lock);
	return;
}
EXPORT_SYMBOL_GPL(delete_m_free_pte);

void delete_m_free_pmd(unsigned long va)
{
	struct m_list *m_node;
	struct m_head_list *m_head;

	read_lock(&user_head_lock);
	list_for_each_entry(m_head, &user_head, list){
		if(m_head->pid == current->tgid){
			// printk(KERN_INFO "delete m pmd %lx", va);
			m_list_write_lock(m_head);
			list_for_each_entry(m_node, &m_head->head, list){
				member_read_lock(m_node);
				if(m_node->base & PMD_FLAG_MASK && m_node->va <= va && va < m_node->va + OFFSET_SIZE){
					printk(KERN_INFO "delete m pmd %lx %lx pid %d %d\n", va, m_node->base, current->pid, current->tgid);
					member_read_unlock(m_node);
					list_del(&m_node->list);
					kfree(m_node);
					break;
				}
				member_read_unlock(m_node);
			}
			m_list_write_unlock(m_head);
			break;
		}
	}
	read_unlock(&user_head_lock);
	return;
}
EXPORT_SYMBOL_GPL(delete_m_free_pmd);

void delete_m_free_pud(unsigned long va)
{
	struct m_list *m_node;
	struct m_head_list *m_head;

	read_lock(&user_head_lock);
	list_for_each_entry(m_head, &user_head, list){
		if(m_head->pid == current->tgid){
			// printk(KERN_INFO "delete m pud %lx", va);
			m_list_write_lock(m_head);
			list_for_each_entry(m_node, &m_head->head, list) {
				member_read_lock(m_node);
				if(m_node->base & PUD_FLAG_MASK && m_node->va <= va && va < m_node->va + OFFSET_SIZE){
					printk(KERN_INFO "delete m pud %lx %lx pid %d %d\n", va, m_node->base, current->pid, current->tgid);
					member_read_unlock(m_node);
					list_del(&m_node->list);
					kfree(m_node);
					break;
				}
				member_read_unlock(m_node);
			}
			m_list_write_unlock(m_head);
			break;
		}
	}
	read_unlock(&user_head_lock);
	return;
}
EXPORT_SYMBOL_GPL(delete_m_free_pud);

void delete_m_free_pgd(unsigned long va)
{
	struct m_list *m_node;
	struct m_head_list *m_head;

	read_lock(&user_head_lock);
	list_for_each_entry(m_head, &user_head, list) {
		if(m_head->pid == current->tgid) {
			// printk(KERN_INFO "delete m pgd %lx", va);
			m_list_write_lock(m_head);
			list_for_each_entry(m_node, &m_head->head, list) {
				member_read_lock(m_node);
				if(m_node->base & PGD_FLAG_MASK && m_node->va <= va && va < m_node->va + OFFSET_SIZE) {
					if(list_is_last(&m_node->list, &m_head->head)) {
						printk(KERN_INFO "delete m pgd %lx %lx pid %d %d\n", va, PGD_FLAG_MASK, current->pid, current->tgid);
						member_read_unlock(m_node);
						list_del(&m_node->list);
						kfree(m_node);
						break;
					}
				}
				member_read_unlock(m_node);
			}
			m_list_write_unlock(m_head);

			if(list_empty(&m_head->head)) {
				list_del(&m_head->list);
				kfree(m_head);
				printk(KERN_INFO "delete m head %d %d\n", current->pid, current->tgid);
			}
			break;
		}
	}
	read_unlock(&user_head_lock);
	return;
}
EXPORT_SYMBOL_GPL(delete_m_free_pgd);

static long delete_m_all(void)
{
	struct m_list *mnode, *itr;
	struct m_head_list *mhead, *tmp; 

	read_lock(&user_head_lock);
	list_for_each_entry_safe(mhead, tmp, &user_head, list) {
		m_list_write_lock(mhead);
		list_for_each_entry_safe(mnode, itr, &mhead->head, list) {
			delete_ds_all(mnode);
			delete_broken_pte_all(mnode);
			list_del(&mnode->list);
			kfree(mnode);
		}
		m_list_write_unlock(mhead);
		list_del(&mhead->list);
		kfree(mhead);
	}
	printk(KERN_INFO "delete user all\n");
	read_unlock(&user_head_lock);
	return 0;
}

SYSCALL_DEFINE0(mycall_ds_m_delete)
{
	return delete_m_all();
}