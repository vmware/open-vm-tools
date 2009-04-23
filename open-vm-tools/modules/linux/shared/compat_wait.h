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

#ifndef __COMPAT_WAIT_H__
#   define __COMPAT_WAIT_H__


#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/file.h>

#include "compat_file.h"


/*
 * The DECLARE_WAITQUEUE() API appeared in 2.3.1
 * It was back ported in 2.2.18
 *
 *  --hpreg
 */

#ifndef DECLARE_WAITQUEUE

typedef struct wait_queue *wait_queue_head_t;
#   define init_waitqueue_head(_headPtr) *(_headPtr) = NULL
#   define DECLARE_WAITQUEUE(_var, _task) \
   struct wait_queue _var = {_task, NULL, }

typedef struct wait_queue wait_queue_t;
#   define init_waitqueue_entry(_wait, _task) ((_wait)->task = (_task))

#endif

/*
 * The 'struct poll_wqueues' appeared in 2.5.48, when global
 * /dev/epoll interface was added.  It was backported to the
 * 2.4.20-wolk4.0s.
 */

#ifdef VMW_HAVE_EPOLL // {
#define compat_poll_wqueues struct poll_wqueues
#else // } {
#define compat_poll_wqueues poll_table
#endif // }

#ifdef VMW_HAVE_EPOLL // {

/* If prototype does not match, build will abort here */
extern void poll_initwait(compat_poll_wqueues *);

#define compat_poll_initwait(wait, table) ( \
   poll_initwait((table)), \
   (wait) = &(table)->pt \
)

#define compat_poll_freewait(wait, table) ( \
   poll_freewait((table)) \
)

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0) // {

/* If prototype does not match, build will abort here */
extern void poll_initwait(compat_poll_wqueues *);

#define compat_poll_initwait(wait, table) ( \
   (wait) = (table), \
   poll_initwait(wait) \
)

#define compat_poll_freewait(wait, table) ( \
   poll_freewait((table)) \
)

#else // } {

#define compat_poll_initwait(wait, table) ( \
   (wait) = (table), /* confuse compiler */ \
   (wait) = (poll_table *) __get_free_page(GFP_KERNEL), \
   (wait)->nr = 0, \
   (wait)->entry = (struct poll_table_entry *)((wait) + 1), \
   (wait)->next = NULL \
)

static inline void
poll_freewait(poll_table *wait)
{
   while (wait) {
      struct poll_table_entry * entry;
      poll_table *old;

      entry = wait->entry + wait->nr;
      while (wait->nr > 0) {
	 wait->nr--;
	 entry--;
	 remove_wait_queue(entry->wait_address, &entry->wait);
	 compat_fput(entry->filp);
      }
      old = wait;
      wait = wait->next;
      free_page((unsigned long) old);
   }
}

#define compat_poll_freewait(wait, table) ( \
   poll_freewait((wait)) \
)

#endif // }

/*
 * The wait_event_interruptible_timeout() interface is not
 * defined in pre-2.6 kernels.
 */
#ifndef wait_event_interruptible_timeout
#define __wait_event_interruptible_timeout(wq, condition, ret)		\
do {									\
   wait_queue_t __wait;						        \
   init_waitqueue_entry(&__wait, current);				\
									\
   add_wait_queue(&wq, &__wait);					\
   for (;;) {							        \
      set_current_state(TASK_INTERRUPTIBLE);			        \
      if (condition)						        \
	 break;						                \
      if (!signal_pending(current)) {				        \
	 ret = schedule_timeout(ret);			                \
	 if (!ret)					                \
	    break;					                \
	 continue;					                \
      }							                \
      ret = -ERESTARTSYS;					        \
      break;							        \
   }								        \
   set_current_state(TASK_RUNNING);				        \
   remove_wait_queue(&wq, &__wait);				        \
} while (0)

#define wait_event_interruptible_timeout(wq, condition, timeout)	\
({									\
   long __ret = timeout;						\
   if (!(condition))						        \
      __wait_event_interruptible_timeout(wq, condition, __ret);         \
   __ret;								\
})
#endif

/*
 * The wait_event_timeout() interface is not
 * defined in pre-2.6 kernels.
 */
#ifndef wait_event_timeout
#define __wait_event_timeout(wq, condition, ret)        		\
do {									\
   wait_queue_t __wait;						        \
   init_waitqueue_entry(&__wait, current);				\
									\
   add_wait_queue(&wq, &__wait);					\
   for (;;) {							        \
      set_current_state(TASK_UNINTERRUPTIBLE);        	                \
      if (condition)						        \
         break;						                \
      ret = schedule_timeout(ret);			                \
      if (!ret)					                        \
         break;					                        \
   }								        \
   set_current_state(TASK_RUNNING);				        \
   remove_wait_queue(&wq, &__wait);				        \
} while (0)

#define wait_event_timeout(wq, condition, timeout)	                \
({									\
   long __ret = timeout;						\
   if (!(condition))						        \
      __wait_event_timeout(wq, condition, __ret);                       \
   __ret;								\
})
#endif

/*
 * DEFINE_WAIT() and friends were added in 2.5.39 and backported to 2.4.28.
 *
 * Unfortunately it is not true. While some distros may have done it the
 * change has never made it into vanilla 2.4 kernel. Instead of testing
 * particular kernel versions let's just test for presence of DEFINE_WAIT
 * when figuring out whether we need to provide replacement implementation
 * or simply alias existing one.
 */

#ifndef DEFINE_WAIT

# define COMPAT_DEFINE_WAIT(_wait)                              \
   DECLARE_WAITQUEUE(_wait, current)
# define compat_init_prepare_to_wait(_sleep, _wait, _state)     \
   do {                                                         \
      __set_current_state(_state);                              \
      add_wait_queue(_sleep, _wait);                            \
   } while (0)
# define compat_cont_prepare_to_wait(_sleep, _wait, _state)     \
   set_current_state(_state)
# define compat_finish_wait(_sleep, _wait, _state)              \
   do {                                                         \
      __set_current_state(_state);                              \
      remove_wait_queue(_sleep, _wait);                         \
   } while (0)

#else

# define COMPAT_DEFINE_WAIT(_wait)                              \
   DEFINE_WAIT(_wait)
# define compat_init_prepare_to_wait(_sleep, _wait, _state)     \
   prepare_to_wait(_sleep, _wait, _state)
# define compat_cont_prepare_to_wait(_sleep, _wait, _state)     \
   prepare_to_wait(_sleep, _wait, _state)
# define compat_finish_wait(_sleep, _wait, _state)              \
   finish_wait(_sleep, _wait)

#endif /* #ifndef DEFINE_WAIT */

#endif /* __COMPAT_WAIT_H__ */
