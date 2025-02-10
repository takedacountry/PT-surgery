#include <linux/types.h>

#define PTE_READ_SHIFT		(0)
#define PTE_READ_MASK		(_AT(int, 1) << PTE_READ_SHIFT)
#define PTE_UPDATE_SHIFT	(1)
#define PTE_UPDATE_MASK		(_AT(int, 1) << PTE_UPDATE_SHIFT)
#define PTE__SHIFT		(2)
#define PTE__MASK		(_AT(int, 1) << PTE__SHIFT)

extern int make_pgd_m_list(unsigned long pgd_va);
extern int make_pud_m_list(unsigned long p4d_va, unsigned long pud_va);
extern int make_pmd_m_list(unsigned long pud_va, unsigned long pmd_va);
extern int make_pte_m_list(unsigned long pmd_va, unsigned long pte_va);

extern int make_ds_list_usr(unsigned long va, pte_t pte);

extern int clear_wrbit_ds_list(unsigned long va);
extern int register_broken_pte_and_recover_broken_pgtable(unsigned long va);
extern int register_broken_pte_and_make_recovery_thread(unsigned long va);
extern int check_pte_is_broken_for_pte_write(pte_t *ptep);
extern pte_t check_pte_is_broken_for_pte_read(pte_t *ptep);
extern int wait_to_recover_broken_pgtable(unsigned long pmd_va);
// extern int recover_broken_pte_from_pgtable_va(unsigned long va);

extern int increment_m_list_ref_count(unsigned long va);
extern int decrement_m_list_ref_count(unsigned long va);

extern void delete_m_free_pte(unsigned long va);
extern void delete_m_free_pmd(unsigned long va);
extern void delete_m_free_pud(unsigned long va);
extern void delete_m_free_pgd(unsigned long va);

extern bool check_parent_is_target(pid_t ppid, pid_t pid);
extern void register_child(struct task_struct *p);

extern long print_user_pgtable(struct task_struct *p);
extern long print_user_pgtable2(struct task_struct *p);
