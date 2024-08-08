#include <linux/types.h>

// struct ds_list{
// 	unsigned long base;
// 	unsigned long limit;
// 	long offset;
// 	unsigned long flag;
// 	struct list_head list;
// };

// struct ds_head_list{
// 	pid_t pid;
// 	struct list_head head;
// 	struct list_head list;
// };

// // struct ds_list_head{
// // 	struct list_head usr_ds_list;
// // 	struct list_head ker_ds_list;
// // };

// struct m_list{
// 	unsigned long va;
// 	unsigned long num;
// 	struct list_head list;
// };

// struct m_head_list{
// 	pid_t pid;
// 	struct list_head head;
// 	struct list_head list;
// };
		
// // struct m_list_head{
// // 	struct list_head usr_m_list;
// // 	struct list_head ker_m_list;
// };

extern struct list_head usr_m_head;
extern struct list_head ker_m_head;
extern struct list_head usr_ds_head;
extern struct list_head ker_ds_head;

extern int make_pgd_m_list(unsigned long pgd_va);
extern int make_pud_m_list(unsigned long p4d_va, unsigned long pud_va);
extern int make_pmd_m_list(unsigned long pud_va, unsigned long pmd_va);
extern int make_pte_m_list(unsigned long pmd_va, unsigned long pte_va);

extern int make_ds_list_usr(unsigned long va, pte_t pte);

extern void delete_ds_m_free_pte(unsigned long va);
extern void delete_m_free_pmd(unsigned long va);
extern void delete_m_free_pud(unsigned long va);
extern void delete_m_free_pgd(unsigned long va);

extern bool check_parent_is_target(pid_t ppid, pid_t pid);
extern void register_child(struct task_struct *p);

extern void print_pte_addr(struct page *pte);

extern long make_user_pgtable(struct task_struct *p);
extern long make_user_pgtable2(struct task_struct *p);
