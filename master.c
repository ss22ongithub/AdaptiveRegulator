#include "kernel_headers.h"
#include "master.h"
#include "ar.h"
#include "ar_perfs.h"

static struct task_struct* mthread = NULL;

/* Unused functions*/
static void throttle( u8 cpu_id) __attribute__((unused));
static void unthrottle( u8 cpu_id) __attribute__((unused));

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

const u8 CPUID = 1;

static int master_thread_func(void * data) {
    pr_info("%s: Enter",__func__);

    //    sched_set_fifo(current);

    while (!kthread_should_stop() ) {

        trace_printk("Master thread looping\n");
        if (kthread_should_stop()){
        	pr_info("Stopping thread %s\n",__func__);
            break;
        }
        struct core_info* cinfo = get_core_info(CPUID);
        if (cinfo == NULL){
            pr_err("coreinfo not found exiting %s...",__func__);
            return -1;
        }
        struct perf_event* read_event = cinfo->read_event;

        cinfo->g_read_count_old = cinfo->g_read_count_new;
        cinfo->g_read_count_new = perf_event_count(read_event);
        trace_printk("Counter(%llx): New: %llx  Old: %llx PerfEventState: %d\n",
                     read_event->attr.config,
                     cinfo->g_read_count_new,
                     cinfo->g_read_count_old,
                     read_event->state);
        // cinfo->g_read_count_used = cinfo->g_read_count_new - cinfo->g_read_count_old; 


        ssleep(5);
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
}