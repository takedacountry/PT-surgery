#include <linux/types.h>

struct ds_log {
	unsigned long base;
	unsigned long limit;
	long offset;
	unsigned long flag;
	struct list_head list;
};

struct broken_pte_log {
	unsigned long base;
	struct list_head list;
};

struct m_log {
	unsigned long base;			/* user address */ 
	void *replica;				/* page table replica */
	spinlock_t recovery_lock;	/* recovery lock */
	struct list_head head;		/* broken pte log per PTE */
	spinlock_t broken_lock;		/* lock for broken pte log */
	// unsigned int recovery_state;/* recovery state: 00->fine, 01->waiting to replace, 10->replacing */
};

// struct krecoverd_info{
// 	struct m_head_struct *mhead;
// 	struct page *page;
// 	struct task_struct *krecoverd_task;
// };

struct m_head_struct{
	pid_t pid;
	struct mm_struct *mm;
	// struct krecoverd_info *kinfo;
	// spinlock_t krecoverd_lock;
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
