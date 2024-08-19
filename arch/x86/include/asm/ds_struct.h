#include <linux/types.h>

struct ds_list{
	unsigned long base;
	unsigned long limit;
	long offset;
	unsigned long flag;
	struct list_head list;
};

struct log_list{
	unsigned long base;
	pte_t pte;	// if update
	int flag;	// update, free, read
	int commit;
	struct list_head list;
};

struct m_list{
	unsigned long va;
	unsigned long base;
	struct list_head ds_head;
	struct list_head log_head;
	struct list_head list;
};

struct pid_list{
	pid_t pid;
	struct list_head head;
	struct list_head list;
};

