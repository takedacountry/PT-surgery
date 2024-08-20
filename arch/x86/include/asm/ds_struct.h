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
	struct list_head head;
	struct list_head list;
};

struct m_head_list{
	pid_t pid;
	struct list_head head;
	struct list_head list;
};

