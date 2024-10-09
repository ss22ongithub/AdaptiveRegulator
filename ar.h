#if !defined AR_H
#define AR_H 

/* percpu info */
struct core_info {
   
    u32 g_read_count_new;
    u32 g_read_count_old;
    int cpu_id;

    wait_queue_head_t throttle_evt;

};

struct core_info* get_core_info(void);

#endif