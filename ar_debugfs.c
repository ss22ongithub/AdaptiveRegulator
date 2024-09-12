/**
 * Dynamic adaptive memory bandwidth controller for multi-core systems
 *
 *
 * This file is distributed under GPL v2 License. 
 * See LICENSE.TXT for details.
 *
 */


/**************************************************************************
 * Included Files
 **************************************************************************/
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/irq_work.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <asm/atomic.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/trace_events.h>


#define BUF_SIZE 256

static u32 ar_regulation_time_ms = 1000; //ms, default 1000ms
static struct dentry *ar_dir = NULL;

static int ar_reg_interval_show(struct seq_file *m, void *v)
{
	int tmp = ar_regulation_time_ms;
	pr_info("%s: Reading.",__func__);
	seq_printf(m, "regulation_interval = %u",tmp);
	return 0;
}


static int ar_reg_interval_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, ar_reg_interval_show, NULL);
}

static ssize_t ar_reg_interval_write(struct file *filp,
				    const char __user *ubuf,
				    size_t cnt, loff_t *ppos){

	char buf[BUF_SIZE];
	
	if (copy_from_user(&buf, ubuf, (cnt > BUF_SIZE) ? BUF_SIZE: cnt) != 0)
		return 0;

	pr_info("%s: Received %s",__func__,buf);
	return cnt;
}

static const struct file_operations ar_reg_interval_fops = {
	.open		= ar_reg_interval_open,
	.write      = ar_reg_interval_write,
	.read		= seq_read,
	.release	= single_release,
};

int ar_init_debugfs(void)
{

	ar_dir = debugfs_create_dir("ar", NULL);
	BUG_ON(!ar_dir);
	debugfs_create_file("regulation_interval", 0444, ar_dir, NULL,
			    &ar_reg_interval_fops);
	return 0;
}

void ar_remove_debugfs(void){

	debugfs_remove_recursive(ar_dir);
}

