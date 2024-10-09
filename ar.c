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


#include "ar_debugfs.h"
#include "ar_perfs.h"
#include "ar.h"


/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define CACHE_LINE_SIZE 64
#define TIMEOUT_NSEC ( 1000000000L )
#define TIMEOUT_SEC  ( 5 )
#define MAX_NO_CPUS 6

#define DEBUG(x)


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

// # define TOTAL_NRT_CORES  3 
// # define TOAL_RT_CORES  1


/**************************************************************************
 * Function Declarations 
 **************************************************************************/




/**************************************************************************
 * Global Variables
 **************************************************************************/

static int g_read_counter_id = PMU_LLC_MISS_COUNTER_ID;
module_param(g_read_counter_id, hexint,  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
// static int g_period_us=1000;
static u64 g_bw_setpoints_mb[MAX_NO_CPUS] = {100,200,300, 400, 500, 600}; /*Bandwidth setpoints in MB/s */
static u64 g_bw_max_mb[MAX_NO_CPUS] = {1000,1000,1000, 1000, 1000, 1000}; /*Bandwidth setpoints in MB/s */
static ktime_t last_time;

/**************************************************************************
 * Public Types
 **************************************************************************/

static struct core_info* cinfo = NULL;

struct core_info* get_core_info(void){
    return cinfo;
}

// static struct core_info __percpu *core_info;




/**************************************************************************
 * Utils
 **************************************************************************/

/** convert MB/s to #of events (i.e., LLC miss counts) per 1ms */
static inline u64 convert_mb_to_events(int mb)
{
    return div64_u64((u64)mb*1024*1024,
             CACHE_LINE_SIZE * (1000/get_regulation_time()));
}
static inline int convert_events_to_mb(u64 events)
{
	/*
	 * BW  = (event * CACHE_LINE_SIZE)/ time_in_ms  - bytes/ms 
	 *     =  (event * CACHE )/ (time_in_ms * 1024 *1024)  = mb/ms
	 *     =  (event * CACHE * 1000 )/ (time_in_sec * 1024 *1024)  = mb/s
	 */ 
    u32 ar_regulation_time_ms = get_regulation_time();
	int divisor = ar_regulation_time_ms*1024*1024;
	int mb = div64_u64(events*CACHE_LINE_SIZE*1000, divisor);
	return mb;
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



static struct task_struct* thread_kt1 = NULL;

static int thread_kt1_func(void * data){
    int cpunr = (unsigned long)data;
    pr_info("%s: Enter",__func__);

    
    struct core_info *cinfo = get_core_info();//per_cpu_ptr(core_info, cpunr);

    sched_set_fifo(current);

    while (!kthread_should_stop() && cpu_online(cpunr)) {

        DEBUG(trace_printk("wait an event\n"));
        wait_event_interruptible(cinfo->throttle_evt,
                     thread_kt1 ||
                     kthread_should_stop());

        DEBUG(trace_printk("got an event\n"));

        if (kthread_should_stop())
            break;

        while (thread_kt1 && !kthread_should_stop())
        {
            smp_mb();
            cpu_relax();
            /* TODO: mwait */
        }
    }

    pr_info("%s: Exit",__func__);
    return 0;
}

/**************************************************************************
 * Other Utils
 **************************************************************************/

static void update_error(int error) {

    static long int error_accumulator = 0;
    error_accumulator += error;
}

static int do_pid_control(int error){

    static int last_error = 0;
    static int Kp = 1;
    static int Ki = 1;
    static int Kd = 1;
    static int integral=0;

    ktime_t current_time  = ktime_get();
    u32 time_diff = ktime_to_ms(ktime_sub(current_time, last_time));
    //error = setpoint bw - used bw 
    if (error > 0 ){
        // there is unused bw 

    }else if (error < 0) {

    }else {
        // no difference 
        return 0;
    }

    /* Proportional term */
    int P = Kp*error;

    /* Integral term */
    integral += error*time_diff;
    int I = Ki * integral/(1000);

    // /* Derivative term */
    // derivative = (error-last_error)/time_diff;
    // D = Kd * derivative/1000;
    int D = 0;

    int out = P + I + D;
    trace_printk("P=%d,I=%d,D=%d\n", P, I , D );

    last_time = current_time;
    last_error = error;

    return out;

}




static enum hrtimer_restart ar_regu_timer_callback(struct hrtimer *timer)
{
     /* do your timer stuff here */
	// pr_info("%s:",__func__);

    struct perf_event *read_event = get_read_event();
    struct core_info *cinfo = get_core_info();
    /*Stop the counter*/
    read_event->pmu->stop(read_event, PERF_EF_UPDATE);
    cinfo->g_read_count_new = perf_event_count(read_event);
    
    s64 read_count_used = cinfo->g_read_count_new - cinfo->g_read_count_old;
	s64 used_bw_mb = convert_events_to_mb(read_count_used);
	cinfo->g_read_count_old = cinfo->g_read_count_new;

    s64 setpoint_cpu_bw_mb = g_bw_setpoints_mb[cinfo->cpu_id];
    s64 error = setpoint_cpu_bw_mb - used_bw_mb;
    // update_error(error);

    s64 correction_mb = do_pid_control(error);

    s64 read_event_new_budget = convert_mb_to_events(g_bw_setpoints_mb[cinfo->cpu_id]); //convert_mb_to_events(correction_mb);

    // if (read_event_new_budget > g_bw_max_mb[cpu_id]  ){
    //     read_event_new_budget = g_bw_max_mb[cpu_id];
    // }
    trace_printk("used_bw_mb= %lld, error_mb=%lld, setpoint_cpu_bw_mb=%lld,correction_mb=%lld \n",used_bw_mb,error,setpoint_cpu_bw_mb,correction_mb);
    trace_printk("ovflo=%lld",get_llc_ofc());
    local64_set(&read_event->hw.period_left, read_event_new_budget);


    /* forward timer */
    int orun = hrtimer_forward_now(timer, ms_to_ktime(get_regulation_time()));
    BUG_ON(orun == 0);

    /*Re-enabled the counter*/
    read_event->pmu->start(read_event, PERF_EF_RELOAD);
    return HRTIMER_RESTART;
    
}


static struct hrtimer ar_regu_timer ;


/**************************************************************************************************************************
 * Module main
 **************************************************************************************************************************/

static int __init ar_init (void ){
    
    pr_info("NR_CPUS: %d, online_cpus: %d\n", NR_CPUS, num_online_cpus());

    cinfo = (struct core_info*)kzalloc(sizeof (struct core_info), GFP_KERNEL);

    /*

    u32 g_read_count_new;
    u32 g_read_count_old;
    int cpu_id;
    wait_queue_head_t throttle_evt;
    */

    cinfo->cpu_id = 5;


    ar_init_debugfs();

    init_perf_workq();  

    struct perf_event*  read_event =  init_counter(cinfo->cpu_id,
                                    convert_mb_to_events(g_bw_setpoints_mb[cinfo->cpu_id]),
                                    g_read_counter_id,
                                    event_read_overflow_callback);
                

    if (read_event == NULL){
	    pr_err("read_event %p did not allocate ", read_event);
        return -1;
	}
   

    set_read_event(read_event);
    enable_event(read_event);
    
     

   thread_kt1 = kthread_create_on_node(thread_kt1_func,
                                       (void *)((unsigned long)cinfo->cpu_id),
                                       cpu_to_node(cpu_id),
                                       "kt/%d",cinfo->cpu_id);

	BUG_ON(IS_ERR(thread_kt1));
	kthread_bind(thread_kt1, cpu_id);
    wake_up_process(thread_kt1);

    hrtimer_init( &ar_regu_timer, CLOCK_MONOTONIC , HRTIMER_MODE_REL_PINNED);
    ar_regu_timer.function=&ar_regu_timer_callback;
    last_time = ktime_get();
    hrtimer_start( &ar_regu_timer, ms_to_ktime(get_regulation_time()), HRTIMER_MODE_REL_PINNED  );


    pr_info("ar: Module Initialized\n");
    
    return 0;

}


static void __exit ar_exit( void )
{
    struct perf_event *read_event = get_read_event();
    if (read_event){
        disable_event(read_event);
        // read_event= NULL;
    }
    //kthread_stop(thread_kt1);
    hrtimer_cancel(&ar_regu_timer);

    ar_remove_debugfs();

    kfree(get_core_info());
	pr_info("ar: Module removed\n");
	return;
}

module_init(ar_init);
module_exit(ar_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sudarshan S <sudarshan.srinivasan@research.iiit.ac.in>");
