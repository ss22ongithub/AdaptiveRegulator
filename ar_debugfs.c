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
#include "kernel_headers.h"
#include "ar.h"
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
static u32 ar_sw_size = 25;  //If changing this change SLIDING_WINDOW_SIZE as well
atomic_t enable_reg; // Memory regulation enabled or disabled via debugfs
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
        return ret;
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
        return ret;
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
        return ret;
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
/******************************************************
 Fops functions for enable/disable regulation interval
******************************************************/
static ssize_t ar_enable_reg_write(struct file *filp,
                                const char __user *ubuf,size_t cnt, loff_t *ppos) {
    char buf[BUF_SIZE];
    memset(buf,sizeof(buf),0);
    u8 user_value = false ;

    if (copy_from_user(&buf, ubuf, (cnt > BUF_SIZE) ? BUF_SIZE: cnt) != 0)
    return 0;

    pr_info("%s: Received %s",__func__,buf);

    int ret = kstrtou8(buf, 10, &user_value);

    if (ret || (user_value > 1) ){
        pr_err("%s: Failed to update: Wrong value %u (error:%d)",__func__,user_value,ret);
        return -EINVAL;
    }

    if ( atomic_read(&enable_reg) == user_value ){
        pr_info("Regulation %s",(user_value?"Enabled":"Disabled"));
        return cnt;
    }

    atomic_set(&enable_reg,(user_value?true:false) );

    u8 cpu_id;
    for_each_online_cpu(cpu_id){
        switch(cpu_id){
            case 1:
            case 2:
            case 3:
            case 4:
                if (user_value){
                    start_regulation(cpu_id);
                }else{
                    stop_regulation(cpu_id);
                }
                break;
            default: continue;
        }
    }

    pr_info("Regulation %s",(user_value?"Enabled":"Disabled"));
    return cnt;
}

static int ar_enable_reg_read(struct seq_file *m, void *v)
{
    int tmp = atomic_read(&enable_reg);
    seq_printf(m, "%u \n",tmp);
    return 0;
}

static int ar_enable_reg_open(struct inode *inode, struct file *filp)
{
    return single_open(filp,ar_enable_reg_read , NULL);
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
static const struct file_operations ar_enable_reg = {
    .open       = ar_enable_reg_open,
    .write      = ar_enable_reg_write,
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
    debugfs_create_file("enable_regulation", 0444, ar_dir, NULL,
                        &ar_enable_reg);
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
