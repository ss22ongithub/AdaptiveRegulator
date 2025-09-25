//
// Created by ss22 on 25/9/25.
//

/* Placeholder for any utility function used adaptive regulation */

#ifndef ADAPTIVEREGULATOR_UTILS_H
#define ADAPTIVEREGULATOR_UTILS_H

extern u32  get_regulation_time(void);
/** convert MB/s to #of events (i.e., LLC miss counts) per 1ms */
static inline u64 convert_mb_to_events(int mb)
{
    return div64_u64((u64)mb*1024*1024,
                     CACHE_LINE_SIZE * (1000/get_regulation_time()));
}

/* Convert # of events to MB/s */
static inline u64 convert_events_to_mb(u64 events)
{
/*
 * BW  = (event * CACHE_LINE_SIZE)/ time_in_ms  - bytes/ms
 *     =  (event * CACHE )/ (time_in_ms * 1024 *1024)  = mb/ms
 *     =  (event * CACHE * 1000 )/ (time_in_sec * 1024 *1024)  = mb/s
 */
    int divisor = get_regulation_time()*1024*1024;
    int mb = div64_u64(events*CACHE_LINE_SIZE*1000 + (divisor-1), divisor);
    return mb;
}

static inline void print_current_context(void)
{
    trace_printk("in_interrupt(%ld)(hard(%ld),softirq(%d)"
                 ",in_nmi(%d)),irqs_disabled(%d)\n",
                 in_interrupt(), in_irq(), (int)in_softirq(),
                 (int)in_nmi(), (int)irqs_disabled());
}

#endif //ADAPTIVEREGULATOR_UTILS_H
