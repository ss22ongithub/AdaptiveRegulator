/**
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
#include <linux/init.h>

/* Timer / time*/
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/delay.h>
/* CPU / SMP */
#include <linux/smp.h> /* IPI calls */
#include <linux/cpumask.h> //for CPU hotplugging
/* Per CPU */
#include <linux/percpu.h>
#include <linux/smp.h>
#include <linux/cpumask.h>

/* IRQ processing*/
#include <linux/irq_work.h>
#include <linux/hardirq.h>
#include <linux/interrupt.h>
/*perf infrastructure framework*/
#include <linux/perf_event.h>
/* Memory */
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/notifier.h>
/*Threads and Process */
#include <linux/kthread.h>
/* Tracing / printing */
#include <linux/printk.h>
#include <linux/trace_events.h>
/* Other Misc.*/
#include <linux/topology.h>
#include <linux/kfifo.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <asm/atomic.h>
#include <linux/hw_breakpoint.h>
#include <linux/kstrtox.h>

/* Scheduler*/
#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 0, 0)
#  include <uapi/linux/sched/types.h>
#elif LINUX_VERSION_CODE > KERNEL_VERSION(4, 13, 0)
#  include <linux/sched/types.h>
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0)
#  include <linux/sched/rt.h>
#endif
#include <linux/sched.h>

/* Adaptive regulator*/
#include "ar_debugfs.h"
#include "ar_perfs.h"
#include "ar.h"

//extern void set_cpu_online(unsigned int cpu, bool online);


/**************************************************************************
 * Function Declarations 
 **************************************************************************/
void switch_off_cpu(int cpu);
void switch_on_cpu(int cpu);
// static wait_queue_head_t evt;
// static u8 next_task = 0;

/**************************************************************************
 * Callbacks and Handlers
 **************************************************************************/
//static struct task_struct* thread_kt1 = NULL;
//static int master_thread_func(void * data) {
//    int cpu_id = 1;
//    pr_info("%s: Enter",__func__);
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
//
////    sched_set_fifo(current);
//
//    while (!kthread_should_stop() ) {
//
//        AR_DEBUG("Wait for Event\n");
////        wait_event_interruptible( evt,
////                     cinfo->throttled_task ||
////                     kthread_should_stop());
////
//        if (kthread_should_stop()){
//        	pr_info("test: Stopping thread %s\n",__func__);
//            break;
//        }
//
////        AR_DEBUG("Throttling...\n");
////
////        if (cpu_online(cpu_id)) {
////			set_cpu_online(cpu_id,false);
////    	    pr_info("%s: cpu down = %d \n",__func__,cpu_id, ret);
////        }else {
////          	int ret = cpu_up(cpu_id);
////            pr_info("%s: cpu_up(%d) = %d \n",__func__,cpu_id, ret);
////        }
//        ssleep(5);
//    }
//
//    pr_info("ar: %s: Exit",__func__);
//    return 0;
//}

//
//void switch_off_cpu(int cpu){
//
// if (cpu_online(cpu)) {
//	set_cpu_online(cpu,false);
//	pr_info("%s: cpu down = %d \n",__func__,cpu);
// }
//}
//
//void switch_on_cpu(int cpu){
//  	set_cpu_online(cpu,true);
//	pr_info("%s: cpu up = %d \n",__func__,cpu);
//
/*
* Define a custom structure with 2 u32 variables.
*/
struct my_cpu_stats {
    u32 counter_a;
    u32 counter_b;
};

/*
 * Define a per-CPU instance of this structure.
 * This creates a separate 'struct my_cpu_stats' for every CPU core.
 */
DEFINE_PER_CPU(struct my_cpu_stats, my_percpu_data);

/*
 * A function that runs on a specific CPU to increment its local counters
 */
static void increment_local_counter(void *info)
{
    /* * get_cpu_var() disables preemption and returns a reference
     * to the current CPU's instance of the structure.
     */
    struct my_cpu_stats *local_stats = &get_cpu_var(my_percpu_data);

    local_stats->counter_a++;
    local_stats->counter_b += 10; // Increment b by 10 to show difference

    pr_info("CPU[%d]: incremented A to %u, B to %u\n",
            smp_processor_id(), local_stats->counter_a, local_stats->counter_b);

    /* * put_cpu_var() re-enables preemption. Must be called after get_cpu_var.
     */
    put_cpu_var(my_percpu_data);
}

/**************************************************************************************************************************
 * Module main
 **************************************************************************************************************************/

static int __init test_init (void ){
    int cpu;

    pr_info("--- Per-CPU Structure Demo Start ---\n");

    /* * Step 1: Initialize the structure members on all CPUs to 0
     * We use per_cpu_ptr() to avoid "aggregate value" errors.
     */
    for_each_possible_cpu(cpu) {
        struct my_cpu_stats* s =  per_cpu_ptr(&my_percpu_data,cpu);
        s->counter_a = 0;
        s->counter_b = 0;
//        per_cpu_ptr(&my_percpu_data, cpu)->counter_a = 0;
//        per_cpu_ptr(&my_percpu_data, cpu)->counter_b = 0;
    }

    /*
     * Step 2: Increment the counters on EACH CPU.
     * We use on_each_cpu() to run our function on all online cores.
     */
    on_each_cpu(increment_local_counter, NULL, 1);

    /*Alternatively:
     Call increment() on all "other" processors ...*/
    smp_call_function(increment_local_counter, NULL, 1);
    /*... and then explicitly on the current cpu*/
    increment_local_counter(NULL);

    /*
     * Step 3: Increment specific members just on the current CPU.
     * We use this_cpu_ptr() to safely access members of the struct.
     */
    this_cpu_ptr(&my_percpu_data)->counter_a++;
    this_cpu_ptr(&my_percpu_data)->counter_b += 100;

    /* Alternatively:
     * on the current cpu */
    struct my_cpu_stats*  this_cpu_stats = this_cpu_ptr(&my_percpu_data);
    this_cpu_stats->counter_a++;
    this_cpu_stats->counter_b += 100;

    pr_info("CPU[%d]: manually modified local A to %u, B to %u\n",
            smp_processor_id(),
            this_cpu_ptr(&my_percpu_data)->counter_a,
            this_cpu_ptr(&my_percpu_data)->counter_b);

    /*
     * Step 4: Print the final values to prove they are different.
     */
    pr_info("Final Values:\n");
    for_each_online_cpu(cpu) {
        struct my_cpu_stats *stats = per_cpu_ptr(&my_percpu_data, cpu);
        pr_info("  CPU %d -> A: %u, B: %u\n", cpu, stats->counter_a, stats->counter_b);
    }

    return 0;

}


static void __exit test_exit( void )
{ 


	pr_info("test: Module removed\n");
	return;
}

module_init(test_init);
module_exit(test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sudarshan S <sudarshan.srinivasan@research.iiit.ac.in>");
