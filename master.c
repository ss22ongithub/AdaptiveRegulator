#include "kernel_headers.h"
#include "master.h"
#include "ar.h"
#include "ar_perfs.h"
#include "utils.h"
#include "model.h"

static struct task_struct* mthread = NULL;

/* Master thread state management */
static atomic_t master_state = ATOMIC_INIT(MASTER_STATE_INITIAL);
static wait_queue_head_t master_wait_queue;

/* External Functions */
extern u64 estimate(u64* feat, u8 feat_len, double *wm, u8 wm_len, u8 index);
extern void update_weight_matrix(s64 error,struct core_info* cinfo );

extern void __throttle( void* cpu );
extern void __unthrottle( void* cpu );

/* External Variables / constants */
extern u64 g_bw_intial_setpoint_mb[MAX_NO_CPUS+1];/*Pre-defined initial / min Bandwidth in MB/s */
extern u64 g_bw_max_mb[MAX_NO_CPUS+1]; /*Pre-defined max Bandwidth per core in MB/s */

static int master_thread_func(void * data) {
    pr_info("%s: Enter",__func__);

    /* Step 1: Immediately throttle all 4 cores */
    pr_info("%s: Throttling all cores at startup", __func__);
    for (u8 cpu_id = 1; cpu_id <= 4; cpu_id++) {
        struct core_info* cinfo = get_core_info(cpu_id);
        if (cinfo) {
            __throttle((void*)cinfo);
            pr_info("%s: CPU(%d) throttled", __func__, cpu_id);
        }
    }

    /* Step 2: Wait in INITIAL state until regulation is enabled */
    pr_info("%s: Entering INITIAL state, waiting for regulation to start", __func__);
    wait_event_interruptible(master_wait_queue,
                             atomic_read(&master_state) == MASTER_STATE_RUNNING
                             || kthread_should_stop());

    if (kthread_should_stop()) {
	pr_info("%s: Exit",__func__);
        return 0;
    }

    /* Step 3: Unthrottle all cores when regulation begins */
    pr_info("%s: Regulation enabled, unthrottling all cores", __func__);
    for (u8 cpu_id = 1; cpu_id <= 4; cpu_id++) {
        struct core_info* cinfo = get_core_info(cpu_id);
        if (cinfo) {
            __unthrottle((void*)cinfo);
            pr_info("%s: CPU(%d) unthrottled", __func__, cpu_id);
        }
    }


    /* Step 4: Begin normal regulation loop */
    pr_info("%s: Starting normal regulation loop", __func__);


    while (!kthread_should_stop() ) {
        u8 cpu_id;
        
        /* Check if we should pause regulation */
        if (atomic_read(&master_state) != MASTER_STATE_RUNNING) {
            pr_info("%s: Regulation paused, waiting...", __func__);
            wait_event_interruptible(master_wait_queue,
                                     atomic_read(&master_state) == MASTER_STATE_RUNNING
                                     || kthread_should_stop());
            if (kthread_should_stop()) {
                break;
            }
            pr_info("%s: Regulation resumed", __func__);
        }

        if (kthread_should_stop()){
            pr_info("Stopping thread %s\n",__func__);
            break;
        }

        for_each_online_cpu(cpu_id){
            s64 bw_total_req = 0;
            switch(cpu_id){
                case 1:
                case 2:
                case 3:
                case 4:
                    struct core_info* cinfo = get_core_info(cpu_id);
                    WARN_ON(cinfo == NULL);
                    WARN_ON(cinfo->read_event == NULL);

                    struct perf_event* read_event = cinfo->read_event;
                    struct perf_event* cycles_l3miss_event = cinfo->cycles_l3miss_event;

                    cinfo->g_read_count_old = cinfo->g_read_count_new;
                    cinfo->g_read_count_new = convert_events_to_mb( perf_event_count(read_event)) ;
                    cinfo->g_read_count_used = cinfo->g_read_count_new -
                                               cinfo->g_read_count_old;

                    u64 cycles_l3miss_count = perf_event_count(cycles_l3miss_event);



                    bw_total_req += cinfo->g_read_count_used;

                    cinfo->read_event_hist[cinfo->ri] = cinfo->g_read_count_used;
                    cinfo->next_estimate = estimate( cinfo->read_event_hist,
                                                     sizeof(cinfo->read_event_hist)/sizeof(cinfo->read_event_hist[0]),
                                                     cinfo->weight_matrix,
                                                     sizeof(cinfo->weight_matrix)/sizeof(cinfo->weight_matrix[0]),
                                                     cinfo->ri) + g_bw_intial_setpoint_mb[cpu_id];

                    if(cinfo->next_estimate < 0){
                        AR_DEBUG("CPU(%u): Negative Estimate=%lld \n",cpu_id,cinfo->next_estimate);
                        //scale down the weights
                        initialize_weight_matrix(cinfo, false);
                        continue;
                    }

                    //TODO: When estimate crosses a thrhold
                    // if (cinfo->next_estimate > g_bw_max_mb[cpu_id]){
                    // 	AR_DEBUG("CPU(%u): Estimated(%u) = %lld > Max Limit \n",cpu_id, cinfo->next_estimate);
                    // 	cinfo->next_estimate = g_bw_max_mb[cpu_id];
                    // }
                    s64 allocation = 0 ;
                    if (bw_total_req >= BW_TOTAL_AVAILABLE){
                        allocation = (cinfo->next_estimate/bw_total_req)* BW_TOTAL_AVAILABLE;
                    }else {
                        allocation = cinfo->next_estimate;
                    }
                    atomic64_set(&cinfo->budget_est, convert_mb_to_events(allocation));

                    s64 error = cinfo->g_read_count_used - cinfo->prev_estimate;
                    update_weight_matrix(error,cinfo);

#if defined(AR_DEBUG)
                    char buf[HIST_SIZE][51]={0};
                    for (u8 i = 0; i < HIST_SIZE; i++){
                        kernel_fpu_begin();
                        print_double(buf[i],cinfo->weight_matrix[i]);
                        kernel_fpu_end();
                    }
#endif
                    (cinfo->ri)++;
                    cinfo->ri = (cinfo->ri == HIST_SIZE)? 0:cinfo->ri;

                    AR_DEBUG("CPU(%u):Used=%llu nxt_est=%lld err=%lld w0=%s w1=%s w2=%s w3=%s w4=%s treq=%lld alloc=%lld cycles_l3miss_count=%llu \n",
                                 cpu_id,
                                 cinfo->g_read_count_used,
                                 cinfo->next_estimate,
                                 error,
                                 buf[0],buf[1],buf[2],buf[3], buf[4],
                                 bw_total_req,allocation,
                                 cycles_l3miss_count);
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
    
    /* Initialize wait queue for master thread state changes */
    init_waitqueue_head(&master_wait_queue);
    
    /* Set initial state */
    atomic_set(&master_state, MASTER_STATE_INITIAL);
    
    mthread = kthread_create_on_node(master_thread_func,
                                       (void*)NULL,
                                       cpu_to_node(cpu_id_zero),
                                       "areg_master_thread/%d",cpu_id_zero);
    BUG_ON(IS_ERR(mthread));
    kthread_bind(mthread, cpu_id_zero);
    wake_up_process(mthread);

    pr_info("%s: Master thread initialized in INITIAL state", __func__);
}

void deinitialize_master(void){
    if (mthread){
        /* Wake up master thread if it's waiting */
        atomic_set(&master_state, MASTER_STATE_STOPPED);
        wake_up_interruptible(&master_wait_queue);
        
        kthread_stop(mthread);
        mthread = NULL;
    }
    pr_info("%s: Exit!",__func__ );
}

void master_start_regulation(void){
    pr_info("%s: Starting regulation", __func__);
    atomic_set(&master_state, MASTER_STATE_RUNNING);
    wake_up_interruptible(&master_wait_queue);
}

void master_stop_regulation(void){
    pr_info("%s: Stopping regulation", __func__);
    atomic_set(&master_state, MASTER_STATE_INITIAL);
}

int master_get_state(void){
    return atomic_read(&master_state);
}
