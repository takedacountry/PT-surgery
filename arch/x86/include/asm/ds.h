#include <linux/types.h>

struct ds_list{
	unsigned long base;
	unsigned long limit;
	long offset;
	unsigned long flag;
	struct list_head list;
};

struct ds_head_list{
	pid_t pid;
	struct list_head ds_head;
	struct list_head head_list;
};

// struct ds_list_head{
// 	struct list_head usr_ds_list;
// 	struct list_head ker_ds_list;
// };

struct m_list{
	unsigned long va;
	unsigned long num;
	struct list_head list;
};

struct m_head_list{
	pid_t pid;
	struct list_head m_head;
	struct list_head head_list;
};
		
// struct m_list_head{
// 	struct list_head usr_m_list;
// 	struct list_head ker_m_list;
// };

// extern struct m_list_head *m_list;
// extern struct ds_list_head *ds_list;
extern struct list_head usr_m_head;
extern struct list_head ker_m_head;
extern struct list_head usr_ds_head;
extern struct list_head ker_ds_head;

extern void init_ds_list_head(void);
extern void init_m_list_head(void);
extern void free_list_head(void);

extern int make_pgd_m_list(unsigned long pgd_va);
extern int make_pud_m_list(unsigned long pgd_va, unsigned long pud_va);
extern int make_pmd_m_list(unsigned long pud_va, unsigned long pmd_va);
extern int make_pte_m_list(unsigned long pmd_va, unsigned long pte_va);
extern int make_usr_ds_list(unsigned long va, pte_t pte);

extern int make_ds_list(unsigned long address, pte_t *ptep);
