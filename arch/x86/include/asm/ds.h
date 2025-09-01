#include <linux/types.h>

#define PGD_FLAG_SHIFT		(0)
#define PGD_FLAG_MASK		(_AT(long, 1) << PGD_FLAG_SHIFT)
#define P4D_FLAG_SHIFT		(1)
#define P4D_FLAG_MASK		(_AT(long, 1) << P4D_FLAG_SHIFT)
#define PUD_FLAG_SHIFT		(2)
#define PUD_FLAG_MASK		(_AT(long, 1) << PUD_FLAG_SHIFT)
#define PMD_FLAG_SHIFT		(3)
#define PMD_FLAG_MASK		(_AT(long, 1) << PMD_FLAG_SHIFT)
#define PTE_FLAG_SHIFT		(4)
#define PTE_FLAG_MASK		(_AT(long, 1) << PTE_FLAG_SHIFT)

extern struct list_head user_head;
extern int make_pgd_m_list(pgd_t *pgd);
extern int make_pud_m_list(p4d_t *p4d, pud_t *pud);
extern int make_pmd_m_list(pud_t *pud, pmd_t *pmd);
extern int make_pte_m_list(pmd_t *pmd, pte_t *pte);

extern int make_ds_log_usr(pte_t *ptep, pte_t pte);
extern int clear_wrbit_ds_log(pte_t *ptep);

// extern int register_broken_pte_and_recover_broken_pgtable(unsigned long pte_va);
extern int register_broken_pte_and_make_recovery_thread(unsigned long pte_va);
extern int check_pte_is_broken_for_pte_write(pte_t *ptep);
extern pte_t check_pte_is_broken_for_pte_read(pte_t *ptep);
extern int wait_to_recover_broken_pgtable(pmd_t *pmdp);

extern int inc_page_ref_count(pte_t *ptep);
extern int dec_page_ref_count(pte_t *ptep);

extern void delete_pte_ds_log(struct page *pte_page);
extern void delete_pmd_ds_log(struct page *pmd_page);
extern void delete_pud_ds_log(struct page *pud_page);
extern void delete_pgd_ds_log(struct page *pgd_page);

extern bool check_parent_is_target(pid_t ppid, pid_t pid);
extern void register_child(struct task_struct *p);

extern long print_user_pgtable(struct task_struct *p);
extern long print_user_pgtable2(struct task_struct *p);
extern long print_pgtable(struct mm_struct *mm, pid_t pid);

// extern void check_target_pid(void);