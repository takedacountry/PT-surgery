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
	struct list_head head;
	struct list_head list;
};
		
// struct m_list_head{
// 	struct list_head usr_m_list;
// 	struct list_head ker_m_list;
// };
