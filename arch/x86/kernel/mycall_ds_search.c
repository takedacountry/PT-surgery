#include <linux/kernel.h>
#include <linux/syscalls.h> //
// #include <linux/types.h>
#include <linux/fs.h>
// #include <linux/list.h> //
// #include <linux/slab.h> //
#include <linux/err.h> //
#include <asm/current.h> //
#include "ds.h" //
// #include "ds_struct.h" //

static int print_usr_ds(pid_t pid)
{
	struct ds_list *dnode;
	struct m_head_list *mhead;
	struct m_list *mnode;
	int count = 0;

	struct file *file;
	char *filename = "./usr_ds_txt";
	int size;
	char *buf;
    loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)) {
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
    buf = kmalloc(PATH_MAX, GFP_KERNEL);
    if(!buf) {
		filp_close(file, NULL);
		return -1;
	}
	memset(buf, '\0', 256);

	read_lock(&user_head_lock);
	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == pid) {
		// if(mhead->pid == target_task->pid) {
			m_list_read_lock(mhead);
			printk(KERN_INFO "ds pid: %d\n", pid);
			size = sprintf(buf, "ds pid: %d\n", pid);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);

			list_for_each_entry(mnode, &mhead->head, list) {
				member_read_lock(mnode);
				size = sprintf(buf, "m list %lx\n", mnode->base);
				kernel_write(file, buf, size, &pos);
				vfs_fsync_range(file, 0, size, 1);
				member_read_unlock(mnode);

				ds_list_read_lock(mnode);
				list_for_each_entry(dnode, &mnode->ds_head, list) {
					// printk(KERN_INFO "%lx %lx %lx %lx  %lx\n", dnode->base, dnode->limit, dnode->offset, dnode->flag, __pa((unsigned long)dnode));
					size = sprintf(buf, "  %lx %lx %lx %lx\n", dnode->base, dnode->limit, dnode->offset, dnode->flag);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
					count++;
				}
				ds_list_read_unlock(mnode);
			}
			m_list_read_unlock(mhead);
			break;
		}
	}
	read_unlock(&user_head_lock);

	printk(KERN_INFO "user ds count %d, pid %d\n", count, pid);
	size = sprintf(buf, "user ds count %d, pid %d\n", count, pid);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);

	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}

// static int print_ker_ds(void)
// {
// 	struct ds_list *itr;
// 	int count = 0;

// 	struct file *file;
// 	char *filename = "./ker_ds_txt";
// 	int size;
// 	char *buf;
//         loff_t pos = 0;

// 	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
// 	if(IS_ERR(file)){
// 		printk("pre_file open err=%ld", PTR_ERR(file));
// 		return -1;
// 	}
	
//         buf = kmalloc(PATH_MAX, GFP_KERNEL);
//         if(!buf)
// 		return -1;
// 	memset(buf, '\0', 100);
	
// 	list_for_each_entry(itr, &ker_ds_head, list){
// 		// printk(KERN_INFO "%lx %lx %lx %lx   %lx\n", itr->base, itr->limit, itr->offset, itr->flag, __pa((unsigned long)itr));
// 		size = sprintf(buf, "%lx %lx %lx %lx   %lx\n", itr->base, itr->limit, itr->offset, itr->flag, __pa((unsigned long)itr));
// 		kernel_write(file, buf, size, &pos);
// 		vfs_fsync_range(file, 0, size, 1);
// 		count++;
// 	}
// 	printk(KERN_INFO "kern ds count %d\n", count);
// 	size = sprintf(buf, "kern ds count %d\n", count);
// 	kernel_write(file, buf, size, &pos);
// 	vfs_fsync_range(file, 0, size, 1);

// 	kfree(buf);
// 	filp_close(file, NULL);
	
// 	return 0;
// }

SYSCALL_DEFINE0(mycall_ds_search)
{
	print_usr_ds(current->tgid);
	// print_ker_ds();
	return 0;
}

static int print_usr_ds2(pid_t pid)
{
	struct ds_list *dnode;
	struct m_head_list *mhead;
	struct m_list *mnode;
	int count = 0;

	struct file *file;
	char *filename = "./usr_ds_txt2";
	int size;
	char *buf;
    loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)) {
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
    buf = kmalloc(PATH_MAX, GFP_KERNEL);
    if(!buf) {
		filp_close(file, NULL);
		return -1;
	}
	memset(buf, '\0', 256);

	read_lock(&user_head_lock);
	list_for_each_entry(mhead, &user_head, list) {
		if(mhead->pid == pid) {
		// if(mhead->pid == target_task->pid) {
			m_list_read_lock(mhead);
			printk(KERN_INFO "ds pid: %d\n", pid);
			size = sprintf(buf, "ds pid: %d\n", pid);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);

			list_for_each_entry(mnode, &mhead->head, list) {
				member_read_lock(mnode);
				size = sprintf(buf, "m list %lx\n", mnode->base);
				kernel_write(file, buf, size, &pos);
				vfs_fsync_range(file, 0, size, 1);
				member_read_unlock(mnode);

				ds_list_read_lock(mnode);
				list_for_each_entry(dnode, &mnode->ds_head, list) {
					// printk(KERN_INFO "%lx %lx %lx %lx   %lx\n", dnode->base, dnode->limit, dnode->offset, dnode->flag, __pa((unsigned long)dnode));
					size = sprintf(buf, "  %lx %lx %lx %lx\n", dnode->base, dnode->limit, dnode->offset, dnode->flag);
					kernel_write(file, buf, size, &pos);
					vfs_fsync_range(file, 0, size, 1);
					count++;
				}
				ds_list_read_unlock(mnode);
			}
			m_list_read_unlock(mhead);
			break;
		}
	}
	read_unlock(&user_head_lock);

	printk(KERN_INFO "user ds count %d, pid %d\n", count, pid);
	size = sprintf(buf, "user ds count %d, pid %d\n", count, pid);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);

	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}

SYSCALL_DEFINE0(mycall_ds_search2)
{
	print_usr_ds2(current->tgid);
	return 0;
}

static int print_usr_m(pid_t pid)
{
	struct m_list *itr;
	struct m_head_list *m_head;
	int count=0;
	
	struct file *file;
	char *filename = "./usr_m_txt";
	int size;
	char *buf;
    loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)) {
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
    buf = kmalloc(PATH_MAX, GFP_KERNEL);
    if(!buf) {
		filp_close(file, NULL);
		return -1;
	}
	memset(buf, '\0', 256);

	read_lock(&user_head_lock);
	list_for_each_entry(m_head, &user_head, list) {
		if(m_head->pid == pid) {
		// if(m_head->pid == target_task->pid){
			m_list_read_lock(m_head);
			printk(KERN_INFO "m pid: %d\n", pid);
			size = sprintf(buf, "m pid: %d\n", pid);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			list_for_each_entry(itr, &m_head->head, list) {
				member_read_lock(itr);
				// printk(KERN_INFO "%lx %lx   %lx\n",itr->va, itr->base, __pa((unsigned long)itr));
				size = sprintf(buf, "%lx %lx\n",itr->va, itr->base);
				kernel_write(file, buf, size, &pos);
				vfs_fsync_range(file, 0, size, 1);
				count++;
				member_read_unlock(itr);
			}
			m_list_read_unlock(m_head);
			break;
		}
	}
	read_unlock(&user_head_lock);

	printk(KERN_INFO "user m count %d, pid %d\n", count, pid);
	size = sprintf(buf, "user m count %d pid %d\n", count, pid);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);

	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}

// static int print_ker_m(void)
// {
// 	struct m_list *itr;
// 	int count=0;
	
// 	struct file *file;
// 	char *filename = "./ker_m_txt";
// 	int size;
// 	char *buf;
//         loff_t pos = 0;

// 	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
// 	if(IS_ERR(file)){
// 		printk("pre_file open err=%ld", PTR_ERR(file));
// 		return -1;
// 	}
	
//         buf = kmalloc(PATH_MAX, GFP_KERNEL);
//         if(!buf)
// 		return -1;
// 	memset(buf, '\0', 100);
	
// 	list_for_each_entry(itr, &ker_m_head, list){
// 		// printk(KERN_INFO "%lx %lx   %lx\n",itr->va, itr->base, __pa((unsigned long)itr));
// 		size = sprintf(buf, "%lx %lx   %lx\n",itr->va, itr->base, __pa((unsigned long)itr));
// 		kernel_write(file, buf, size, &pos);
// 		vfs_fsync_range(file, 0, size, 1);
// 		count++;
// 	}
// 	printk(KERN_INFO "kern m count %d\n", count);
// 	size = sprintf(buf, "kern m count %d\n", count);
// 	kernel_write(file, buf, size, &pos);
// 	vfs_fsync_range(file, 0, size, 1);

// 	kfree(buf);
// 	filp_close(file, NULL);
	
// 	return 0;
// }

SYSCALL_DEFINE0(mycall_m_search)
{
	print_usr_m(current->tgid);
	// print_ker_m();
	return 0;
}

static int print_usr_m2(pid_t pid)
{
	struct m_list *itr;
	struct m_head_list *m_head;
	int count=0;
	
	struct file *file;
	char *filename = "./usr_m_txt2";
	int size;
	char *buf;
    loff_t pos = 0;

	file = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if(IS_ERR(file)) {
		printk("pre_file open err=%ld", PTR_ERR(file));
		return -1;
	}
	
	buf = kmalloc(PATH_MAX, GFP_KERNEL);
	if(!buf) {
		filp_close(file, NULL);
		return -1;
	}
	memset(buf, '\0', 256);

	read_lock(&user_head_lock);
	list_for_each_entry(m_head, &user_head, list) {
		if(m_head->pid == pid) {
		// if(m_head->pid == target_task->pid){
			m_list_read_lock(m_head);
			printk(KERN_INFO "m pid: %d\n", pid);
			size = sprintf(buf, "m pid: %d\n", pid);
			kernel_write(file, buf, size, &pos);
			vfs_fsync_range(file, 0, size, 1);
			
			list_for_each_entry(itr, &m_head->head, list) {
				member_read_lock(itr);
				// printk(KERN_INFO "%lx %lx   %lx\n",itr->va, itr->base, __pa((unsigned long)itr));
				size = sprintf(buf, "%lx %lx\n",itr->va, itr->base);
				kernel_write(file, buf, size, &pos);
				vfs_fsync_range(file, 0, size, 1);
				count++;
				member_read_unlock(itr);
			}
			m_list_read_unlock(m_head);
			break;
		}
	}
	read_unlock(&user_head_lock);

	printk(KERN_INFO "user m count %d, pid %d\n", count, pid);
	size = sprintf(buf, "user m count %d pid %d\n", count, pid);
	kernel_write(file, buf, size, &pos);
	vfs_fsync_range(file, 0, size, 1);

	kfree(buf);
	filp_close(file, NULL);
	
	return 0;
}

SYSCALL_DEFINE0(mycall_m_search2)
{
	print_usr_m2(current->tgid);
	return 0;
}

static void count_up_m_ds(void)
{
	struct ds_list *dnode;
	struct m_head_list *mhead;
	struct m_list *mnode;
	
	read_lock(&user_head_lock);
	list_for_each_entry(mhead, &user_head, list) {
		int ds_num = 0;
		int m_num = 0;
		int pte_num = 0;
		m_list_read_lock(mhead);
		list_for_each_entry(mnode, &mhead->head, list) {
			m_num++;
			if (mnode->base & PTE_FLAG_MASK)
				pte_num++;
			ds_list_read_lock(mnode);
			list_for_each_entry(dnode, &mnode->ds_head, list) {
				ds_num++;
			}
			ds_list_read_unlock(mnode);
		}
		m_list_read_unlock(mhead);
		printk(KERN_INFO "pid %d m count %d, pte count %d, ds count %d\n",mhead->pid, m_num, pte_num, ds_num);
	}
	read_unlock(&user_head_lock);
	
	return;
}

SYSCALL_DEFINE0(mycall_m_ds_count)
{
	count_up_m_ds();
	return 0;
}