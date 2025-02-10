#include <linux/types.h>
#include <linux/rwlock_types.h>
#include <linux/spinlock_types.h>

struct ds_list{
	unsigned long base;
	unsigned long limit;
	long offset;
	unsigned long flag;
	struct list_head list;
};

// struct ds_head_list{
// 	pid_t pid;
// 	struct list_head head;
// 	struct list_head list;
// };

// struct pte_log_list{
// 	pte_t *ptep;		// for read. if EMEs occur, *ptep set from ds list.
// 	unsigned long base;	// pagetable address
// 	pte_t pte; 			// pte update
// 	int flag;			// pte update, or read, or free
// 	struct list_head list;
// };

// struct thread_log_list{
// 	pid_t tid;
// 	int ref;
// 	struct list_head head;
// 	struct list_head list;
// };

struct broken_pte_list{
	unsigned int offset;
	struct list_head list;
};

struct m_list{
	unsigned long va;
	unsigned long base;
	pte_t *dup_pte;
	int ref_count;
	spinlock_t ref_lock;
	rwlock_t member_lock;
	rwlock_t ds_lock;
	rwlock_t broken_lock;
	struct list_head ds_head;
	struct list_head broken_head;
	struct list_head list;
};

struct m_head_list{
	pid_t pid;
	rwlock_t m_lock;
	struct list_head head;
	struct list_head list;
};
