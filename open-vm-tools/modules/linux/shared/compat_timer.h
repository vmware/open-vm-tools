/*********************************************************
 * Copyright (C) 2002 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

#ifndef __COMPAT_TIMER_H__
#   define __COMPAT_TIMER_H__


/*
 * The del_timer_sync() API appeared in 2.3.43
 * It became reliable in 2.4.0-test3
 *
 *   --hpreg
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
#   define compat_del_timer_sync(timer) del_timer_sync(timer)
#else
#   if LINUX_VERSION_CODE < KERNEL_VERSION(2, 3, 43)
       /* 2.3.43 removed asm/softirq.h's reference to bh_base. */
#      include <linux/interrupt.h>
#   endif
#   include <asm/softirq.h>

static inline int
compat_del_timer_sync(struct timer_list *timer) // IN
{
   int wasPending;

   start_bh_atomic();
   wasPending = del_timer(timer);
   end_bh_atomic();

   return wasPending;
}
#endif


/*
 * The msleep_interruptible() API appeared in 2.6.9.
 * It is based on the msleep() API, which appeared in 2.4.29.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
#   include <linux/delay.h>
#   define compat_msleep_interruptible(msecs) msleep_interruptible(msecs)
#   define compat_msleep(msecs) msleep(msecs)
#else
#   include <linux/sched.h>
/* 
 * msecs_to_jiffies appeared in 2.6.7.  For earlier kernels,
 * fall back to slow-case code (we don't use this operation
 * enough to need the performance).
 */
#   if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 7)
#      define msecs_to_jiffies(msecs) (((msecs) * HZ + 999) / 1000)
#   endif
/*
 * set_current_state appeared in 2.2.18.
 */
#   if LINUX_VERSION_CODE < KERNEL_VERSION(2, 2, 18)
#      define set_current_state(a) do { current->state = (a); } while(0)
#   endif

static inline void
compat_msleep_interruptible(unsigned long msecs) // IN
{
   set_current_state(TASK_INTERRUPTIBLE);
   schedule_timeout(msecs_to_jiffies(msecs) + 1);
}

static inline void
compat_msleep(unsigned long msecs) // IN
{
   set_current_state(TASK_UNINTERRUPTIBLE);
   schedule_timeout(msecs_to_jiffies(msecs) + 1);
}
#endif


/*
 * There is init_timer_deferrable() since 2.6.22.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#   define compat_init_timer_deferrable(timer) init_timer_deferrable(timer)
#else
#   define compat_init_timer_deferrable(timer) init_timer(timer)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
static inline void compat_setup_timer(struct timer_list * timer,
                                      void (*function)(unsigned long),
                                      unsigned long data)
{
   timer->function = function;
   timer->data = data;
   init_timer(timer);
}
#else
#   define compat_setup_timer(timer, function, data) \
       setup_timer(timer, function, data)
#endif


#endif /* __COMPAT_TIMER_H__ */
