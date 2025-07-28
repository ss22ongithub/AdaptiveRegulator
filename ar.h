#if !defined AR_H
#define AR_H 


#define SLIDING_WINDOW_SIZE 25  // TODO: CHANGE AR_SW_SIZE IN AR_DEBUGFS.C AS WELL

/* Each CPU core's info */
struct core_info {
    u64 g_read_count_new;
    u64 g_read_count_old;
    u8  cpu_id;
    wait_queue_head_t throttle_evt;
    /* UPDATE: Currently this variable unused and atomic throttle variable is used instead.
     * True = core in throttled state  False = core not throttled */
    atomic_t throttler_task;

//    Pointer to the task that is currently getting throttled. It is used as a condition to check whether
//    the core should be throttled (restrict sending the LLC load store request).
//    This pointer is either assigned to NULL (in which case it throttle_thread waiting ) or
//    assigned to "current" (in which case it throttling is in force ).
//    struct task_struct * throttler_task;

    /*
     * Pointer to the throttler kthread context.
     */
    struct task_struct * throttler_thread;

//  Bandwidth utilization parameters
    u64  prev_used_bw_mb; /* BW utilized in the previous regulation interval , units: Mbps*/
    u64  cur_used_bw_mb;
    u64  used_bw_mb_list[SLIDING_WINDOW_SIZE];
    u64  used_avg_bw_mb;
    u32  used_bw_idx;
// perf events
    struct perf_event*  read_event;
};
struct utilization {
	s64  prev_used_bw_mb; /* BW utilized in the previous regulation interval , units: Mbps*/
	s64  cur_used_bw_mb;
	u64  used_bw_mb_list[SLIDING_WINDOW_SIZE];
	u64  used_avg_bw_mb;
	u32  used_bw_idx;
};

struct core_info* get_core_info(u8 cpu_id);

struct bw_distribution {
	u32 time;
	u32 rd_avg_bw;
};


#endif