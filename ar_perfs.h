#ifndef AR_PERFS_H
#define AR_PERFS_H


struct perf_event *init_counter(int cpu, int sample_period, int counter_id, void *callback);

void event_read_overflow_callback(struct perf_event *event,
                    struct perf_sample_data *data,
                    struct pt_regs *regs);

inline u64 perf_event_count(struct perf_event *event);

inline void enable_event(struct perf_event *event);

inline void disable_event(struct perf_event *event);

void init_perf_workq(void);

/* Getter setters for read event */

struct perf_event* get_read_event(void);

void set_read_event(struct perf_event* event);


//Debug functions 
u64 get_llc_ofc(void);


#endif /*AR_PERFS_H*/
