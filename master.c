#include "kernel_headers.h"
#include "master.h"
#include "ar.h"
#include "ar_perfs.h"
#include "utils.h"
#include "model.h"

static struct task_struct* mthread = NULL;

/* Unused functions*/
static void throttle( u8 cpu_id) __attribute__((unused));
static void unthrottle( u8 cpu_id) __attribute__((unused));

/* External Functions */
extern u64 estimate(u64* feat, u8 feat_len, double *wm, u8 wm_len, u8 index);
extern void update_weight_matrix(s64 error,struct core_info* cinfo );
/* External Variables / constants */
extern u64 g_bw_intial_setpoint_mb[MAX_NO_CPUS+1];/*Pre-defined initial / min Bandwidth in MB/s */
extern u64 g_bw_max_mb[MAX_NO_CPUS+1]; /*Pre-defined max Bandwidth per core in MB/s */

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
                                                     cinfo->ri) + g_bw_intial_setpoint_mb[cpu_id];
                    
                    if(cinfo->next_estimate < 0){
						trace_printk("CPU(%u): Negative Estimate=%lld \n",cpu_id,cinfo->next_estimate);
                        //reset the weights
                        initialize_weight_matrix(cinfo);
                        continue;
                    }
					
                    //TODO: When estimate crosses a thrhold 
                    // if (cinfo->next_estimate > g_bw_max_mb[cpu_id]){
					// 	trace_printk("CPU(%u): Estimated(%u) = %lld > Max Limit \n",cpu_id, cinfo->next_estimate);
					// 	cinfo->next_estimate = g_bw_max_mb[cpu_id];
					// }

                    atomic64_set(&cinfo->budget_est, convert_mb_to_events(cinfo->next_estimate));

                    s64 error = cinfo->g_read_count_used - cinfo->prev_estimate;
                    update_weight_matrix(error,cinfo);

                    char buf[HIST_SIZE][51]={0};    
                        for (u8 i = 0; i < HIST_SIZE; i++){
                         kernel_fpu_begin();
                         print_double(buf[i],cinfo->weight_matrix[i]);
                         kernel_fpu_end();
                    }


                    (cinfo->ri)++;
                    cinfo->ri = (cinfo->ri == HIST_SIZE)? 0:cinfo->ri;
                    trace_printk("CPU(%u):Used=%llu nxt_est=%lld err=%lld w0=%s w1=%s w2=%s w3=%s w4=%s\n",
                                 cpu_id,
                                 cinfo->g_read_count_used,
                                 cinfo->next_estimate,
                                 error,
                                 buf[0],buf[1],buf[2],buf[3], buf[4]);
                    cinfo->prev_estimate=cinfo->next_estimate;
                    break;
                default:
                    continue;
            }
        }
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
