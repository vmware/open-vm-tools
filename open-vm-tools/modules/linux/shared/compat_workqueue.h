/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_WORKQUEUE_H__
# define __COMPAT_WORKQUEUE_H__

#include <linux/kernel.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 5, 41)
# include <linux/workqueue.h>
#endif

/*
 *
 * Work queues and delayed work queues.
 *
 * Prior to 2.5.41, the notion of work queues did not exist.  Taskqueues are
 * used for work queues and timers are used for delayed work queues.
 *
 * After 2.6.20, normal work structs ("work_struct") and delayed work
 * ("delayed_work") structs were separated so that the work_struct could be
 * slimmed down.  The interface was also changed such that the address of the
 * work_struct itself is passed in as the argument to the work function.  This
 * requires that one embed the work struct in the larger struct containing the
 * information necessary to complete the work and use container_of() to obtain
 * the address of the containing structure.
 *
 * Users of these macros should embed a compat_work or compat_delayed_work in
 * a larger structure, then specify the larger structure as the _data argument
 * for the initialization functions, specify the work function to take
 * a compat_work_arg or compat_delayed_work_arg, then use the appropriate
 * _GET_DATA macro to obtain the reference to the structure passed in as _data.
 * An example is below.
 *
 *
 *   typedef struct WorkData {
 *      int data;
 *      compat_work work;
 *   } WorkData;
 *
 *
 *   void
 *   WorkFunc(compat_work_arg data)
 *   {
 *      WorkData *workData = COMPAT_WORK_GET_DATA(data, WorkData, work);
 *
 *      ...
 *   }
 *
 *
 *   {
 *      WorkData *workData = kmalloc(sizeof *workData, GFP_EXAMPLE);
 *      if (!workData) {
 *         return -ENOMEM;
 *      }
 *
 *      COMPAT_INIT_WORK(&workData->work, WorkFunc, workData);
 *      compat_schedule_work(&workData->work);
 *   }
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 41)  /* { */
typedef struct tq_struct compat_work;
typedef struct compat_delayed_work {
   struct tq_struct work;
   struct timer_list timer;
} compat_delayed_work;
typedef void * compat_work_arg;
typedef void * compat_delayed_work_arg;

/*
 * Delayed work queues need to run at some point in the future in process
 * context, but task queues don't support delaying the task one is scheduling.
 * Timers allow us to delay the execution of our work queue until the future,
 * but timer handlers run in bottom-half context.  As such, we use both a timer
 * and task queue and use the timer handler below to schedule the task in
 * process context immediately.  The timer lets us delay execution, and the
 * task queue lets us run in process context.
 *
 * Note that this is similar to how delayed_work is implemented with work
 * queues in later kernel versions.
 */
static inline void
__compat_delayed_work_timer(unsigned long arg)
{
   compat_delayed_work *dwork = (compat_delayed_work *)arg;
   if (dwork) {
      schedule_task(&dwork->work);
   }
}

# define COMPAT_INIT_WORK(_work, _func, _data)            \
   INIT_LIST_HEAD(&(_work)->list);                        \
   (_work)->sync = 0;                                     \
   (_work)->routine = _func;                              \
   (_work)->data = _data
# define COMPAT_INIT_DELAYED_WORK(_work, _func, _data)    \
   COMPAT_INIT_WORK(&(_work)->work, _func, _data);        \
   init_timer(&(_work)->timer);                           \
   (_work)->timer.expires = 0;                            \
   (_work)->timer.function = __compat_delayed_work_timer; \
   (_work)->timer.data = (unsigned long)_work
# define compat_schedule_work(_work)                      \
   schedule_task(_work)
# define compat_schedule_delayed_work(_work, _delay)      \
   (_work)->timer.expires = jiffies + _delay;             \
   add_timer(&(_work)->timer)
# define COMPAT_WORK_GET_DATA(_p, _type, _member)         \
   (_type *)(_p)
# define COMPAT_DELAYED_WORK_GET_DATA(_p, _type, _member) \
   (_type *)(_p)

#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)       \
      && !defined(__VMKLNX__) /* } { */
typedef struct work_struct compat_work;
typedef struct work_struct compat_delayed_work;
typedef void * compat_work_arg;
typedef void * compat_delayed_work_arg;
# define COMPAT_INIT_WORK(_work, _func, _data)            \
   INIT_WORK(_work, _func, _data)
# define COMPAT_INIT_DELAYED_WORK(_work, _func, _data)    \
   INIT_WORK(_work, _func, _data)
# define compat_schedule_work(_work)                      \
   schedule_work(_work)
# define compat_schedule_delayed_work(_work, _delay)      \
   schedule_delayed_work(_work, _delay)
# define COMPAT_WORK_GET_DATA(_p, _type, _member)         \
   (_type *)(_p)
# define COMPAT_DELAYED_WORK_GET_DATA(_p, _type, _member) \
   (_type *)(_p)

#else  /* } Linux >= 2.6.20 { */
typedef struct work_struct compat_work;
typedef struct delayed_work compat_delayed_work;
typedef struct work_struct * compat_work_arg;
typedef struct work_struct * compat_delayed_work_arg;
# define COMPAT_INIT_WORK(_work, _func, _data)            \
   INIT_WORK(_work, _func)
# define COMPAT_INIT_DELAYED_WORK(_work, _func, _data)    \
   INIT_DELAYED_WORK(_work, _func)
# define compat_schedule_work(_work)                      \
   schedule_work(_work)
# define compat_schedule_delayed_work(_work, _delay)      \
   schedule_delayed_work(_work, _delay)
# define COMPAT_WORK_GET_DATA(_p, _type, _member)         \
   container_of(_p, _type, _member)
# define COMPAT_DELAYED_WORK_GET_DATA(_p, _type, _member) \
   container_of(_p, _type, _member.work)
#endif /* } */

#endif /* __COMPAT_WORKQUEUE_H__ */

