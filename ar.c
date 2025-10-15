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
#include "master.h"
#include "ar_debugfs.h"
#include "ar_perfs.h"
#include "utils.h"
#include "model.h"

/**************************************************************************
 * Public Definitions
 **************************************************************************/





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

/**************************************************************************
 * Local Function Declarations
 **************************************************************************/

static void deinitialize_cpu_info(const u8 cpu_id);
static int  setup_cpu_info(const u8 cpu_id);
static void ar_handle_read_overflow(struct irq_work *entry);
static enum hrtimer_restart new_ar_regu_timer_callback(struct hrtimer *timer);

/**************************************************************************
 * External Function Declarations
 **************************************************************************/

/**************************************************************************
 * Global Variables
 **************************************************************************/

static int g_read_counter_id = PMU_LLC_MISS_COUNTER_ID;
module_param(g_read_counter_id, hexint,  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

u64 g_bw_intial_setpoint_mb[MAX_NO_CPUS+1] = {0,1000,1000,1000,1000}; /*Pre-defined initial / min Bandwidth in MB/s */
u64 g_bw_max_mb[MAX_NO_CPUS+1] = {0,30000,30000,30000,30000}; /*Pre-defined max Bandwidth per core in MB/s */

/**************************************************************************
 * Public Types
 **** **********************************************************************/

static struct core_info all_cinfo[MAX_NO_CPUS + 1];

struct core_info* get_core_info(u8 cpu_id){
    switch(cpu_id){
        case 1:
        case 2:
        case 3:
        case 4:
            return &all_cinfo[cpu_id];
        default:
            pr_err("Invalid CPU ID %u !!!", cpu_id);
            return NULL;
    }
}



/**************************************************************************
 * Utils
 **************************************************************************/



/**************************************************************************/

static void __start_timer_on_cpu(void* cpu)
{
    u8 cpu_id = (u8)(uintptr_t)cpu;
    struct core_info* cinfo = get_core_info(cpu_id);
    BUG_ON(!cinfo);


    /* start timer */
        hrtimer_start(&cinfo->reg_timer, ms_to_ktime(get_regulation_time()),
                      HRTIMER_MODE_REL_PINNED);

}

/**************************************************************************
 * Callbacks and Handlers
 **************************************************************************/

static enum hrtimer_restart new_ar_regu_timer_callback(struct hrtimer *timer)
{
    u8 cpu_id = smp_processor_id();
//    trace_printk("\n");

    struct core_info *cinfo =  get_core_info(cpu_id);
    BUG_ON(!cinfo);

    /* 
       Stop the counter and determine the used count in 
       the previous regulation interval
    */
    cinfo->read_event->pmu->stop(cinfo->read_event, PERF_EF_UPDATE);

    u64 read_event_new_budget = atomic64_read(&cinfo->budget_est);
    local64_set(&cinfo->read_event->hw.period_left, read_event_new_budget);
    trace_printk("CPU(%u):New budget: %llu\n",cpu_id,read_event_new_budget);

    //un-throttle if the core is in throttle state
    atomic_set(&cinfo->throttler_task,false);

    hrtimer_forward_now(timer, ms_to_ktime(get_regulation_time()));

    /*Re-enabled the counter*/
    cinfo->read_event->pmu->start(cinfo->read_event, PERF_EF_RELOAD);

    /*Re-enabled the timer*/
    return HRTIMER_RESTART;
}


static int throttler_task_func1(void * data){
    u8 cpu_id = (unsigned long)data;
    pr_info("%s: Enter CPU(%d)",__func__,cpu_id);

    struct core_info *cinfo = get_core_info(cpu_id);//per_cpu_ptr(core_info, cpunr);
    BUG_ON(cinfo == NULL);
    sched_set_fifo(current);

    while (!kthread_should_stop() && cpu_online(cpu_id)) {

        trace_printk("CPU(%d):Waiting for Event\n", cpu_id);
        wait_event_interruptible(cinfo->throttle_evt,
                                 atomic_read(&cinfo->throttler_task)
                                 || kthread_should_stop() );
        trace_printk("CPU(%d):Got Event\n", cpu_id);

        if (kthread_should_stop())
            break;

        trace_printk("CPU(%d):Throttling...\n",cpu_id);
       while (atomic_read(&cinfo->throttler_task)
                 && !kthread_should_stop())
       {
           smp_mb();
           cpu_relax();
           /* TODO: mwait */
       }
    }

    pr_info("%s: Exit",__func__);
    return 0;
}

/* Callback when read counter exhuasts its budget*/
static void read_event_overflow_callback(struct perf_event *event,
                    struct perf_sample_data *data,
                    struct pt_regs *regs)
{
    u8 cpu_id = smp_processor_id();
    if (cpu_id == 0){
        trace_printk("%s: CPU(%d) not expected here \n",__func__,cpu_id);
        return;
    }

    struct core_info *cinfo = get_core_info(cpu_id);
    BUG_ON(!cinfo);
    irq_work_queue(&cinfo->read_irq_work);
}

static void ar_handle_read_overflow(struct irq_work *entry)
{
    u8 cpu_id = smp_processor_id();
    if (cpu_id == 0){
        trace_printk("%s: CPU(%d) not expected here\n",__func__,cpu_id);
        return;
    }

    BUG_ON(in_nmi() || !in_irq());
    trace_printk("\n");

    struct core_info *cinfo = get_core_info(cpu_id);
    BUG_ON(!cinfo);

    //Activate Throttling
    atomic_set(&cinfo->throttler_task,true);
    wake_up_interruptible(&cinfo->throttle_evt);

}


/**************************************************************************
 * Other Utils
 **************************************************************************/

static int  setup_cpu_info(const u8 cpu_id){
    pr_info("%s: Enter CPU(%d)", __func__,cpu_id );
    struct core_info* cinfo = get_core_info(cpu_id);
    BUG_ON(cinfo==NULL);
    memset(cinfo, sizeof(struct core_info), 0);
    cinfo->cpu_id = cpu_id;


    /* Initialize NMI irq_work_queue */
    init_irq_work(&cinfo->read_irq_work, ar_handle_read_overflow);

    /* Disable the throttle flag */
    atomic_set(&cinfo->throttler_task,false);   
    

    /* Initialize Wait queue for throttler */
    init_waitqueue_head(&cinfo->throttle_evt);

    /* TODO: Investigate kthread_run_on_cpu API which is  a convenience wrapper
     * for kthread_creat_on_node + kthread_bind + wake_up_process.
     * This has an issue the incorrect cpuid is displayed in the ps command.
     * This needs to be reconfirmed.
     * https://docs.kernel.org/next/driver-api/basics.html
     * */
#if defined(TODO)
    cinfo->throttler_thread = kthread_run_on_cpu(throttler_task_func1,
                                    (void *)((unsigned long)cpu_id),
                                    cpu_to_node(cpu_id),
                                    "areg_kthrottler/%u",cpu_id);

    BUG_ON(IS_ERR(cinfo->throttler_task));
#else
    cinfo->throttler_thread = kthread_create_on_node(throttler_task_func1,
                                    (void *)((unsigned long)cpu_id),
                                    cpu_to_node(cpu_id),
                                    "areg_kthrottler/%u",cpu_id);

    BUG_ON(IS_ERR(cinfo->throttler_thread));
    kthread_bind(cinfo->throttler_thread, cpu_id);
    wake_up_process(cinfo->throttler_thread);
#endif

    /* Initialize the regulation timer. However the timer will be started using @ __start_timer() */
    hrtimer_init(&cinfo->reg_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
    cinfo->reg_timer.function = &new_ar_regu_timer_callback;

    /***** Regulation to be started by setting
    /sys/kernel/debug/ar/enable_regulation to 1 ****/

    /* Initiialize weight matrix to predefined values */
    initialize_weight_matrix(cinfo);

    pr_info("%s: Exit", __func__ );
    return 0;
}

static void deinitialize_cpu_info( const u8 cpu_id){

    pr_info("%s:Enter CPU (%d)",__func__, cpu_id );
    
    struct core_info *cinfo = get_core_info(cpu_id);
    
    BUG_ON(cinfo == NULL);
        
    /* As much as possible keep the de-initialization in the reverse sequence of initialization
     * Refer: setup_cpu_info()
     * */
    
    // Stop the timer. 
    // WARNING: Ensure timer is intialized before cancelling
    
    hrtimer_cancel(&cinfo->reg_timer);
    
    //End the throttle thread
    if(cinfo->throttler_thread){
        kthread_stop(cinfo->throttler_thread);
        atomic_set(&cinfo->throttler_task,false);
        cinfo->throttler_thread = NULL;
    }
    
    //Free the perf event counter
    if (cinfo->read_event) {
        disable_event(cinfo->read_event);
        cinfo->read_event= NULL;
    }
    pr_info("%s:Exit",__func__ );
}

bool start_perf_counters(u8 cpu_id){
    struct core_info* cinfo = get_core_info(cpu_id);
    BUG_ON(cinfo==NULL);
    cinfo->read_event =  init_counter(cinfo->cpu_id,
                                      convert_mb_to_events(g_bw_intial_setpoint_mb[cpu_id]),
                                      g_read_counter_id,
                                      NULL);
    if (cinfo->read_event == NULL){
        pr_err("Read_event %p did not allocate ", cinfo->read_event);
        return false;
    }

    /* Enable perf event */

    enable_event(cinfo->read_event);
    pr_info("Read event started");
    return true;
}

void stop_perf_counters(u8 cpu_id){
    struct core_info* cinfo = get_core_info(cpu_id);
    BUG_ON(cinfo==NULL);
    /* Disable perf event */
    disable_event(cinfo->read_event);
    pr_info("Read event stopped");
}

bool start_regulation(u8 cpu_id){
    struct core_info* cinfo = get_core_info(cpu_id);
    BUG_ON(cinfo==NULL);
    cinfo->next_estimate=0;
    cinfo->prev_estimate=0;
    /* Cleanup the exiting perf counters and recreate the perf counters.
       This time we register the callback */
    disable_event(cinfo->read_event);
    cinfo->read_event =  init_counter(cinfo->cpu_id,
                                      convert_mb_to_events(g_bw_intial_setpoint_mb[cpu_id]),
                                      g_read_counter_id,
                                      read_event_overflow_callback);
    if (cinfo->read_event == NULL){
        pr_err("Read_event %p did not allocate ", cinfo->read_event);
        return false;
    }

    /* Start the timer on the specific core*/
    smp_call_function_single(cpu_id,__start_timer_on_cpu,(void*)(long)cpu_id,false);
    pr_info("%s: Exit: (CPU %u)",__func__,cpu_id );
    return true;
}

void stop_regulation(u8 cpu_id){
    struct core_info* cinfo = get_core_info(cpu_id);
    BUG_ON(cinfo==NULL);

    /* Stop the timer running on the specific core. Even if the timer
     is pinned to a core , it can be cancelled from any other core*/
    disable_event(cinfo->read_event);
    hrtimer_cancel(&cinfo->reg_timer);

    pr_info("%s: Exit: (CPU %u)",__func__,cpu_id );
}
/**************************************************************************************************************************
 * Module main
 **************************************************************************************************************************/

static int __init ar_init (void ){

    pr_info("Supported CPUs: %d, online_cpus: %d\n", NR_CPUS, num_online_cpus());
//    pr_info("FPU supported : %d",kernel_fpu_available());

    //Initialise core infos
    memset(all_cinfo, 0, sizeof(all_cinfo));

    //Setup CPU info for CPU 1,2, 3, 4
    int ret = setup_cpu_info((u8)1);
    if (ret != 0){
        pr_err("setup_cpu() Failed");
        deinitialize_cpu_info(1);
        return -ENOMEM;
    }

    ret = setup_cpu_info((u8)2);
    if (ret != 0){
        pr_err("setup_cpu() Failed");
        deinitialize_cpu_info((u8)2);
        deinitialize_cpu_info((u8)1);
        return -ENOMEM;
    }

    ret = setup_cpu_info((u8)3);
    if (ret != 0){
        pr_err("setup_cpu() Failed");
        deinitialize_cpu_info((u8)3);
        deinitialize_cpu_info((u8)2);
        deinitialize_cpu_info((u8)1);
        return -ENOMEM;
    }

    ret = setup_cpu_info((u8)4);
    if (ret != 0){
        pr_err("setup_cpu() Failed");
        deinitialize_cpu_info((u8)4);
        deinitialize_cpu_info((u8)3);
        deinitialize_cpu_info((u8)2);
        deinitialize_cpu_info((u8)1);
        return -ENOMEM;
    }

    /* Initialize the master thread */
    initialize_master();

    /* Create entries in debugfs */
    ar_init_debugfs();

    pr_info("Module Initialized\n");
    return 0;

}


static void __exit ar_exit( void )
{
    /* Keep the deinitializing sequence reverse of the allocation sequence seen in  __init function */
    ar_remove_debugfs();
    
    deinitialize_master();
    
    deinitialize_cpu_info((u8)1);
    
    deinitialize_cpu_info((u8)2);
    
    deinitialize_cpu_info((u8)3);
    
    deinitialize_cpu_info((u8)4);

    pr_info("Module removed\n");
	return;
}

module_init(ar_init);
module_exit(ar_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sudarshan S <sudarshan.srinivasan@research.iiit.ac.in>");

