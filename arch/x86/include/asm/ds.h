struct ds_list{
	unsigned long base;
	unsigned long limit;
	long offset;
	unsigned long flag;
	struct list_head list;
};

// struct usr_ds_list_head{
// 	int id;
// 	struct list_head proc_list;
// };

struct ds_list_head{
	struct list_head usr_ds_list;
	struct list_head ker_ds_list;
};

struct m_list{
	unsigned long va;
	unsigned long num;
	struct list_head list;
};

// struct usr_m_list_head{
// 	int id;
// 	struct list_head proc_list;
// };
		
struct m_list_head{
	struct list_head usr_m_list;
	struct list_head ker_m_list;
};

// static LIST_HEAD(ds_list_head);
// static LIST_HEAD(m_list_head);

extern struct m_list_head *m_list;
extern struct ds_list_head *ds_list;

extern int make_usr_list(unsigned long address, pte_t *ptep);
extern int make_ker_list(unsigned long address, pte_t *ptep);
