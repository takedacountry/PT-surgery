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
	struct list_head head;
	struct list_head list;
};

struct pte_log_list{
	unsigned long base; // for update
	pte_t pte; // if pte update
	int flag; // pte update, or read, or free
	struct list_head list;
};

struct thread_log_list{
	u32 cpu;
	int commit;
	struct list_head head;
	struct list_head list;
};

struct m_list{
	unsigned long va;
	unsigned long base;
	struct list_head head;
	struct list_head list;
};

struct m_head_list{
	pid_t pid;
	struct list_head head;
	struct list_head list;
};

