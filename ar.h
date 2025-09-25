#if !defined AR_H
#define AR_H

// TODO: CHANGE AR_SW_SIZE IN AR_DEBUGFS.C AS WELL
#define SLIDING_WINDOW_SIZE 25
#define HIST_SIZE 5
#define CACHE_LINE_SIZE 64
#define MAX_NO_CPUS 4

/* Each CPU core's info */
struct core_info {
  u64 g_read_count_new;
  u64 g_read_count_old;
  u64 g_read_count_used;

  // History of count of LLC read misses occurred between the regulation intervals.
  // (Event: Read misses)
  u64 read_event_hist[HIST_SIZE];
  u8 ri;

  u8 cpu_id;
  wait_queue_head_t throttle_evt;
  /* UPDATE: Currently this variable unused and atomic throttle variable is used
   * instead. True = core in throttled state  False = core not throttled */
  atomic_t throttler_task;

  /*
   * Pointer to the throttler kthread context.
   */
  struct task_struct *throttler_thread;

  //  Bandwidth utilization parameters
  u64 prev_used_bw_mb; /* BW utilized in the previous regulation interval ,
                          units: Mbps*/
  u64 cur_used_bw_mb;
  u64 used_bw_mb_list[SLIDING_WINDOW_SIZE];
  u64 used_avg_bw_mb;
  u32 used_bw_idx;

  // PMC events
  struct perf_event *read_event;

  // Timer related
  struct hrtimer reg_timer;
  struct irq_work read_irq_work;

  // Memory Bandwidth Budget estimate for the core.
  // Computed by master core
  atomic64_t budget_est;
  /* Each core has an array of weights to generate the prediction */
  double weight_matrix [HIST_SIZE];
  s64 next_estimate;
  s64 prev_estimate;

};

struct utilization {
  s64 prev_used_bw_mb; /* BW utilized in the previous regulation interval ,
                          units: Mbps*/
  s64 cur_used_bw_mb;
  u64 used_bw_mb_list[SLIDING_WINDOW_SIZE];
  u64 used_avg_bw_mb;
  u32 used_bw_idx;
};

struct core_info *get_core_info(u8 cpu_id);
void start_regulation(u8 cpu_id);
void stop_regulation(u8 cpu_id);


struct bw_distribution {
  u32 time;
  u32 rd_avg_bw;
};

#endif
