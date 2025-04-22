#include <linux/types.h>

#define PTE_READ_SHIFT		(0)
#define PTE_READ_MASK		(_AT(int, 1) << PTE_READ_SHIFT)
#define PTE_UPDATE_SHIFT	(1)
#define PTE_UPDATE_MASK		(_AT(int, 1) << PTE_UPDATE_SHIFT)
#define PTE__SHIFT		(2)
#define PTE__MASK		(_AT(int, 1) << PTE__SHIFT)

extern int make_pgd_m_list(pgd_t *pgd);
extern int make_pud_m_list(p4d_t *p4d, pud_t *pud);
extern int make_pmd_m_list(pud_t *pud, pmd_t *pmd);
extern int make_pte_m_list(pmd_t *pmd, pte_t *pte);

extern int make_ds_log_usr(pte_t *ptep, pte_t pte);

extern int clear_wrbit_ds_log(pte_t *ptep);
// extern int register_broken_pte_and_recover_broken_pgtable(unsigned long va);
// extern int register_broken_pte_and_make_recovery_thread(unsigned long va);
extern int check_pte_is_broken_for_pte_write(pte_t *ptep);
extern pte_t check_pte_is_broken_for_pte_read(pte_t *ptep);
extern int wait_to_recover_broken_pgtable(pmd_t *pmdp);
// extern int recover_broken_pte_from_pgtable_va(unsigned long va);

extern int increment_m_list_ref_count(pte_t *ptep);
extern int decrement_m_list_ref_count(pte_t *ptep);

extern void delete_pte_ds_log(struct page *pte_page);
extern void delete_pmd_ds_log(struct page *pmd_page);
extern void delete_pud_ds_log(struct page *pud_page);
extern void delete_pgd_ds_log(struct page *pgd_page);

extern bool check_parent_is_target(pid_t ppid, pid_t pid);
extern void register_child(struct task_struct *p);

extern long print_user_pgtable(struct task_struct *p);
extern long print_user_pgtable2(struct task_struct *p);
