#include <linux/types.h>

#define PTE_READ_SHIFT		(0)
#define PTE_READ_MASK		(_AT(int, 1) << PTE_READ_SHIFT)
#define PTE_UPDATE_SHIFT	(1)
#define PTE_UPDATE_MASK		(_AT(int, 1) << PTE_UPDATE_SHIFT)
#define PTE__SHIFT		(2)
#define PTE__MASK		(_AT(int, 1) << PTE__SHIFT)


// extern struct list_head usr_m_head;
// extern struct list_head ker_m_head;
// extern struct list_head usr_ds_head;
// extern struct list_head ker_ds_head;
extern struct list_head user_head;
extern struct list_head kern_head;

extern int make_pgd_m_list(unsigned long pgd_va);
extern int make_pud_m_list(unsigned long p4d_va, unsigned long pud_va);
extern int make_pmd_m_list(unsigned long pud_va, unsigned long pmd_va);
extern int make_pte_m_list(unsigned long pmd_va, unsigned long pte_va);

extern int make_ds_list_usr(unsigned long va, pte_t pte);

extern int make_thread_log_list_usr(unsigned long va);
extern int make_pte_log_list_usr(unsigned long va, pte_t pte, int flag);
extern int delete_thread_log_list_usr(unsigned long va);
extern int delete_pte_log_list_usr(unsigned long va, int flag);

extern void delete_ds_m_free_pte(unsigned long va);
extern void delete_m_free_pmd(unsigned long va);
extern void delete_m_free_pud(unsigned long va);
extern void delete_m_free_pgd(unsigned long va);

extern bool check_parent_is_target(pid_t ppid, pid_t pid);
extern void register_child(struct task_struct *p);

extern void print_pte_addr(struct page *pte);

extern long make_user_pgtable(struct task_struct *p);
extern long make_user_pgtable2(struct task_struct *p);

extern void print_native_pte_clear(pte_t *ptep);
extern void print_pte_clear(pte_t *ptep);
