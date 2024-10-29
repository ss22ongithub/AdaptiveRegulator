/**
 * Dynamic adaptive memory bandwidth controller for multi-core systems
 *
 *
 * This file is distributed under GPL v2 License. 
 * See LICENSE.TXT for details.
 *
 */
#if !defined AR_DEBUGFS_H
#define AR_DEBUGFS_H

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <asm/atomic.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/trace_events.h>

int ar_init_debugfs(void);
void ar_remove_debugfs(void);
u32 get_regulation_time(void);
u32 get_sliding_window_size(void);

#endif /* AR_DEBUGFS_H */
