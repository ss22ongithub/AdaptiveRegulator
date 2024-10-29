/**************************************************************************
 * Included Files
 **************************************************************************/

#include <linux/perf_event.h>
#include "ar.h"
#include "ar_perfs.h"

/**************************************************************************
 * Perf Structure definitions 
 **************************************************************************/


static struct perf_event *llc_miss_read_event=NULL;
static struct irq_work llc_miss_event_irq_work;


/**************************************************************************
 * Callbacks and Handlers
 **************************************************************************/
static u64 llc_overflow_count =0 ;
static void llc_miss_event_irq_work_handler(struct irq_work *entry){
    
    BUG_ON(in_nmi() || !in_irq());
    struct core_info * cinfo = get_core_info();
    // s64 read_budget_used = llc_overflow_count= perf_event_count(llc_miss_read_event) - (cinfo->g_read_count_old);

    // trace_printk("read_budget_used %lld\n",read_budget_used);

    /* Send notfication to throttle_thread  to start the throttling*/
    cinfo->throttled_task = current;
    wake_up_interruptible(&cinfo->throttle_evt);

}
u64 get_llc_ofc(void){
    return llc_overflow_count;
}

/**************************************************************************
 * Perf Utils
 **************************************************************************/

/** read current counter value. */
inline u64 perf_event_count(struct perf_event *event)
{
    return local64_read(&event->count) + 
        atomic64_read(&event->child_count);
}

struct perf_event *init_counter(int cpu, int sample_period, int counter_id, void *callback)
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


void event_read_overflow_callback(struct perf_event *event,
                    struct perf_sample_data *data,
                    struct pt_regs *regs)
{
    irq_work_queue(&llc_miss_event_irq_work);
}


void enable_event(struct perf_event *event){

	perf_event_enable(event);

}

void disable_event(struct perf_event *event){

    perf_event_disable(event);
    perf_event_release_kernel(event);
    pr_info("Perf event disabled\n");

}

void init_perf_workq(void){
	/* initialize irq_work_queue */
	pr_info("%s",__func__);
    init_irq_work(&llc_miss_event_irq_work, llc_miss_event_irq_work_handler);

}

struct perf_event* get_read_event(){
    return llc_miss_read_event;
}

void set_read_event(struct perf_event* e){
    llc_miss_read_event  = e;
}