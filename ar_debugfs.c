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

/**************************************************************************
 * Included Files
 **************************************************************************/
#include "ar_debugfs.h"

/**************************************************************************
 * Constants /Macros
 **************************************************************************/
#define BUF_SIZE 256

/**************************************************************************
 * Globals
 **************************************************************************/
static u32 ar_regulation_time_ms = 1; //ms, default 1000ms
static u32 ar_observation_time_ms = 1000;
static u32 ar_sw_size = 20;
static struct dentry *ar_dir = NULL;


/****************************************
 * Fops functions for Regulation interval
 ****************************************/
static int ar_reg_interval_show(struct seq_file *m, void *v)
{
    int tmp = ar_regulation_time_ms;
    pr_info("%s: Reading.",__func__);
    seq_printf(m, "%u\n",tmp);
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
    u32 tmp = 0 ;

    if (copy_from_user(&buf, ubuf, (cnt > BUF_SIZE) ? BUF_SIZE: cnt) != 0)
        return 0;

    pr_info("%s: Received %s",__func__,buf);

    int ret = kstrtou32(buf, 10, &tmp);

    if (ret){

        pr_err("%s: ret %d",__func__,ret);
        return 0;
    }

    ar_regulation_time_ms =  tmp;
    return cnt;
}

/****************************************
 * Fops functions for Observation interval
 ****************************************/
static ssize_t ar_obs_interval_write(struct file *filp,
                    const char __user *ubuf,
                    size_t cnt, loff_t *ppos){

    char buf[BUF_SIZE];
    u32 tmp = 0 ;

    if (copy_from_user(&buf, ubuf, (cnt > BUF_SIZE) ? BUF_SIZE: cnt) != 0)
        return 0;

    pr_info("%s: Received %s",__func__,buf);

    int ret = kstrtou32(buf, 10, &tmp);

    if (ret){
        pr_err("%s: ret %d",__func__,ret);
        return 0;
    }

    ar_observation_time_ms =  tmp;
    return cnt;
}

static int ar_obs_interval_show(struct seq_file *m, void *v)
{
    int tmp = ar_observation_time_ms;
    pr_info("%s: Reading.",__func__);
    seq_printf(m, "%u \n",tmp);
    return 0;
}


static int ar_obs_interval_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, ar_obs_interval_show, NULL);
}


/****************************************
 * Fops functions for Sliding window size interval
 ****************************************/
static ssize_t ar_sw_size_write(struct file *filp,
                    const char __user *ubuf,
                    size_t cnt, loff_t *ppos){

    char buf[BUF_SIZE];
    u32 tmp = 0 ;

    if (copy_from_user(&buf, ubuf, (cnt > BUF_SIZE) ? BUF_SIZE: cnt) != 0)
        return 0;

    if (1){
        pr_err("%s: Attempted changing window size to %s, not supported \n",__func__, buf);
        return 0;
    }
    
    pr_info("%s: Received %s",__func__,buf);

    int ret = kstrtou32(buf, 10, &tmp);

    if (ret){
        pr_err("%s: ret %d",__func__,ret);
        return 0;
    }

    ar_sw_size = tmp;
    return cnt;
}

static int ar_sw_size_read(struct seq_file *m, void *v)
{
    int tmp = ar_sw_size;
    // pr_info("%s: Reading.",__func__);
    seq_printf(m, "%u \n",tmp);
    return 0;
}


static int ar_sw_size_open(struct inode *inode, struct file *filp)
{
    return single_open(filp,ar_sw_size_read , NULL);
}

/****************************************
 * debug Fops 
 ****************************************/


static const struct file_operations ar_obs_interval_fops = {
    .open       = ar_obs_interval_open,
    .write      = ar_obs_interval_write,
    .read       = seq_read,
    .release    = single_release,
};

static const struct file_operations ar_reg_interval_fops = {
    .open       = ar_reg_interval_open,
    .write      = ar_reg_interval_write,
    .read       = seq_read,
    .release    = single_release,
};

static const struct file_operations ar_sliding_window_size = {
    .open       = ar_sw_size_open,
    .write      = ar_sw_size_write,
    .read       = seq_read,
    .release    = single_release,
};

int ar_init_debugfs(void)
{

    ar_dir = debugfs_create_dir("ar", NULL);
    BUG_ON(!ar_dir);
    debugfs_create_file("regu_interval", 0444, ar_dir, NULL,
                &ar_reg_interval_fops);
    debugfs_create_file("obs_interval", 0444, ar_dir, NULL,
                &ar_obs_interval_fops);
    debugfs_create_file("sliding_window_size", 0444, ar_dir, NULL,
            &ar_sliding_window_size);
    return 0;
}

void inline ar_remove_debugfs(void){

    debugfs_remove_recursive(ar_dir);
}

u32 inline get_regulation_time(void){
	return ar_regulation_time_ms;
}

u32 inline get_sliding_window_size(void){
    return ar_sw_size;
}