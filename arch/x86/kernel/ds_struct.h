#include <linux/types.h>

struct ds_log{
	unsigned long base;
	unsigned long limit;
	long offset;
	unsigned long flag;
	struct list_head list;
};

struct ds_info{
	unsigned long base;			/* user addreess */ 
	void *dup_pt;				/* duplicate page table */
	struct list_head ds_head;	/* ds head */
	unsigned int recovery_state;/* recovery state: 00->fine, 01->waiting to replace, 10->replacing */
	spinlock_t recovery_lock;	/* recovery lock */
};

// struct broken_pte_list{
// 	unsigned int offset;
// 	struct list_head list;
// };

// struct m_list{
// 	unsigned long va;
// 	unsigned long base;
// 	pte_t *dup_pte;
// 	int ref_count;
// 	spinlock_t ref_lock;
// 	rwlock_t member_lock;
// 	rwlock_t ds_lock;
// 	rwlock_t broken_lock;
// 	struct list_head ds_head;
// 	struct list_head broken_head;
// 	struct list_head list;
// };

// struct m_head_list{
// 	pid_t pid;
// 	struct mm_struct *mm;
// 	rwlock_t m_lock;
// 	struct list_head head;
// 	struct list_head list;
// };

struct broken_pte_log{
	unsigned long base;
	struct list_head list;
};

struct m_head_struct{
	pid_t pid;
	struct mm_struct *mm;
	struct task_struct *krecoverd_task;
	spinlock_t krecoverd_lock;
	struct list_head head;
	struct list_head list;
};

struct recovery_count{
	pid_t pid;
	unsigned long kcount;
	unsigned long ksuccount;
	unsigned int ucount;
	unsigned int usuccount;
	struct list_head list;
};
