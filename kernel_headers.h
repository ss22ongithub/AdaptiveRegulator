//
// Created by ss22 on 3/7/25.
//

#ifndef ADAPTIVEREGULATOR_KERNEL_HEADERS_H
#define ADAPTIVEREGULATOR_KERNEL_HEADERS_H

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/smp.h> /* IPI calls */
#include <linux/irq_work.h>
#include <linux/hardirq.h>
#include <linux/perf_event.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <asm/atomic.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/notifier.h>
#include <linux/kthread.h>
#include <linux/printk.h>
#include <linux/interrupt.h>
#include <linux/trace_events.h>
#include <linux/cpumask.h>
#include <linux/topology.h>
#include <linux/kfifo.h>
#include <asm/fpu/api.h>
#include <linux/init.h>
#include <linux/hw_breakpoint.h>
#include <linux/kstrtox.h>
#include <linux/math64.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 0, 0)
#  include <uapi/linux/sched/types.h>
#elif LINUX_VERSION_CODE > KERNEL_VERSION(4, 13, 0)
#  include <linux/sched/types.h>
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0)
#  include <linux/sched/rt.h>
#endif
#include <linux/sched.h>

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#endif //ADAPTIVEREGULATOR_KERNEL_HEADERS_H
