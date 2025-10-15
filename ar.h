#if !defined AR_H
#define AR_H

#define HIST_SIZE 5
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

struct core_info *get_core_info(u8 cpu_id);
bool start_regulation(u8 cpu_id);
void stop_regulation(u8 cpu_id);
void stop_perf_counters(u8 cpu_id);
bool start_perf_counters(u8 cpu_id);

struct bw_distribution {
  u32 time;
  u32 rd_avg_bw;
};

#endif
