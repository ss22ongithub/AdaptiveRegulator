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

#include <linux/topology.h>
#include <linux/kfifo.h>


#include <linux/cpumask.h> //for CPU hotplugging


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



static struct task_struct* thread_kt1 = NULL;

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
//}
/**************************************************************************************************************************
 * Module main
 **************************************************************************************************************************/

static int __init test_init (void ){
    
    pr_info("ar: Supported CPUs: %d, online_cpus: %d\n", NR_CPUS, num_online_cpus());

    /* CPU0 is not hot pluggable  we use CPU 0 to run
	    the masterthread. Also confirm  CONFIG_HOTPLUG_CPU=y
		is enabled in the kernel  configuration
	*/


//    int cpu_id  = 0; //cpuid= 1,2,3 4 are reserved for BW regulation
//    thread_kt1 = kthread_create_on_node(master_thread_func,
//                                       (void*)NULL,
//                                       cpu_to_node(cpu_id),
//                                       "kcontroller/%d",cpu_id);
//	BUG_ON(IS_ERR(thread_kt1));
//	kthread_bind(thread_kt1, cpu_id);
//    wake_up_process(thread_kt1);

    extern int estimate(void);
    estimate();
    pr_info("Exit");


//	switch_off_cpu(2);
    return 0;

}


static void __exit test_exit( void )
{ 
    //Cleanup
//	switch_on_cpu(2);

//    kthread_stop(thread_kt1);
    

	pr_info("test: Module removed\n");
	return;
}

module_init(test_init);
module_exit(test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sudarshan S <sudarshan.srinivasan@research.iiit.ac.in>");
