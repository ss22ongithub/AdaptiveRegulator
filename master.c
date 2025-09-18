#include "kernel_headers.h"
#include "master.h"
#include "ar.h"
#include "ar_perfs.h"



static struct task_struct* mthread = NULL;

/* Unused functions*/
static void throttle( u8 cpu_id) __attribute__((unused));
static void unthrottle( u8 cpu_id) __attribute__((unused));

/* External Functions */
extern u64 estimate(u64* feat, u8 feat_len, float *wm, u8 wm_len, u8 index);
extern void update_weight_matrix(s64 error,struct core_info* cinfo );
extern u32  get_regulation_time(void);

/* Inline function */
/** convert MB/s to #of events (i.e., LLC miss counts) per 1ms */
static inline u64 convert_mb_to_events(int mb)
{
    return div64_u64((u64)mb*1024*1024,
             CACHE_LINE_SIZE * (1000/get_regulation_time()));
}

/* Convert # of events to MB/s */
static inline u64 convert_events_to_mb(u64 events)
{
    /*
     * BW  = (event * CACHE_LINE_SIZE)/ time_in_ms  - bytes/ms 
     *     =  (event * CACHE )/ (time_in_ms * 1024 *1024)  = mb/ms
     *     =  (event * CACHE * 1000 )/ (time_in_sec * 1024 *1024)  = mb/s
     */ 
    int divisor = get_regulation_time()*1024*1024;
    int mb = div64_u64(events*CACHE_LINE_SIZE*1000 + (divisor-1), divisor);
    return mb;
}



/* WARNING: This function should be kept strictly re-entrant */
static void throttle( u8 cpu_id)
{
    if (cpu_id == 0){
        pr_err("%s: cpu_id cannot be 0!",__func__);
        return;
    }
    struct core_info* cinfo = get_core_info(cpu_id);
    bool t = atomic_read(&cinfo->throttler_task);
    pr_debug("%s: CPU(%d), t = %d",__func__,cpu_id,t);
    if ( t ) {
        pr_err("cinfo->throttler_task=%x, already in throttled state", t);
        return;
    }
    atomic_set(&cinfo->throttler_task,true);
    wake_up_interruptible(&cinfo->throttle_evt);
}

/* WARNING: This function should be kept strictly re-entrant */
static void unthrottle( u8 cpu_id) {
    if (cpu_id == 0){
        pr_err("%s: cpu_id cannot be 0!",__func__);
        return;
    }
    struct core_info* cinfo = get_core_info(cpu_id);
    bool t = atomic_read(&cinfo->throttler_task);
    pr_debug("%s: CPU(%d) t = %d",__func__,cpu_id,t);
    atomic_set(&cinfo->throttler_task,false);
}


static int master_thread_func(void * data) {
    pr_info("%s: Enter",__func__);

    //    sched_set_fifo(current);

    while (!kthread_should_stop() ) {
        u8 cpu_id;
//        trace_printk("Master thread looping\n");
        if (kthread_should_stop()){
        	pr_info("Stopping thread %s\n",__func__);
            break;
        }
        for_each_online_cpu(cpu_id){
            switch(cpu_id){
                case 1:
                case 2:
                case 3:
                case 4:
                    struct core_info* cinfo = get_core_info(cpu_id);
                    WARN_ON(cinfo == NULL);
                    WARN_ON(cinfo->read_event == NULL);

                    struct perf_event* read_event = cinfo->read_event;

                    cinfo->g_read_count_old = cinfo->g_read_count_new;
                    cinfo->g_read_count_new = convert_events_to_mb( perf_event_count(read_event)) ;
                    cinfo->g_read_count_used = cinfo->g_read_count_new -
                                                    cinfo->g_read_count_old;

                    cinfo->read_event_hist[cinfo->ri] = cinfo->g_read_count_used;
                    cinfo->next_estimate = estimate( cinfo->read_event_hist,
                                                     sizeof(cinfo->read_event_hist)/sizeof(cinfo->read_event_hist[0]),
                                                     cinfo->weight_matrix,
                                                     sizeof(cinfo->weight_matrix)/sizeof(cinfo->weight_matrix[0]),
                                                     cinfo->ri);
                    atomic64_set(&cinfo->budget_est, convert_mb_to_events(cinfo->next_estimate));
//                    if(!cinfo->prev_estimate){
//                        cinfo->prev_estimate=cinfo->next_estimate;
//                        continue;
//                    }
                    s64 error = cinfo->g_read_count_used - cinfo->prev_estimate;
                    update_weight_matrix(error,cinfo);


                    (cinfo->ri)++;
                    cinfo->ri = (cinfo->ri == HIST_SIZE)? 0:cinfo->ri;
                    trace_printk("CPU(%u):Used=%llu nxt_est=%lld err=%lld\n",
                                 cpu_id,
                                 cinfo->g_read_count_used,
                                 cinfo->next_estimate,
                                 error);
                    cinfo->prev_estimate=cinfo->next_estimate;
                    break;
                default:
                    continue;
            }
        }
       // usleep_range(100,100);
       msleep(1);
          
    }

    pr_info("%s: Exit",__func__);
    return 0;
}


void initialize_master(void){

    const u8 cpu_id_zero  = 0; //cpuid= 1,2,3 4 are reserved for BW regulation
    mthread = kthread_create_on_node(master_thread_func,
                                       (void*)NULL,
                                       cpu_to_node(cpu_id_zero),
                                       "areg_master_thread/%d",cpu_id_zero);
	BUG_ON(IS_ERR(mthread));
	kthread_bind(mthread, cpu_id_zero);
    wake_up_process(mthread);

}

void deinitialize_master(void){
    if (mthread){
        kthread_stop(mthread);
        mthread = NULL;
    }
    pr_info("%s: Exit!",__func__ );
}
