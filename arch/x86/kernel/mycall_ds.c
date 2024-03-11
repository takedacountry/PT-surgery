#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <asm/current.h>
#include <asm/io.h>
#include <asm/ds.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/page_types.h>
#include <asm/pgtable_types.h>
#include <asm/paravirt.h>
#include <asm-generic/pgalloc.h>
#include <asm-generic/barrier.h>
#include <asm-generic/memory_model.h>

#define USER_MAX 		0x100
#define MAX 			0x200
#define PT_PGTABLE_SHIFT 	9
#define PT_PGTABLE_SIZE		(_AT(long, 1) << PT_PGTABLE_SHIFT)
#define PT_PGTABLE_MASK		(PT_PGTABLE_SIZE - 1)
#define PT_PGTABLE_MASK_NOT	(~PT_PGTABLE_MASK)
#define SAME_ADDR_SHIFT 	16
#define SAME_ADDR_MASK 		(_AT(long, 1) << SAME_ADDR_SHIFT)
#define SAME_ADDR_MASK_NOT 	(~(SAME_ADDR_MASK))
#define HIT_FLAG_SHIFT 		0
#define CONTI_FLAG_SHIFT 	1
#define SAME_FLAG_SHIFT 	2
#define HIT_FLAG_MASK 		(_AT(int, 1) << HIT_FLAG_SHIFT)
#define CONTI_FLAG_MASK 	(_AT(int, 1) << CONTI_FLAG_SHIFT)
#define SAME_FLAG_MASK		(_AT(int, 1) << SAME_FLAG_SHIFT)
#define HIT_FLAG_MASK_NOT 	(~(HIT_FLAG_MASK))
#define CONTI_FLAG_MASK_NOT 	(~(CONTI_FLAG_MASK))
#define SAME_FLAG_MASK_NOT 	(~(SAME_FLAG_MASK))

unsigned long vaddr;
// extern struct ds_list_head *ds_list;
// extern struct m_list_head *m_list;

static pte_t *pte_offset_index(pmd_t *pmd, unsigned long index)
{
	return (pte_t *)pmd_page_vaddr(*pmd) + index;
}

static pmd_t *pmd_offset_index(pud_t *pud, unsigned long index)
{
	return pud_pgtable(*pud) + index;
}

static pud_t *pud_offset_index(p4d_t *p4d, unsigned long index)
{
	return p4d_pgtable(*p4d) + index;
}

static p4d_t *p4d_offset_index(pgd_t *pgd, unsigned long index)
{
	if(!pgtable_l5_enabled())
    		return (p4d_t *)pgd;
	printk(KERN_INFO "pagetable level 5");
  	return (p4d_t *)pgd_page_vaddr(*pgd) + index;
}

static pgd_t *pgd_offset_index(struct mm_struct *mm, unsigned long index)
{
  	return mm->pgd + index;
}


static int get_pfn_scan_pte(pmd_t *pmdp, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
{
  	pte_t *ptep = pte_offset_index(pmdp, pte);
	*(ptepp) = ptep;

  	if(pte_none(*ptep) || !pte_present(*ptep)) {
    		// printk(KERN_INFO "pte %lu is not present.\n", pte);
    		return 4;
  	}
		
  	return 1;
}

static int get_pfn_scan_pmd(pud_t *pudp, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
{
  	pmd_t *pmdp = pmd_offset_index(pudp, pmd);

  	if(pmd_none(*pmdp) || !pmd_present(*pmdp) || pmd_large(*pmdp)){
    		// printk(KERN_INFO "pmd %lu is not present.\n", pmd);
    		return 5;
  	}
  // 	if(pmd_large(*pmdp)){
  //   		pte_value = pmd_pfn(*pmdp);
		// pte_flag = pmd_flags(*pmdp);
  //   		return 2;
  // 	}
  	return get_pfn_scan_pte(pmdp, pgd, pud, pmd, pte, ptepp);
}

static int get_pfn_scan_pud(p4d_t *p4dp, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
{
  	pud_t *pudp = pud_offset_index(p4dp, pud);
	  
  	if(pud_none(*pudp) || !pud_present(*pudp) || pud_large(*pudp)){
	    	// printk(KERN_INFO "pud %lu is not present", pud);
	    	return 6;
  	}
  // 	if(pud_large(*pudp)){
  //   		pte_value = pud_pfn(*pudp);
		// pte_flag = pud_flags(*pudp);
  //   		return 3;
  // 	}
  	return get_pfn_scan_pmd(pudp, pgd, pud, pmd, pte, ptepp);  
}

static int get_pfn_scan_p4d(pgd_t *pgdp, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
{
  	p4d_t *p4dp = p4d_offset_index(pgdp, pgd);
	
	if(p4d_none(*p4dp) || !p4d_present(*p4dp)){
	    	// printk(KERN_INFO "p4d %lu is not present", pgd);
    		return 7;
  	}
  	return get_pfn_scan_pud(p4dp, pgd, pud, pmd, pte, ptepp);
}

static int get_pfn_scan_pgd(struct mm_struct *mm, unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
{
  	pgd_t *pgdp = pgd_offset_index(mm, pgd);

  	if(pgd_none(*pgdp) || !pgd_present(*pgdp)){
	    	// printk(KERN_INFO "pgd %lu is not present.\n", pgd);
    		return 7;
  	}
  	return get_pfn_scan_p4d(pgdp, pgd, pud, pmd, pte, ptepp);
}

static int search_pgtable_get_pfn(unsigned long pgd, unsigned long pud, unsigned long pmd, unsigned long pte, pte_t **ptepp)
{
  	struct mm_struct *mm = current->mm;

  	if(pgd<0 || 512<=pgd || pud<0 || 512<=pud || pmd<0 || 512<=pmd || pte<0 || 512<=pte) {
    		printk(KERN_INFO "error: The numbers are not appropriate.\n");
    		return 0;
  	}
  	return get_pfn_scan_pgd(mm, pgd, pud, pmd, pte, ptepp);
}

static long make_ds_va(unsigned long a, unsigned long b, unsigned long c, unsigned long d)
{
	unsigned long va = a << 27 | b << 18 | c << 9 | d;
	return va;	
}

static long make_ds_offset(long base, unsigned long pte_value)
{
	long offset = base - pte_value;
	return offset;
}

static struct ds_list *make_ds_node(unsigned long base, unsigned long limit, long offset, unsigned long flag)
{
	struct ds_list *list = kmalloc(sizeof(struct ds_list), GFP_KERNEL);
	if(!list)
		return NULL;

	list->base = base;
	list->limit = limit;
	list->offset = offset;
	list->flag = flag;
	// list_add_tail(&list->list, &ds_list->usr_ds_list);
	return list;
}

static struct m_list *make_m_node(unsigned long va, unsigned long num)
{
	struct m_list *list = kmalloc(sizeof(struct m_list), GFP_KERNEL);
	if(!list)
		return NULL;

	list->va = va & PAGE_MASK;
	list->num = num;
	// list_add_tail(&list->list, &m_list->usr_m_list);
	return list;
}

static bool is_ds_node_merge(struct ds_list *prev, struct ds_list *next)
{
	if(prev->limit == next->base && prev->offset == next->offset && prev->flag == next->flag)
		return true;
	else
		return false;
}

static void ds_node_merge(struct ds_list *prev, struct ds_list *next)
{
	if(is_ds_node_merge(prev, next)){
		prev->limit = next->limit;
		list_del(&next->list);
		kfree(next);
	}
}

static bool is_add_usr_m_node(unsigned long num)
{
	struct m_list *itr;
	
	list_for_each_entry(itr, &m_list->usr_m_list, list){
		if(itr->num == num)
			return false;
		if(num < itr->num)
			break;
	}
	return true;
}

static int add_usr_m_node(unsigned long va, unsigned long num)
{
	struct m_list *mnode, *itr;

	if((mnode = make_m_node(va, num)) == NULL)
		return -ENOMEM;

	if(list_empty(&m_list->usr_m_list)){ //no node
		list_add(&mnode->list, &m_list->usr_m_list);
	}else{
		list_for_each_entry(itr, &m_list->usr_m_list, list){
			if(num < itr->num){
				list_add_tail(&mnode->list, &itr->list);
				return 0;
			}
		}
		list_add_tail(&mnode->list, &m_list->usr_m_list);
	}
	return 0;
}

static bool is_add_ker_m_node(unsigned long num)
{
	struct m_list *itr;
	
	list_for_each_entry(itr, &m_list->ker_m_list, list){
		if(itr->num == num)
			return false;
		if(num < itr->num)
			break;
	}
	return true;
}

static int add_ker_m_node(unsigned long va, unsigned long num)
{
	struct m_list *mnode, *itr;

	if((mnode = make_m_node(va, num)) == NULL)
		return -ENOMEM;

	if(list_empty(&m_list->ker_m_list)){ //no node
		list_add(&mnode->list, &m_list->ker_m_list);
	}else{
		list_for_each_entry(itr, &m_list->ker_m_list, list){
			if(num < itr->num){
				list_add_tail(&mnode->list, &itr->list);
				return 0;
			}
		}
		list_add_tail(&mnode->list, &m_list->ker_m_list);
	}
	return 0;
}

int make_usr_list(unsigned long address, pte_t *ptep)
{
	struct ds_list *dnode, *next, *prev;
	unsigned long pte_value = pte_pfn(*ptep);
	unsigned long pte_flag = pte_flags(*ptep);

	if((dnode = make_ds_node(address, address+1, make_ds_offset(address, pte_value), pte_flag)) == NULL)
		return -ENOMEM;

	if(is_add_usr_m_node(address & PT_PGTABLE_MASK_NOT))
		add_usr_m_node((unsigned long)ptep, address & PT_PGTABLE_MASK_NOT);
		
	// incert dnode
	if(list_empty(&ds_list->usr_ds_list)){ //no node
		list_add(&dnode->list, &ds_list->usr_ds_list);
	}else{
		list_for_each_entry(next, &ds_list->usr_ds_list, list){
			if(dnode->limit <= next->base){
				list_add_tail(&dnode->list, &next->list);
				if(list_is_first(&dnode->list, &ds_list->usr_ds_list)){
					ds_node_merge(dnode, next);
					goto end;
				}
				prev = list_prev_entry(dnode, list);
				break;
			}
			if(list_is_last(&next->list, &ds_list->usr_ds_list)){
				list_add_tail(&dnode->list, &ds_list->usr_ds_list);
				prev = list_prev_entry(dnode, list);
				ds_node_merge(prev, dnode);
				goto end;
			}
		}
		ds_node_merge(prev, dnode);
		ds_node_merge(dnode, next);
	}
	// list marge
end:
	return 0;
	
}

int make_ker_list(unsigned long address, pte_t *ptep)
{
	struct ds_list *dnode, *next, *prev;
	unsigned long pte_value = pte_pfn(*ptep);
	unsigned long pte_flag = pte_flags(*ptep);

	if((dnode = make_ds_node(address, address+1, make_ds_offset(address, pte_value), pte_flag)) == NULL)
		return -ENOMEM;

	if(is_add_ker_m_node(address & PT_PGTABLE_MASK_NOT))
		add_ker_m_node((unsigned long)ptep, address & PT_PGTABLE_MASK_NOT);
		
	// incert dnode
	if(list_empty(&ds_list->ker_ds_list)){ //no node
		list_add(&dnode->list, &ds_list->ker_ds_list);
	}else{
		list_for_each_entry(next, &ds_list->ker_ds_list, list){
			if(dnode->limit <= next->base){
				list_add_tail(&dnode->list, &next->list);
				if(list_is_first(&dnode->list, &ds_list->ker_ds_list)){
					ds_node_merge(dnode, next);
					goto end;
				}
				prev = list_prev_entry(dnode, list);
				break;
			}
			if(list_is_last(&next->list, &ds_list->ker_ds_list)){
				list_add_tail(&dnode->list, &ds_list->ker_ds_list);
				prev = list_prev_entry(dnode, list);
				ds_node_merge(prev, dnode);
				goto end;
			}
		}
		ds_node_merge(prev, dnode);
		ds_node_merge(dnode, next);
	}
	// list marge
end:
	return 0;
	
}


// static long make_ds_user(void)
// {
// 	pte_t *ptep;
// 	unsigned long pte_value;
// 	unsigned long pte_flag;
	
// 	int num;
// 	int count;
// 	int hit_flag = 0;
		
// 	unsigned long base;
// 	unsigned long limit;
// 	long offset;
// 	unsigned long flag;
	
// 	unsigned long pte_value_pre;
// 	unsigned long pte_flag_pre;

// 	unsigned long pte_num;
// 	unsigned long pte_num_pre = 0;
	
// 	for(unsigned long a=0; a<USER_MAX; a++){
//         	for(unsigned long b=0; b<MAX; b++){
//             		for(unsigned long c=0; c<MAX; c++){
//                 		for(unsigned long d=0; d<MAX; d++){
//                     			if((num = search_pgtable_get_pfn(a, b, c, d, &ptep)) > 0 && num < 4){ //pte hit
// 						pte_value = pte_pfn(*ptep);
// 						pte_flag = pte_flags(*ptep);
						
// 						pte_num = make_ds_va(a, b, c, 0); // first entry num
// 						// if(pte_num_pre == 0)
// 						// 	vaddr = (unsigned long)ptep;
						
// 						if(pte_num_pre != pte_num){
// 							if(make_m_node((unsigned long)ptep, pte_num) == NULL)
// 								goto end;
// 							pte_num_pre = pte_num;
// 						}
						
// 						if(hit_flag == 0){ // miss, first hit
// 							// make ds members
// 							base = make_ds_va(a, b, c, d);
// 							offset = make_ds_offset(base, pte_value);
// 							flag = pte_flag;
							
// 							hit_flag = 1;
// 							pte_value_pre = pte_value; // pte_value_pre initialize
// 							pte_flag_pre = pte_flag; // pte_flag_pre initialize
// 						}else if(pte_value == pte_value_pre + 1 && pte_flag == pte_flag_pre){ // continuous address hit
// 							pte_value_pre = pte_value;
// 						}else{ // last hit, first nit
// 							// add ds_list list
// 							limit = make_ds_va(a, b, c, d);
// 							if(make_ds_node(base, limit, offset, flag) == NULL)
// 								goto end;

// 							// make ds members
// 							base = limit;
// 							offset = make_ds_offset(base, pte_value);
// 							flag = pte_flag;
							
// 							pte_value_pre = pte_value;
// 							pte_flag_pre = pte_flag;
// 						}
//                         			count = num;
//                     			}else if(num == 0){ // error
// 						goto end;
// 					}else{ // pte miss
// 						if(hit_flag > 0){ // last hit, miss
// 							// add ds_list list
// 							limit = make_ds_va(a, b, c, d);
// 							if(make_ds_node(base, limit, offset, flag) == NULL)
// 								goto end;
// 							hit_flag = 0;
// 						}
//                         			count = num - 3;
//                     			}
//                     			num = 0;
//                     			if(--count > 0)
//                         			break;
//                     			count = 0;
//                 		}
//                 		if(--count > 0)
//                   			break;
//                 		count = 0;
//             		}
//             		if(--count > 0)
//               			break;
//             		count = 0;
//         	}
//         	if(--count > 0)
//           		break;
//         	count = 0;
//     	}
// end:
	
// 	return 0;
// }

static long make_ds_user(void)
{
	pte_t *ptep;
	
	int num;
	int count;
	int flag=0;
		
	unsigned long pte_num;

	for(unsigned long a=0; a<USER_MAX; a++){
        	for(unsigned long b=0; b<MAX; b++){
            		for(unsigned long c=0; c<MAX; c++){
                		for(unsigned long d=0; d<MAX; d++){
                    			if((num = search_pgtable_get_pfn(a, b, c, d, &ptep)) > 0 && num < 4){ //pte hit
						pte_num = make_ds_va(a, b, c, d);
						if(flag == 0){
							vaddr = (unsigned long)ptep;
							flag = 1;
						}
						if(make_usr_list(pte_num, ptep) < 0)
							goto end;
						
                        			count = num;
                    			}else if(num == 0){ // error
						goto end;
					}else{
                        			count = num - 3;
                    			}
                    			num = 0;
                    			if(--count > 0)
                        			break;
                    			count = 0;
                		}
                		if(--count > 0)
                  			break;
                		count = 0;
            		}
            		if(--count > 0)
              			break;
            		count = 0;
        	}
        	if(--count > 0)
          		break;
        	count = 0;
    	}
end:
	
	return 0;
}


// static long make_ds_kernel(void)
// {
// 	pte_t *ptep;
// 	unsigned long pte_value;
// 	unsigned long pte_flag;
	
// 	int num;
// 	int count;
// 	int ds_flag = 0;
	
// 	unsigned long base;
// 	unsigned long limit;
// 	long offset;
// 	unsigned long flag;
	
// 	unsigned long pte_value_pre;
// 	unsigned long pte_flag_pre;

// 	unsigned long pte_num;
// 	unsigned long pte_num_pre = 0;
	
// 	for(unsigned long a=USER_MAX; a<MAX; a++){
//         	for(unsigned long b=0; b<MAX; b++){
//             		for(unsigned long c=0; c<MAX; c++){
//                 		for(unsigned long d=0; d<MAX; d++){
//                     			if((num = search_pgtable_get_pfn(a, b, c, d, &ptep)) > 0 && num < 4){ //pte hit
// 						pte_value = pte_pfn(*ptep);
// 						pte_flag = pte_flags(*ptep);
						
// 						pte_num = make_ds_va(a, b, c, 0); // first entry num
// 						if(pte_num_pre == 0)
// 							vaddr = (unsigned long)ptep;
						
// 						if(pte_num_pre != pte_num){
// 							if(make_m_node((unsigned long)ptep, pte_num) == NULL)
// 								goto end;
// 							pte_num_pre = pte_num;
// 						}

// 						if(!(ds_flag & HIT_FLAG_MASK)){
// 							// make ds members
// 							base = make_ds_va(a, b, c, d);
// 							offset = make_ds_offset(base, pte_value);
// 							flag = pte_flag;
							
// 							ds_flag |= HIT_FLAG_MASK;
// 							pte_value_pre = pte_value; // pte_value_pre initialize
// 							pte_flag_pre = pte_flag; // pte_flag_pre initialize
// 						}else if(pte_value == pte_value_pre + 1 && pte_flag == pte_flag_pre && !(ds_flag & SAME_FLAG_MASK)){ // continuous address hit
// 							pte_value_pre = pte_value;
// 							if(!(ds_flag & CONTI_FLAG_MASK))
// 								ds_flag |= CONTI_FLAG_MASK;
// 						}else if(pte_value == pte_value_pre && pte_flag == pte_flag_pre && !(ds_flag & CONTI_FLAG_MASK)){ // same address hit
// 							if(!(ds_flag & SAME_FLAG_MASK))
// 								ds_flag |= SAME_FLAG_MASK;
// 						}else{ // last hit, first nit
// 							// add ds_list list
// 							limit = make_ds_va(a, b, c, d);
// 							if(ds_flag & SAME_FLAG_MASK){
// 								flag |= SAME_ADDR_MASK;
// 								ds_flag &= SAME_FLAG_MASK_NOT;
// 							}
// 							if(make_ds_node(base, limit, offset, flag) == NULL)
// 								goto end;

// 							// make ds members
// 							base = limit;
// 							offset = make_ds_offset(base, pte_value);
// 							flag = pte_flag;
							
// 							pte_value_pre = pte_value;
// 							pte_flag_pre = pte_flag;
// 							ds_flag &= CONTI_FLAG_MASK_NOT;
// 						}
//                         			count = num;
//                     			}else if(num == 0){ // error
// 						goto end;
// 					}else{ // pte miss
// 						if(ds_flag & HIT_FLAG_MASK){ // last hit, miss
// 							// add ds_list list
// 							limit = make_ds_va(a, b, c, d);
// 							if(ds_flag & SAME_FLAG_MASK){
// 								flag |= SAME_ADDR_MASK;
// 							}
// 							if(make_ds_node(base, limit, offset, flag) == NULL)
// 								goto end;
// 							ds_flag = 0;
// 						}
//                         			count = num - 3;
//                     			}
//                     			num = 0;
//                     			if(--count > 0)
//                         			break;
//                     			count = 0;
//                 		}
//                 		if(--count > 0)
//                   			break;
//                 		count = 0;
//             		}
//             		if(--count > 0)
//               			break;
//             		count = 0;
//         	}
//         	if(--count > 0)
//           		break;
//         	count = 0;
//     	}
// end:
	
// 	return 0;
// }

static long make_ds_kernel(void)
{
	pte_t *ptep;
	
	int num;
	int count;

	unsigned long pte_num;
	
	for(unsigned long a=USER_MAX; a<MAX; a++){
        	for(unsigned long b=0; b<MAX; b++){
            		for(unsigned long c=0; c<MAX; c++){
                		for(unsigned long d=0; d<MAX; d++){
                    			if((num = search_pgtable_get_pfn(a, b, c, d, &ptep)) > 0 && num < 4){ //pte hit
						pte_num = make_ds_va(a, b, c, d);
						if(make_ker_list(pte_num, ptep) < 0)
							goto end;
						
                        			count = num;
                    			}else if(num == 0){ // error
						goto end;
					}else{ // pte miss
                        			count = num - 3;
                    			}
                    			num = 0;
                    			if(--count > 0)
                        			break;
                    			count = 0;
                		}
                		if(--count > 0)
                  			break;
                		count = 0;
            		}
            		if(--count > 0)
              			break;
            		count = 0;
        	}
        	if(--count > 0)
          		break;
        	count = 0;
    	}
end:
	
	return 0;
}

SYSCALL_DEFINE0(mycall_ds_make)
{
	long ret1, ret2;
	ret1 = make_ds_user();
	ret2 = make_ds_kernel();
	
	if(ret1 == ret2)
		return 0;
   	return -1;
}

SYSCALL_DEFINE0(mycall_ds_make_user)
{
	long ret;
	ktime_t start, end;

	start = ktime_get();
	ret = make_ds_user();
	end = ktime_get();

	printk(KERN_INFO "ds_make_user time: %lld\n", ktime_sub(end, start));
	
	return ret;
}

SYSCALL_DEFINE0(mycall_ds_make_kernel)
{
	long ret;
	ktime_t start, end;

	start = ktime_get();
	ret = make_ds_kernel();
	end = ktime_get();

	printk(KERN_INFO "ds_make_kernel time: %lld\n", ktime_sub(end, start));
	
	return ret;
}


static int print_usr_ds(void)
{
	struct ds_list *itr;
	int count = 0;

	struct file *file;
	char *filename = "./usr_ds_txt";
	int size;
	char *buf;
        loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)){
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if(!buf)
		return -1;
	memset(buf, '\0', 100);
	
	list_for_each_entry(itr, &ds_list->usr_ds_list, list){
		// printk(KERN_INFO "%lx %lx %lx %lx   %lx\n", itr->base, itr->limit, itr->offset, itr->flag, __pa((unsigned long)itr));
		size = sprintf(buf, "%lx %lx %lx %lx   %lx\n", itr->base, itr->limit, itr->offset, itr->flag, __pa((unsigned long)itr));
		kernel_write(file, buf, size, &pos);
		vfs_fsync_range(file, 0, size, 1);
		count++;
	}
	printk(KERN_INFO "user ds count %d\n", count);
	size = sprintf(buf, "user ds count %d\n", count);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);
	
	return 0;
}

static int print_ker_ds(void)
{
	struct ds_list *itr;
	int count = 0;

	struct file *file;
	char *filename = "./ker_ds_txt";
	int size;
	char *buf;
        loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)){
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if(!buf)
		return -1;
	memset(buf, '\0', 100);
	
	list_for_each_entry(itr, &ds_list->ker_ds_list, list){
		// printk(KERN_INFO "%lx %lx %lx %lx   %lx\n", itr->base, itr->limit, itr->offset, itr->flag, __pa((unsigned long)itr));
		size = sprintf(buf, "%lx %lx %lx %lx   %lx\n", itr->base, itr->limit, itr->offset, itr->flag, __pa((unsigned long)itr));
		kernel_write(file, buf, size, &pos);
		vfs_fsync_range(file, 0, size, 1);
		count++;
	}
	printk(KERN_INFO "kern ds count %d\n", count);
	size = sprintf(buf, "kern ds count %d\n", count);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);
	
	return 0;
}

SYSCALL_DEFINE0(mycall_ds_search)
{
	print_usr_ds();
	print_ker_ds();
	return 0;
}

static int print_usr_m(void)
{
	struct m_list *itr;
	int count=0;
	
	struct file *file;
	char *filename = "./usr_m_txt";
	int size;
	char *buf;
        loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)){
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if(!buf)
		return -1;
	memset(buf, '\0', 100);
	
	list_for_each_entry(itr, &m_list->usr_m_list, list){
		// printk(KERN_INFO "%lx %lx   %lx\n",itr->va, itr->num, __pa((unsigned long)itr));
		size = sprintf(buf, "%lx %lx   %lx\n",itr->va, itr->num, __pa((unsigned long)itr));
		kernel_write(file, buf, size, &pos);
		vfs_fsync_range(file, 0, size, 1);
		count++;
	}
	printk(KERN_INFO "user m count %d\n", count);
	size = sprintf(buf, "user m count %d\n", count);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);
	
	return 0;
}

static int print_ker_m(void)
{
	struct m_list *itr;
	int count=0;
	
	struct file *file;
	char *filename = "./ker_m_txt";
	int size;
	char *buf;
        loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)){
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if(!buf)
		return -1;
	memset(buf, '\0', 100);
	
	list_for_each_entry(itr, &m_list->ker_m_list, list){
		// printk(KERN_INFO "%lx %lx   %lx\n",itr->va, itr->num, __pa((unsigned long)itr));
		size = sprintf(buf, "%lx %lx   %lx\n",itr->va, itr->num, __pa((unsigned long)itr));
		kernel_write(file, buf, size, &pos);
		vfs_fsync_range(file, 0, size, 1);
		count++;
	}
	printk(KERN_INFO "kern m count %d\n", count);
	size = sprintf(buf, "kern m count %d\n", count);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);
	
	return 0;
}

SYSCALL_DEFINE0(mycall_m_search)
{
	print_usr_m();
	print_ker_m();
	return 0;
}



static int get_pmd_scan_pmd(pud_t *pudp, unsigned long pgd, unsigned long pud, unsigned long pmd, pmd_t **pmdpp)
{
  	pmd_t *pmdp = pmd_offset_index(pudp, pmd);
	*(pmdpp) = pmdp;

  	if(pmd_none(*pmdp) || !pmd_present(*pmdp) || pmd_large(*pmdp)){
    		// printk(KERN_INFO "pmd %lu is not present.\n", pmd);
    		return 4;
  	}
	if(pgd < 0x100) // in user space
		return 1;
	
  	return 2; // in kernel space
}

static int get_pmd_scan_pud(p4d_t *p4dp, unsigned long pgd, unsigned long pud, unsigned long pmd, pmd_t **pmdp)
{
  	pud_t *pudp = pud_offset_index(p4dp, pud);
	  
  	if(pud_none(*pudp) || !pud_present(*pudp) || pud_large(*pudp)){
	    	// printk(KERN_INFO "pud %lu is not present", pud);
	    	return 5;
  	}
  	return get_pmd_scan_pmd(pudp, pgd, pud, pmd, pmdp);  
}

static int get_pmd_scan_p4d(pgd_t *pgdp, unsigned long pgd, unsigned long pud, unsigned long pmd, pmd_t **pmdp)
{
  	p4d_t *p4dp = p4d_offset_index(pgdp, pgd);
	
	if(p4d_none(*p4dp) || !p4d_present(*p4dp)){
	    	// printk(KERN_INFO "p4d %lu is not present", pgd);
    		return 6;
  	}
  	return get_pmd_scan_pud(p4dp, pgd, pud, pmd, pmdp);
}

static int get_pmd_scan_pgd(struct mm_struct *mm, unsigned long pgd, unsigned long pud, unsigned long pmd, pmd_t **pmdp)
{
  	pgd_t *pgdp = pgd_offset_index(mm, pgd);

  	if(pgd_none(*pgdp) || !pgd_present(*pgdp)){
	    	// printk(KERN_INFO "pgd %lu is not present.\n", pgd);
    		return 6;
  	}
  	return get_pmd_scan_p4d(pgdp, pgd, pud, pmd, pmdp);
}

static int search_pgtable_get_pmd(unsigned long num, pmd_t **pmdp)
{
  	struct mm_struct *mm = current->mm;

	unsigned long pgd = (num >> 27) & PT_PGTABLE_MASK;
	unsigned long pud = (num >> 18) & PT_PGTABLE_MASK;
	unsigned long pmd = (num >> 9) & PT_PGTABLE_MASK;

  	if(pgd<0 || 512<=pgd || pud<0 || 512<=pud || pmd<0 || 512<=pmd) {
    		printk(KERN_INFO "error: The numbers are not appropriate.\n");
    		return 0;
  	}
  	return get_pmd_scan_pgd(mm, pgd, pud, pmd, pmdp);
}

static void pmd_repopulate(struct mm_struct *mm, pmd_t *pmd, pte_t *pte)
{
	paravirt_alloc_pte(mm, __pa(pte) >> PAGE_SHIFT);
	set_pmd(pmd, __pmd(__pa(pte) | pmd_flags(*pmd)));
}

static void pmd_reinstall(struct mm_struct *mm, pmd_t *pmdp, pte_t *ptep)
{
	spinlock_t *ptl = pmd_lock(mm, pmdp);
	
	if (!pmd_none(*pmdp) && pmd_present(*pmdp)) {
		smp_wmb(); /* See comment in pmd_install() */
		pmd_repopulate(mm, pmdp, ptep);
		ptep = NULL;
	}
	spin_unlock(ptl);
	
	if (ptep)
		pte_free(mm, virt_to_page(ptep));
}

static pte_t *pte_realloc(struct mm_struct *mm)
{
	struct page *new = (struct page *)pte_alloc_one(mm);
	unsigned long pte;
	if(!new)
		return	NULL;
	
	pte = (unsigned long)page_address(new);
	return (pte_t *)pte;
}

static void pmd_reinstall_kernel(pmd_t *pmdp, pte_t *ptep, struct file *file, loff_t *pos)
{
	int size;
	char *buf;
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
	memset(buf, '\0', 100);

	spin_lock(&init_mm.page_table_lock);
	if (!pmd_none(*pmdp) && pmd_present(*pmdp)) {
		smp_wmb(); /* See comment in pmd_install() */
		pmd_repopulate(&init_mm, pmdp, ptep);
		ptep = NULL;
	}
	spin_unlock(&init_mm.page_table_lock);
	
	size = sprintf(buf, "pmd populate\n");
	kernel_write(file, buf, size, pos);
	vfs_fsync_range(file, 0, size, 1);
	
	if (ptep)
		pte_free_kernel(&init_mm, ptep);
}

static pte_t *pte_realloc_kernel(void)
{
	pte_t *new = pte_alloc_one_kernel(&init_mm);
	if (!new)
		return NULL;
	return new;
}



static void dup_pte(pte_t **ptep, struct ds_list *itr, unsigned long start, unsigned long end)
{
	unsigned long count;
	unsigned long flag;
	pte_t *pte = *(ptep);

	if(itr->base <= start && end < itr->limit){
		// recover pgtable from one ds
		if(itr->flag & SAME_ADDR_MASK){
			flag = itr->flag & SAME_ADDR_MASK_NOT;
			for(count=start; count <= end; count++){
				set_pte(pte, __pte(((pteval_t)(itr->base - itr->offset) << PAGE_SHIFT) | flag));
				pte++;
			}
		}else{
			for(count=start; count <= end; count++){
				set_pte(pte, __pte(((pteval_t)(count - itr->offset) << PAGE_SHIFT) | itr->flag));
				pte++;
			}
		}		
	}else if(itr->base <= start && itr->limit <= end){
		// first recover
		if(itr->flag & SAME_ADDR_MASK){
			flag = itr->flag & SAME_ADDR_MASK_NOT;
			for(count=start; count < itr->limit; count++){
				set_pte(pte, __pte(((pteval_t)(itr->base - itr->offset) << PAGE_SHIFT) | flag));
				pte++;
			}
		}else{
			for(count=start; count < itr->limit; count++){
				set_pte(pte, __pte(((pteval_t)(count - itr->offset) << PAGE_SHIFT) | itr->flag));
				pte++;
			}
		}
	}else if(start < itr->base && end < itr->limit){
		// last recover
		if(itr->flag & SAME_ADDR_MASK){
			flag = itr->flag & SAME_ADDR_MASK_NOT;
			for(count=start; count <= end; count++){
				if(count >= itr->base)
					set_pte(pte, __pte(((pteval_t)(itr->base - itr->offset) << PAGE_SHIFT) | flag));
				pte++;
			}
		}else{
			for(count=start; count <= end; count++){
				if(count >= itr->base)
					set_pte(pte, __pte(((pteval_t)(count - itr->offset) << PAGE_SHIFT) | itr->flag));
				pte++;
			}
		}
	}else if(start < itr->base && itr->limit <= end){
		// second ~ last-1 recover
		if(itr->flag & SAME_ADDR_MASK){
			flag = itr->flag & SAME_ADDR_MASK_NOT;
			for(count=start; count < itr->limit; count++){
				if(count >= itr->base)
					set_pte(pte, __pte(((pteval_t)(itr->base - itr->offset) << PAGE_SHIFT) | flag));
				pte++;
			}
		}else{
			for(count=start; count < itr->limit; count++){
				if(count >= itr->base)
					set_pte(pte, __pte(((pteval_t)(count - itr->offset) << PAGE_SHIFT) | itr->flag));
				pte++;
			}
		}
	}
	*(ptep) = pte;
}

/*
static void print_pte(pmd_t *pmdp)
{
	unsigned long pte;
	for(pte=0; pte<MAX; pte++){
		pte_t *ptep = pte_offset_index(pmdp, pte);

  		if(!pte_none(*ptep) && pte_present(*ptep)) {
			pte_value = pte_pfn(*ptep);
			pte_flag = pte_flags(*ptep);
			printk(KERN_INFO "%ld %lx %lx", pte,  pte_value, pte_flag);
		}
	}
}
*/

static int update_pgtable(unsigned long va_start, pte_t *pte, struct file *file, loff_t *pos)
{
	struct ds_list *itr;
	unsigned long va_end;
	int flag = 0;
	
	int size;
	char *buf;
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
	memset(buf, '\0', 100);

	va_end = va_start | PT_PGTABLE_MASK;

	// printk(KERN_INFO "%ld-%ld-%ld-0  %lx %lx\n", (va_start >> 27) & PT_PGTABLE_MASK, (va_start >> 18) & PT_PGTABLE_MASK, (va_start >> 9) & PT_PGTABLE_MASK, va_start, va_end);
	size = sprintf(buf, "%ld-%ld-%ld-0  %lx %lx\n", (va_start >> 27) & PT_PGTABLE_MASK, (va_start >> 18) & PT_PGTABLE_MASK, (va_start >> 9) & PT_PGTABLE_MASK, va_start, va_end);
	kernel_write(file, buf, size, pos);
	vfs_fsync_range(file, 0, size, 1);
	
	list_for_each_entry(itr, &ds_list->usr_ds_list, list){
		if(itr->limit <= va_start){
			continue; // not hit yet
		}else if(va_end < itr->base){
			break; // already finished
		}else{ // recover pgtable from ds
			// printk(KERN_INFO "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
			size = sprintf(buf, "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
			kernel_write(file, buf, size, pos);
			vfs_fsync_range(file, 0, size, 1);
			
			dup_pte(&pte, itr, va_start, va_end);
			va_start = itr->limit;
			flag = 1;
		}
	}
	return flag;
}

static int __recover_pgtable(unsigned long va_start, struct file *file, loff_t *pos)
{
	pmd_t *pmdp;
	pte_t *ptep_old;
	pte_t *ptep_new;
	int num;

	int size;
	char *buf;
	
        buf = kmalloc(PATH_MAX, GFP_KERNEL);
	memset(buf, '\0', 100);
		
	if((num = search_pgtable_get_pmd(va_start, &pmdp)) == 1){ // in user
		ptep_old = pte_offset_index(pmdp, 0);

		// printk(KERN_INFO "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
		size = sprintf(buf, "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
		kernel_write(file, buf, size, pos);
		vfs_fsync_range(file, 0, size, 1);
		// printk(KERN_INFO "pte before: %lx\n", (unsigned long)__pa(ptep_old));
		size = sprintf(buf, "pte before: %lx\n", (unsigned long)__pa(ptep_old));
		kernel_write(file, buf, size, pos);
		vfs_fsync_range(file, 0, size, 1);
		
		ptep_new = pte_realloc(current->mm);
		
		if(!ptep_new){
			printk(KERN_INFO "out of memory\n");
			goto err;
		}
		
		if(update_pgtable(va_start, ptep_new, file, pos) == 1){
			pmd_reinstall(current->mm, pmdp, ptep_new);

			// printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
			size = sprintf(buf, "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
			kernel_write(file, buf, size, pos);
			vfs_fsync_range(file, 0, size, 1);

			// printk(KERN_INFO "pte after:  %lx", (unsigned long)__pa(ptep_new));
			size = sprintf(buf, "pte after: %lx\n",(unsigned long)__pa(ptep_new));
			kernel_write(file, buf, size, pos);
			vfs_fsync_range(file, 0, size, 1);
		}else{
			// printk(KERN_INFO "not dup pte\n");
			size = sprintf(buf, "not dup pte\n");
			kernel_write(file, buf, size, pos);
			vfs_fsync_range(file, 0, size, 1);
			pte_free(current->mm, virt_to_page(ptep_new));
		}
	}
	else if(num == 2){ // in kernel
		ptep_old = pte_offset_index(pmdp, 0);
		
		// printk(KERN_INFO "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
		size = sprintf(buf, "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
		kernel_write(file, buf, size, pos);
		vfs_fsync_range(file, 0, size, 1);
		// printk(KERN_INFO "pte before: %lx\n", (unsigned long)__pa(ptep_old));
		size = sprintf(buf, "pte before: %lx\n", (unsigned long)__pa(ptep_old));
		kernel_write(file, buf, size, pos);
		vfs_fsync_range(file, 0, size, 1);
		
		ptep_new = pte_realloc_kernel();
		
		if(!ptep_new){
			printk(KERN_INFO "out of memory\n");
			goto err;
		}
		
		if(update_pgtable(va_start, ptep_new, file, pos) == 1){
			pmd_reinstall_kernel(pmdp, ptep_new, file, pos);
			
			// printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
			size = sprintf(buf, "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
			kernel_write(file, buf, size, pos);
			vfs_fsync_range(file, 0, size, 1);

			// printk(KERN_INFO "pte after:  %lx", (unsigned long)__pa(ptep_new));
			size = sprintf(buf, "pte after: %lx\n",(unsigned long)__pa(ptep_new));
			kernel_write(file, buf, size, pos);
			vfs_fsync_range(file, 0, size, 1);
		}else{
			// printk(KERN_INFO "not dup pte\n");
			size = sprintf(buf, "not dup pte\n");
			kernel_write(file, buf, size, pos);
			vfs_fsync_range(file, 0, size, 1);
			pte_free_kernel(&init_mm, ptep_new);
		}
	}
	else if(num == 0){
		goto err;
	}
	else{
		return num - 3;
	}
	return 0;
err:
	return -1;
}

static long recover_all_pgtable(void)
{
	// pmd_t *pmdp;
	// pte_t *ptep_old;
	// pte_t *ptep_new;
	// pte_t *pte;
	
	unsigned long va_start;
	// unsigned long va_end;
	// spinlock_t *ptl;
	
	// int num;
	int count;
	// int flag = 0;
	// struct ds_list *itr;

	struct file *file;
	char *filename = "./write_log_txt";
	// int size;
	// char *buf;
        loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)){
		printk("pre_file open err=%ld", PTR_ERR(file));
		goto err;
	}
	
        // buf = kmalloc(PATH_MAX, GFP_KERNEL);
        // if(!buf)
		// goto err;
	// memset(buf, '\0', 100);
	
	for(unsigned long a=0; a<MAX; a++){
        	for(unsigned long b=0; b<MAX; b++){
            		for(unsigned long c=0; c<MAX; c++){
				va_start = make_ds_va(a, b, c, 0);

				if((count = __recover_pgtable(va_start, file, &pos)) < 0){
					goto err;
				}
				
				/*if((num = search_pgtable_get_pmd(va_start, &pmdp)) == 1){ // in user
					
					// pte_alloc
					ptep_old = pte_offset_index(pmdp, 0);

					// printk(KERN_INFO "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
					size = sprintf(buf, "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
					// printk(KERN_INFO "pte before: %lx\n", (unsigned long)__pa(ptep_old));
					size = sprintf(buf, "pte before: %lx\n", (unsigned long)__pa(ptep_old));
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
					
					// ptep_new = pte_realloc_offset_head(current->mm, pmdp, &ptl);
					ptep_new = pte_realloc(current->mm);
					
					if(!ptep_new){
						// spin_unlock(ptl);
						goto end;
					}
					
					// list_for_each_entry ds
					
					// va_end = make_ds_va(a, b, c, 511);

					// printk(KERN_INFO "%ld-%ld-%ld-0  %lx %lx\n", a, b, c, va_start, va_end);
					// size = sprintf(buf, "%ld-%ld-%ld-0  %lx %lx\n", a, b, c, va_start, va_end);
					// kernel_write(file, buf, size, &pos);
					// vfs_fsync_range(file, 0, size, 1);

					// pte = ptep_new;

					// list_for_each_entry(itr, &ds_list->usr_ds_list, list){
					// 	if(itr->limit <= va_start){
					// 		continue; // not hit yet
					// 	}else if(va_end < itr->base){
					// 		break; // already finished
					// 	}else{ // recover pgtable from ds
					// 		// printk(KERN_INFO "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
					// 		size = sprintf(buf, "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
					// 		kernel_write(file, buf, size, &pos);
					// 		vfs_fsync_range(file, 0, size, 1);
							
					// 		dup_pte(&pte, itr, va_start, va_end);
					// 		va_start = itr->limit;
					// 		flag = 1;
					// 	}
					// }

					if(update_pgtable(va_start, ptep_new, file, &pos) == 1){
						pmd_reinstall(current->mm, pmdp, ptep_new);

						// printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
						size = sprintf(buf, "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
						kernel_write(file, buf, size, &pos);
						vfs_fsync_range(file, 0, size, 1);

						// printk(KERN_INFO "pte after:  %lx", (unsigned long)__pa(ptep_new));
						size = sprintf(buf, "pte after: %lx\n",(unsigned long)__pa(ptep_new));
						kernel_write(file, buf, size, &pos);
						vfs_fsync_range(file, 0, size, 1);
					}else{
						// printk(KERN_INFO "not dup pte %ld-%ld-%ld-0\n", a, b, c);
						size = sprintf(buf, "not dup pte %ld-%ld-%ld-0\n", a, b, c);
						kernel_write(file, buf, size, &pos);
						vfs_fsync_range(file, 0, size, 1);
						pte_free(current->mm, virt_to_page(ptep_new));
					}
					
					// spin_unlock(ptl);
					// pte_free(current->mm, virt_to_page(ptep_old));
					// print_pte(pmdp);
				}
				else if(num == 2){ // in kernel
					ptep_old = pte_offset_index(pmdp, 0);
					
					// printk(KERN_INFO "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
					size = sprintf(buf, "pmd before: %lx\n",(unsigned long)pmd_val(*pmdp));
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
					// printk(KERN_INFO "pte before: %lx\n", (unsigned long)__pa(ptep_old));
					size = sprintf(buf, "pte before: %lx\n", (unsigned long)__pa(ptep_old));
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
					
					// ptep_new = pte_realloc_kernel_offset_head(pmdp, file, &pos);
					ptep_new = pte_realloc_kernel();
					
					if(!ptep_new){
						// spin_unlock(&init_mm.page_table_lock);
						goto end;
					}
					
					// list_for_each_entry ds
					va_start = make_ds_va(a, b, c, 0);
					// va_end = make_ds_va(a, b, c, 511);

					// // printk(KERN_INFO "%ld-%ld-%ld-0  %lx %lx\n", a, b, c, va_start, va_end);
					// size = sprintf(buf, "%ld-%ld-%ld-0  %lx %lx\n", a, b, c, va_start, va_end);
					// kernel_write(file, buf, size, &pos);
					// vfs_fsync_range(file, 0, size, 1);

					// pte = ptep_new;
					
					// list_for_each_entry(itr, &ds_list->usr_ds_list, list){
					// 	if(itr->limit <= va_start){
					// 		continue; // not hit yet
					// 	}else if(va_end < itr->base){
					// 		break; // already finished
					// 	}else{ // recover pgtable from ds
					// 		// printk(KERN_INFO "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
					// 		size = sprintf(buf, "    %lx %lx %lx %lx\n", itr->base, itr->limit, itr->offset, itr->flag);
					// 		kernel_write(file, buf, size, &pos);
					// 		vfs_fsync_range(file, 0, size, 1);
							
					// 		dup_pte(&pte, itr, va_start, va_end);
					// 		va_start = itr->limit;
					// 		flag = 1;
					// 	}
					// }

					

					if(update_pgtable(va_start, ptep_new, file, &pos) == 1){
						pmd_reinstall_kernel(pmdp, ptep_new, file, &pos);
						flag = 0;
						
						// printk(KERN_INFO "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
						size = sprintf(buf, "pmd after: %lx\n",(unsigned long)pmd_val(*pmdp));
						kernel_write(file, buf, size, &pos);
						vfs_fsync_range(file, 0, size, 1);

						// printk(KERN_INFO "pte after:  %lx", (unsigned long)__pa(ptep_new));
						size = sprintf(buf, "pte after: %lx\n",(unsigned long)__pa(ptep_new));
						kernel_write(file, buf, size, &pos);
						vfs_fsync_range(file, 0, size, 1);
					}else{
						// printk(KERN_INFO "not dup pte %ld-%ld-%ld-0\n", a, b, c);
						size = sprintf(buf, "not dup pte %ld-%ld-%ld-0\n", a, b, c);
						kernel_write(file, buf, size, &pos);
						vfs_fsync_range(file, 0, size, 1);
						pte_free_kernel(&init_mm, ptep_new);
					}
					
					// print_pte(pmdp);
					
				}
				else if(num == 0){
					goto end;
				}
				else{
					count = num - 3;
				}
				num = 0;
				*/
				if(--count > 0)
					break;
				count = 0;
			}
			if(--count > 0)
				break;
			count = 0;
		}
		if(--count > 0)
			break;
		count = 0;
	}
	// kfree(buf);
	// filp_close(file, NULL);
	return 0;
err:	
	// kfree(buf);
	// filp_close(file, NULL);
	return -1;	
}


SYSCALL_DEFINE0(mycall_recover_all_pgtable)
{
	int ret = -1;
	printk(KERN_INFO "start recover pgtable\n");
	ret = recover_all_pgtable();
	printk(KERN_INFO "end recover pgtable\n");
	return ret;
}

static long recover_pgtable(unsigned long va)
{
	struct m_list *itr;
	int count;
	
	struct file *file;
	char *filename = "./write_log_txt";
	// int size;
	// char *buf;
        loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)){
		printk("pre_file open err=%ld", PTR_ERR(file));
		goto err;
	}
	
        // buf = kmalloc(PATH_MAX, GFP_KERNEL);
        // if(!buf)
		// goto err;
	// memset(buf, '\0', 100);

	list_for_each_entry(itr, &m_list->usr_m_list, list){
		if(itr->va <= va && va < itr->va + 0x1000){
			printk(KERN_INFO "pgtable found %lx\n",va);
			if((count = __recover_pgtable(itr->num, file, &pos)) < 0){
				goto err;
			}
			return 0;
		}
	}
err:
	printk(KERN_INFO "recover pgtable va:%lx error!\n",va);
	return -1;
}

SYSCALL_DEFINE1(mycall_recover_pgtable, unsigned long, va)
{
	int ret = -1;
	printk(KERN_INFO "start recover pgtable %lx\n",va);
	ret = recover_pgtable(vaddr);
	printk(KERN_INFO "end recover pgtable %lx\n",va);
	return ret;
}

static long delete_ds(void)
{
	struct ds_list *itr;
	int count=0;

	while((&ds_list->usr_ds_list)->next != &ds_list->usr_ds_list){
		itr = list_first_entry(&ds_list->usr_ds_list, typeof(*itr), list);
		list_del(&itr->list);
		kfree(itr);
	}

	while((&ds_list->ker_ds_list)->next != &ds_list->ker_ds_list){
		itr = list_first_entry(&ds_list->ker_ds_list, typeof(*itr), list);
		list_del(&itr->list);
		kfree(itr);
	}

	list_for_each_entry(itr, &ds_list->usr_ds_list, list){
		count++;
	}
	printk(KERN_INFO "delete user ds count %d\n", count);

	count=0;
	list_for_each_entry(itr, &ds_list->ker_ds_list, list){
		count++;
	}
	printk(KERN_INFO "delete kern ds count %d\n", count);

	return 0;
}

SYSCALL_DEFINE0(mycall_ds_delete)
{
	return delete_ds();
}

static long delete_m(void)
{
	struct m_list *itr;
	int count=0;

	while((&m_list->usr_m_list)->next != &m_list->usr_m_list){
		itr = list_first_entry(&m_list->usr_m_list, typeof(*itr), list);
		list_del(&itr->list);
		kfree(itr);
	}

	while((&m_list->ker_m_list)->next != &m_list->ker_m_list){
		itr = list_first_entry(&m_list->ker_m_list, typeof(*itr), list);
		list_del(&itr->list);
		kfree(itr);
	}

	list_for_each_entry(itr, &m_list->usr_m_list, list){
		count++;
	}
	printk(KERN_INFO "delete user m count %d\n", count);

	count=0;
	list_for_each_entry(itr, &m_list->ker_m_list, list){
		count++;
	}
	printk(KERN_INFO "delete kern m count %d\n", count);

	return 0;
}

SYSCALL_DEFINE0(mycall_m_delete)
{
	return delete_m();
}
