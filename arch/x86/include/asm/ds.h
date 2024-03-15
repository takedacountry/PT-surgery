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

struct m_list_head *m_list;
struct ds_list_head *ds_list;

void init_ds_list_head(void)
{
        ds_list = kmalloc(sizeof(struct ds_list_head), GFP_KERNEL);
        if(!ds_list)
                return;
        INIT_LIST_HEAD(&ds_list->usr_ds_list);
        INIT_LIST_HEAD(&ds_list->ker_ds_list);
        printk(KERN_INFO "init ds list head\n");
}

void init_m_list_head(void)
{
        m_list = kmalloc(sizeof(struct m_list_head), GFP_KERNEL);
        if(!m_list)
                return;
        INIT_LIST_HEAD(&m_list->usr_m_list);
        INIT_LIST_HEAD(&m_list->ker_m_list);
        printk(KERN_INFO "init m list head\n");
}

void free_list_head(void)
{
        kfree(ds_list);
        kfree(m_list);
}

int make_usr_list(unsigned long address, pte_t *ptep);
int make_ker_list(unsigned long address, pte_t *ptep);
int make_ds_list(unsigned long address, pte_t *ptep);
