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



#define MAX_BW_SAMPLES 20

//#define AREG_USE_FIFO

#if defined (AREG_USE_FIFO)
#define ERR_HIST_FIFO_SIZE 64
static DECLARE_KFIFO_PTR(err_hist_fifo, s64);
static DECLARE_KFIFO_PTR(err_hist_fifo_D, s64);
#endif

const struct bw_distribution rd_bw_setpoints[MAX_BW_SAMPLES] = {
    {.time = 1, .rd_avg_bw = 1},
    {.time = 2, .rd_avg_bw=3217},
    {.time = 3, .rd_avg_bw=4384},
    {.time = 4, .rd_avg_bw=4761},
    {.time = 5, .rd_avg_bw=4804},
    {.time = 6, .rd_avg_bw=4256},
    {.time = 7, .rd_avg_bw=4844},
    {.time = 8, .rd_avg_bw=4834},
    {.time = 9, .rd_avg_bw=4975},
    {.time = 10, .rd_avg_bw=3558},
    {.time = 11, .rd_avg_bw=3948},
    {.time = 12, .rd_avg_bw=4314},
    {.time = 13, .rd_avg_bw=4531},
    {.time = 14, .rd_avg_bw=4491},
    {.time = 15, .rd_avg_bw=4532},
    {.time = 16, .rd_avg_bw=4544},
    {.time = 17, .rd_avg_bw=4530},
    {.time = 18, .rd_avg_bw=4523},
    {.time = 19, .rd_avg_bw=599},
    {.time = 20, .rd_avg_bw=1}
};

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define CACHE_LINE_SIZE 64
#define TIMEOUT_NSEC ( 1000000000L )
#define TIMEOUT_SEC  ( 5 )
#define MAX_NO_CPUS 4

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

static void deinitialize_cpu_info(struct core_info *cinfo , u8 cpu_id);
static int  setup_cpu_info(struct core_info* cinfo, u8 cpu_id);



/**************************************************************************
 * Global Variables
 **************************************************************************/

static int g_read_counter_id = PMU_LLC_MISS_COUNTER_ID;
module_param(g_read_counter_id, hexint,  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
// static int g_period_us=1000;
static u64 g_bw_intial_setpoint_mb[MAX_NO_CPUS+1] = {0,1000,1000,1000,1000}; /*Bandwidth setpoints in MB/s */
// static u64 g_bw_max_mb[MAX_NO_CPUS+1] = {0,2000,2000, 2000, 2000}; /*Bandwidth setpoints in MB/s */
static ktime_t last_time;

static struct utilization u = {0};


/**************************************************************************
 * Public Types
 **** **********************************************************************/

static struct core_info cinfo;

struct core_info* get_core_info(void){
    return &cinfo;
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

/* Convert # of events to MB/s */
static inline int convert_events_to_mb(u64 events)
{
	/*Linux Kernel Programming (Kaiwan N Billimoria)


	 * BW  = (event * CACHE_LINE_SIZE)/ time_in_ms  - bytes/ms 
	 *     =  (event * CACHE )/ (time_in_ms * 1024 *1024)  = mb/ms
	 *     =  (event * CACHE * 1000 )/ (time_in_sec * 1024 *1024)  = mb/s
	 */ 
    int divisor = get_regulation_time()*1024*1024;
	int mb = div64_u64(events*CACHE_LINE_SIZE*1000 + (divisor-1), divisor);
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


//TODO: instatiate according to the number of core to be regulated
static struct task_struct* thread_kt1 = NULL;

static int thread_kt1_func(void * data){
    u8 cpunr = (unsigned long)data;
    pr_info("%s: Enter",__func__);

    
    struct core_info *cinfo = get_core_info();//per_cpu_ptr(core_info, cpunr);

    sched_set_fifo(current);

    while (!kthread_should_stop() && cpu_online(cpunr)) {

        trace_printk("Wait for Event\n");
        ssleep(5);
//        wait_event_interruptible(cinfo->throttle_evt,
//                     cinfo->throttled_task ||
//                     kthread_should_stop());
//
//        if (kthread_should_stop())
//            break;
//
//        trace_printk("Throttling CPU%u...\n",cpunr);
//        while (cinfo->throttled_task && !kthread_should_stop())
//        {
//            smp_mb();
//            cpu_relax();
//            /* TODO: mwait */
//        }

    }

    pr_info("%s: Exit",__func__);
    return 0;
}

/**************************************************************************
 * Other Utils
 **************************************************************************/
/* Proportional Parameters */
static const s64 Kp_inv = 9;  // Kp=1/9

/* Integral Parameters */
static const s64 Ki_inv = 50; // Ki=1/50
static const s64 Kd_inv = 5;  //Kd=5
static const s64 Ti = 30;
static const s64 Td = 20;

static s64 do_pid_control(s64 error){

    // static ktime_t last_time = 0;

    static s64 sum_of_err=0;
    
    // error removed from sum computation
    s64 error_removed_I = 0;
    s64 error_removed_D = 0;

#if defined (AREG_USE_FIFO)
    int ret = kfifo_in(&err_hist_fifo, &error, 1);    
    if (ret != 1){
        trace_printk("AREG: err_hist_fifo add Failed");
    }else{
        trace_printk("AREG: err_hist_fifo added\n");
    }

    ret = kfifo_in(&err_hist_fifo_D, &error, 1);    
    if (ret != 1){
        trace_printk("AREG: err_hist_fifo_D add Failed");
    }else{
        trace_printk("AREG: err_hist_fifo_D added\n");
    }

    if (kfifo_len(&err_hist_fifo) > Ti){
        trace_printk("AREG: err_hist_fifo: len > %lld \n",Ti);
        ret = kfifo_out(&err_hist_fifo, &error_removed_I, 1);
        if (ret != 1) {
            pr_err("ar: err_hist_fifo remove Failed\n");
        }
    }

    if (kfifo_len(&err_hist_fifo_D) > Td){
        trace_printk("AREG: err_hist_fifo_D: len > %lld \n",Ti);
        ret = kfifo_out(&err_hist_fifo_D, &error_removed_D, 1);
        if (ret != 1) {
            pr_err("ar: err_hist_fifo remove Failed\n");
        }
    }

#endif
    /* Time */
    // ktime_t current_time  = ktime_get();

    /* Proportional term:  P = Kp * error_mb     */
    s64 P = div64_s64(error,Kp_inv);

    /* Integral term : I  = sigma(error_mb of last  T1 samples) / Ki_inv
        
        Initially :sum_p = (e1 + ...+ e_x) / T1

        subsequently new error value = e_y
        => sum = (e2 + ...+ e_x + e_y) / T1  = (sum_p - t1 + ty)/T1
           I = sum / K_inv 
     */

    sum_of_err = sum_of_err + error - error_removed_I;
    trace_printk("AREG: err=%lld,error_removed_I=%lld\n",error, error_removed_I);
    s64 I = div64_s64 (sum_of_err,(Ti * Ki_inv));

    // /* Derivative term */
    // derivative = (error-last_error)/time_diff;
    // D = Kd * derivative/1000;
    s64 D = div64_s64( (error - error_removed_D), (Td * Kd_inv) );

    /*TO DO: removed after tuning*/
    
    s64 out = P + I + D;
    trace_printk("AREG:%s: P=%lld I=%lld D=%lld out=%lld\n",__func__, P, I, D,out);


    // last_time = current_time;
    
    return out;
}


static u32 get_setpoint(void){
    
    if(1){
        return 3000;
    }

    static u8 sp_idx = 0;
    static u32 samples = 0;
    u32 sp = 0 ;
    const u32 samples_to_wait = 100;
    if (samples > samples_to_wait){
        samples = 0;
        sp_idx++;
        if ( sp_idx >= MAX_BW_SAMPLES ) {
            trace_printk("AREG: Error: sp_idx %u > %u. Resetting", sp_idx, MAX_BW_SAMPLES);
            sp_idx = 0;
        }
    }
    samples++;
    sp = rd_bw_setpoints[sp_idx].rd_avg_bw;
    
    trace_printk("AREG: sp_idx=%u rd_bw_setpoint=%u \n",sp_idx,sp);

    return sp;
}


static void update_stats(u64 cur_used_bw_mb){
    trace_printk("AREG: %s\n",__func__);
    u8 ar_sw_size = get_sliding_window_size();

    u.used_bw_mb_list[u.used_bw_idx++] = cur_used_bw_mb;
    if (u.used_bw_idx >= ar_sw_size){
        u.used_bw_idx = 0;
    }
    
    /* Calculate sliding window average of used bandwdths */
    u64 tmp_avg = 0;
    for(u8 i=0 ; i < ar_sw_size; i++){
        tmp_avg += u.used_bw_mb_list[i];
    }    
    u.used_avg_bw_mb = div64_u64(tmp_avg,ar_sw_size);   
}


static enum hrtimer_restart ar_regu_timer_callback(struct hrtimer *timer)
{
	static s64 setpoint_bw_mb = 0;

    struct perf_event *read_event = get_read_event();
    struct core_info *cinfo = get_core_info();

    /* Stop the counter and determine the used count in 
       the previous regulation interval
    */
    read_event->pmu->stop(read_event, PERF_EF_UPDATE);
    cinfo->g_read_count_new = perf_event_count(read_event);
    s64 read_count_used = cinfo->g_read_count_new - cinfo->g_read_count_old;

    if (!read_count_used){
        // No change in the counter , so no  request was sent => possibly no load running on the core 
        trace_printk("AREG: No change in read counter on core%u.\n",cinfo->cpu_id);
        /* forward timer */
        int orun = hrtimer_forward_now(timer, ms_to_ktime(get_regulation_time()));
        BUG_ON(orun == 0);

        /* unthrottle tasks (if any) */
        cinfo->throttled_task = NULL;
        u64 init_budget = convert_mb_to_events(g_bw_intial_setpoint_mb[cinfo->cpu_id]);
        local64_set(&read_event->hw.period_left, init_budget);
        /*Re-enabled the counter*/
        read_event->pmu->start(read_event, PERF_EF_RELOAD);
        return HRTIMER_RESTART;
    }

    u.cur_used_bw_mb = convert_events_to_mb(read_count_used);
    update_stats(u.cur_used_bw_mb);


    /*
    u.cur_used_bw_mb represents the BW used in the last regulation interval (t-1)
    u.prev_used_bw_mb represents the BW used in the last regulation interval (t-2)
    */

    s64 delta = 0;  
    s64 new_alloc_budg_mb = u.cur_used_bw_mb;

    setpoint_bw_mb = get_setpoint();
    s64 error_mb = setpoint_bw_mb - u.used_avg_bw_mb;
    if ( 0 != error_mb ){

        delta = do_pid_control(error_mb);
        new_alloc_budg_mb = u.cur_used_bw_mb + delta;
        if (new_alloc_budg_mb < 0 ){
            new_alloc_budg_mb = 0;
        }
    }

    u64 read_event_new_budget = convert_mb_to_events(new_alloc_budg_mb);

    
    //trace_printk("AREG:err_mb=%lld n_budg=%lld delta=%lld cor_mb=%lld\n", error_mb,read_event_new_budget,delta,new_alloc_budg_mb);
    trace_printk("used_avg_bw=%lld cur_used_bw_mb=%lld u.prev_used_bw_mb=%lld  sp_mb=%lld err_mb=%lld delta=%lld cor_mb=%lld n_budg=%lld o_cnt=%llu n_cnt=%llu used_cnt=%lld \n",
    u.used_avg_bw_mb,
    u.cur_used_bw_mb,
    u.prev_used_bw_mb,
    setpoint_bw_mb,
    error_mb, delta, 
    new_alloc_budg_mb,
    read_event_new_budget,
    cinfo->g_read_count_old, 
    cinfo->g_read_count_new, 
    read_count_used);


    local64_set(&read_event->hw.period_left, read_event_new_budget);  //??? SOME major issue here

    u.prev_used_bw_mb = u.cur_used_bw_mb;
    cinfo->g_read_count_old = cinfo->g_read_count_new;


    /* forward timer */
    int orun = hrtimer_forward_now(timer, ms_to_ktime(get_regulation_time()));
    BUG_ON(orun == 0);

    /* unthrottle tasks (if any) */
    cinfo->throttled_task = NULL;

    /*Re-enabled the counter*/
    read_event->pmu->start(read_event, PERF_EF_RELOAD);
    return HRTIMER_RESTART;
}


static struct hrtimer ar_regu_timer ;

static int  setup_cpu_info(struct core_info* cinfo, const u8 cpu_id){
    pr_info("%s: Enter", __func__ );
    memset(cinfo, sizeof(struct core_info), 0);
    cinfo->cpu_id = cpu_id;

    /* Initialize with initial setpoint bandwidth values */
    cinfo->prev_used_bw_mb = g_bw_intial_setpoint_mb[cinfo->cpu_id];

    cinfo->used_bw_idx = 0;
    cinfo->used_bw_mb_list[cinfo->used_bw_idx] = cinfo->prev_used_bw_mb;
    cinfo->used_bw_idx++;

    cinfo->read_event =  init_counter(cinfo->cpu_id,
                                     convert_mb_to_events(cinfo->prev_used_bw_mb),
                                     g_read_counter_id,
                                     event_read_overflow_callback);
    if (cinfo->read_event == NULL){
        pr_err("Read_event %p did not allocate ", cinfo->read_event);
        return -1;
    }

    //set_read_event(cinfo->read_event);
    //enable_event(cinfo->read_event);


    thread_kt1 = kthread_run_on_cpu(thread_kt1_func,
                                    (void *)((unsigned long)cpu_id),
                                    cpu_to_node(cpu_id),
                                    "kthrottler/%u");

    BUG_ON(IS_ERR(thread_kt1));
    pr_info("%s: Exit", __func__ );
    return 0;
}

static void deinitialize_cpu_info(struct core_info *cinfo , const u8 cpu_id){
    pr_info("%s:Enter",__func__ );
    if (cinfo->read_event){
        disable_event(cinfo->read_event);
        cinfo->read_event= NULL;
    }
    kthread_stop(thread_kt1);
    thread_kt1 = NULL;
    pr_info("%s:Exit",__func__ );

}
/**************************************************************************************************************************
 * Module main
 **************************************************************************************************************************/
const u8 regulated_cpu_ids[] = {1};
static int __init ar_init (void ){

    pr_info("Supported CPUs: %d, online_cpus: %d\n", NR_CPUS, num_online_cpus());


//    #if defined (AREG_USE_FIFO)
//    // Initialize the error fifo
//    int ret = kfifo_alloc(&err_hist_fif    if (ret) {
//o,ERR_HIST_FIFO_SIZE, GFP_KERNEL);
//        printk(KERN_ERR "ar: Failed to allocate for kfifo: err_hist_fifo %d\n",ret);
//        return ret;
//    }
//    ret = kfifo_alloc(&err_hist_fifo_D,ERR_HIST_FIFO_SIZE, GFP_KERNEL);
//    if (ret) {
//        printk(KERN_ERR "ar: Failed to allocate for kfifo: err_hist_fifo_D %d\n",ret);
//        return ret;
//    }
//
//    pr_info("ar: KFIFO err_hist_fifo created");
//    #endif


    /* Creat entries in debugfs*/
    ar_init_debugfs();


    //    init_perf_workq();
    int ret = setup_cpu_info( &cinfo , (u8)1);
    if (ret != 0){
        pr_err("setup_cpu() Failed");
        ar_remove_debugfs();
    }

    //Setup CPU info for CPU 2, 3, 4
    // Initialize the master thread
    initialize_master();

//    hrtimer_init( &ar_regu_timer, CLOCK_MONOTONIC , HRTIMER_MODE_REL_PINNED);
//    ar_regu_timer.function=&ar_regu_timer_callback;
//    last_time = ktime_get();
//    hrtimer_start( &ar_regu_timer, ms_to_ktime(get_regulation_time()), HRTIMER_MODE_REL_PINNED  );

    /* throttled task pointer */
//    cinfo.throttled_task = NULL;
//    init_waitqueue_head(&cinfo.throttle_evt);

    pr_info("Module Initialized\n");
    return 0;

}


static void __exit ar_exit( void )
{


//    kthread_stop(thread_kt1);
//    hrtimer_cancel(&ar_regu_timer);

#if defined (AREG_USE_FIFO)

//    kfifo_free(&err_hist_fifo);
//    pr_info("ar: KFIFO err_hist_fifo freed");
#endif

    /* Deallocate all core info */
//    struct core_info* cinfo = get_core_info();
//    kfree(cinfo);
//    cinfo=NULL;
//    pr_info("ar: Coreinfo freed\n");

    ar_remove_debugfs();

    deinitialize_master();

    deinitialize_cpu_info(&cinfo , 1);

	pr_info("Module removed\n");
	return;
}

module_init(ar_init);
module_exit(ar_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sudarshan S <sudarshan.srinivasan@research.iiit.ac.in>");
