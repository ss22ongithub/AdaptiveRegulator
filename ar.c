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
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/smp.h> /* IPI calls */
#include <linux/irq_work.h>
#include <linux/hardirq.h>
#include <linux/perf_event.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <asm/atomic.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/notifier.h>
#include <linux/kthread.h>
#include <linux/printk.h>
#include <linux/interrupt.h>
#include <linux/trace_events.h>
#include <linux/cpumask.h>
#include <linux/topology.h>


#include <linux/init.h>
#include <linux/hw_breakpoint.h>
#include <linux/kstrtox.h>



#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 0, 0)
#  include <uapi/linux/sched/types.h>
#elif LINUX_VERSION_CODE > KERNEL_VERSION(4, 13, 0)
#  include <linux/sched/types.h>
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0)
#  include <linux/sched/rt.h>
#endif
#include <linux/sched.h>


/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define CACHE_LINE_SIZE 64
#define BUF_SIZE 256
#define TIMEOUT_NSEC ( 1000000000L )
#define TIMEOUT_SEC  ( 5 )


/**************************************************************************
 * COUNTERS Format (Umask_code - EventCode) tools/perf/pmu-events/arch/x86/)
 **************************************************************************/
#if defined(__aarch64__) || defined(__arm__)
#  define PMU_LLC_MISS_COUNTER_ID 0x17   // LINE_REFILL
#  define PMU_LLC_WB_COUNTER_ID   0x18   // LINE_WB
#elif defined(__x86_64__) || defined(__i386__)
#  define PMU_LLC_MISS_COUNTER_ID 0x08b0 // OFFCORE_REQUESTS.ALL_DATA_RD
#  define PMU_LLC_WB_COUNTER_ID   0x40b0 // OFFCORE_REQUESTS.WB
#  define PMU_STALL_L3_MISS_CYCLES_COUNTER_ID   0x06A3 //CYCLE_ACTIVITY.STALLS_L3_MISS, 
#endif

# define TOTAL_NRT_CORES  3 
# define TOAL_RT_CORES  1


/**************************************************************************
 * Function Declarations 
 **************************************************************************/

static inline u64 perf_event_count(struct perf_event *event);
extern int ar_init_debugfs(void);
extern void ar_remove_debugfs(void);

/**************************************************************************
 * Global Variables
 **************************************************************************/

static int g_read_counter_id = PMU_LLC_MISS_COUNTER_ID;
module_param(g_read_counter_id, hexint,  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
static int g_period_us=1000;
static const int regulation_interval_ms=1 ; //1ms 

/**************************************************************************
 * Public Types
 **************************************************************************/
/* percpu info */
struct core_info {
   
    /* for control logic */
    u32 read_budget;         /* assigned read budget */
    u32 write_budget;        /* assigned write budged */

    u32 read_count;
    u32 write_count;

    struct irq_work read_pending;  /* delayed work for NMIs */
    struct perf_event *read_event; /* PMC: LLC misses */
    
    u32 budget_next; /* Per core budget for the NRT core for the next regulation interval , b__i_r */
    
    /* per-core hr timer */

    struct hrtimer hr_timer;
};
/* TODO: Remove once core_info is used*/
struct perf_event *read_event; /* PMC: LLC misses */
/**************************************************************************
 * Perf Structure definitions 
 **************************************************************************/
struct my_perf_event {
    struct perf_event *event;
    struct perf_event_attr attr;
    u64 count;
};

static struct perf_event *llc_misses_event=NULL;
static struct irq_work llc_miss_event_irq_work;
static struct core_info __percpu *core_info;

/**************************************************************************
 * DebugFs functionalities
 **************************************************************************/

#define DEBUGFS_BUF_SIZE 256

static u32 ar_regulation_time_ms = 1000; //ms, default 1000ms
static u32 ar_observation_time_ms = 1000; //ms, default 1000ms
static struct dentry *ar_dir = NULL;




/**************************************************************************
 * Utils
 **************************************************************************/

/** convert MB/s to #of events (i.e., LLC miss counts) per 1ms */
static inline u64 convert_mb_to_events(int mb)
{
    return div64_u64((u64)mb*1024*1024,
             CACHE_LINE_SIZE * (1000000/g_period_us));
}


static inline void print_current_context(void)
{
    trace_printk("in_interrupt(%ld)(hard(%ld),softirq(%d)"
             ",in_nmi(%d)),irqs_disabled(%d)\n",
             in_interrupt(), in_irq(), (int)in_softirq(),
             (int)in_nmi(), (int)irqs_disabled());
}


/**************************************************************************
 * Callbacks and Handlers
 **************************************************************************/

static void llc_miss_event_irq_work_handler(struct irq_work *entry){
    // trace_printk("%s: Enter\n",__func__);
    // print_current_context();
    // u64 count = perf_event_count(my_event);
    u64 llc_misses_count =  perf_event_count(llc_misses_event);
    trace_printk("%s: count = %llu\n", __func__,llc_misses_count);
    // pr_info("llc_misses_count=%lld\n",llc_misses_count);
}

static void event_read_overflow_callback(struct perf_event *event,
                    struct perf_sample_data *data,
                    struct pt_regs *regs)
{
    //irq_work_queue(&llc_miss_event_irq_work);
}
static struct task_struct* thread_kt1;

static int thread_kt1_func(void * data){
    int cpunr = (unsigned long)data;
    pr_info("%s: Enter",__func__);

    while (!kthread_should_stop() && cpu_online(cpunr)) {
        pr_info("%s:Looping on CPU%d",__func__,smp_processor_id());
        msleep(2000);
        if (kthread_should_stop())
            break;
    }

    pr_info("%s: Exit",__func__);
    return 0;
}


/**************************************************************************
 * Perf Utils
 **************************************************************************/
static u64 start_t = 0; 
static enum hrtimer_restart ar_regu_timer_callback(struct hrtimer *timer)
{
     /* do your timer stuff here */
    pr_info("%s:",__func__);
    /*Stop the counter*/
    read_event->pmu->stop(read_event, PERF_EF_UPDATE);
    u64 c = perf_event_count(read_event);
    trace_printk("\n%s: read_event counter = %llu",__func__,c);


    /* Compute previous utilization */


    /* forward timer */
    int orun = hrtimer_forward_now(timer, ms_to_ktime(ar_regulation_time_ms));
    BUG_ON(orun == 0);

    /*Re enabled the counter*/
    read_event->pmu->start(read_event, PERF_EF_RELOAD);
    return HRTIMER_RESTART;
}


static struct hrtimer ar_regu_timer ;
static struct hrtimer ar_obs_timer;

/**************************************************************************
 * Perf Utils
 **************************************************************************/

/** read current counter value. */
static inline u64 perf_event_count(struct perf_event *event)
{
    return local64_read(&event->count) + 
        atomic64_read(&event->child_count);
}

static struct perf_event *init_counter(int cpu, int sample_period, int counter_id, void *callback)
{
    struct perf_event *event = NULL;
    struct perf_event_attr sched_perf_hw_attr = {
        .type       = PERF_TYPE_RAW,
        .size       = sizeof(struct perf_event_attr),
        .pinned     = 1,
        .disabled   = 1,
        .sample_period = sample_period,
        .config         = counter_id,
        .exclude_kernel = 1   /* TODO: 1 mean, no kernel mode counting */
    };

    /* Try to register using hardware perf events */
    event = perf_event_create_kernel_counter(
        &sched_perf_hw_attr,
        cpu, /* CPU */
        NULL,   /* struct task_struct *task */
        callback,
        NULL);  /* void *context */

    if (!event)
        return NULL;

    if (IS_ERR(event)) {
        /* vary the KERN level based on the returned errno */
        if (PTR_ERR(event) == -EOPNOTSUPP)
            pr_info("cpu%d. not supported\n", cpu);
        else if (PTR_ERR(event) == -ENOENT)
            pr_info("cpu%d. not h/w event\n", cpu);
        else
            pr_err("cpu%d. unable to create perf event: %ld\n",
                   cpu, PTR_ERR(event));
        return NULL;
    }

    pr_info("CPU%d configured counter 0x%x\n", cpu, counter_id);
    return event;
}



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

int ar_init_debugfs(void)
{

    ar_dir = debugfs_create_dir("ar", NULL);
    BUG_ON(!ar_dir);
    debugfs_create_file("regu_interval", 0444, ar_dir, NULL,
                &ar_reg_interval_fops);
    debugfs_create_file("obs_interval", 0444, ar_dir, NULL,
                &ar_obs_interval_fops);
    return 0;
}

void ar_remove_debugfs(void){

    debugfs_remove_recursive(ar_dir);
}

/**************************************************************************************************************************
 * Module main
 **************************************************************************************************************************/

static int __init ar_init (void ){
    int cpu_id = 0;
    pr_info("NR_CPUS: %d, online_cpus: %d\n", NR_CPUS, num_online_cpus());


    ar_init_debugfs();

    read_event =  init_counter(cpu_id,
                        convert_mb_to_events(1000),
                        g_read_counter_id,
                        event_read_overflow_callback);

    if (read_event == NULL){
	    pr_err("read_event %p did not allocate ", read_event);
        return -1;
	}
    perf_event_enable(read_event);
    

#if 0
   thread_kt1 = kthread_create_on_node(thread_kt1_func,
                                       (void *)((unsigned long)cpu_id),
                                       cpu_to_node(cpu_id),
                                       "kt/%d",cpu_id);

	BUG_ON(IS_ERR(thread_kt1));
	kthread_bind(thread_kt1, cpu_id);
    wake_up_process(thread_kt1);
#endif
    /* initialize irq_work_queue */
    //init_irq_work(&llc_miss_event_irq_work, llc_miss_event_irq_work_handler);

    hrtimer_init( &ar_regu_timer, CLOCK_MONOTONIC , HRTIMER_MODE_REL_PINNED);
    ar_regu_timer.function=&ar_regu_timer_callback;
    hrtimer_start( &ar_regu_timer, ms_to_ktime(ar_regulation_time_ms), HRTIMER_MODE_REL_PINNED  );


    pr_info("ar: Module Initialized\n");
    return 0;

}


static void __exit ar_exit( void )
{
    if (read_event){
        perf_event_disable(read_event);
        perf_event_release_kernel(read_event);
        read_event= NULL;
        pr_info("Perf event disabled\n");
    }
    //kthread_stop(thread_kt1);
    hrtimer_cancel(&ar_regu_timer);

    ar_remove_debugfs();
	pr_info("ar: Module removed\n");
	return;
}

module_init(ar_init);
module_exit(ar_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sudarshan S <sudarshan.srinivasan@research.iiit.ac.in>");
