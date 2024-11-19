#if !defined AR_H
#define AR_H 

/* percpu info */
struct core_info {
   
    u64 g_read_count_new;
    u64 g_read_count_old;
    int cpu_id;

    wait_queue_head_t throttle_evt;
   	struct task_struct * throttled_task;
};
#define SLIDING_WINDOW_SIZE 20  // TODO: CHANGE AR_SW_SIZE IN AR_DEBUGFS.C AS WELL
struct utilization {
	s64  prev_used_bw_mb; /* BW utilized in the previous regulation interval , units: Mbps*/
	s64  cur_used_bw_mb;
	u64  used_bw_mb_list[SLIDING_WINDOW_SIZE];
	u64  used_avg_bw_mb;
	u32  used_bw_idx;
};

struct core_info* get_core_info(void);

struct bw_distribution {
	u32 time;
	u32 rd_avg_bw;
};


#endif