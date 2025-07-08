#include "kernel_headers.h"
#include "master.h"

static struct task_struct* mthread = NULL;

static int master_thread_func(void * data) {
    pr_info("%s: Enter",__func__);
//	struct perf_event_attr attr;
//
//	memset(&attr, 0, sizeof(struct perf_event_attr));
//	attr.type = PERF_TYPE_HARDWARE;
//	attr.config = PERF_COUNT_HW_CACHE_MISSES;
//	attr.size = sizeof(struct perf_event_attr);
//	attr.disabled = 0;
//	attr.exclude_kernel = 0;
//	attr.exclude_hv = 1;
//
//	// Create perf event on CPU 0
//	llc_miss_event = perf_event_create_kernel_counter(
//		&attr, 0, NULL, NULL, NULL);
//
//	if (IS_ERR(llc_miss_event)) {
//		pr_err("Failed to create perf event\n");
//		return PTR_ERR(llc_miss_event);
//	}

//    sched_set_fifo(current);

    while (!kthread_should_stop() ) {

        trace_printk("Wait for Event\n");
//        wait_event_interruptible( evt,
//                     cinfo->throttled_task ||
//                     kthread_should_stop());
//
        if (kthread_should_stop()){
        	pr_info("test: Stopping thread %s\n",__func__);
            break;
        }

//        trace_printk("Throttling...\n");
//
//        if (cpu_online(cpu_id)) {
//			set_cpu_online(cpu_id,false);
//    	    pr_info("%s: cpu down = %d \n",__func__,cpu_id, ret);
//        }else {
//          	int ret = cpu_up(cpu_id);
//            pr_info("%s: cpu_up(%d) = %d \n",__func__,cpu_id, ret);
//        }
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