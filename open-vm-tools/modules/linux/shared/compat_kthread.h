/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_KTHREAD_H__
#   define __COMPAT_KTHREAD_H__

/*
 * The kthread interface for managing kernel threads appeared in 2.6.4, but was
 * only exported for module use in 2.6.7.
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 7)
# include <linux/kthread.h>

# define COMPAT_KTHREAD_DECLARE_STOP_INFO()
# define compat_kthread_stop(_tsk) kthread_stop(_tsk)
# define compat_kthread_should_stop() kthread_should_stop()
# define compat_kthread_run(_fn, _data, _namefmt, ...)                         \
   kthread_run(_fn, _data, _namefmt, ## __VA_ARGS__)
# define compat_kthread_create(_fn, _data, _namefmt, ...)                      \
   kthread_create(_fn, _data, _namefmt, ## __VA_ARGS__)
#else

/*
 * When the kthread interface isn't available, we do our best to emulate it,
 * with a few notable exceptions:
 *
 * 1: We use semaphores instead of mutexes for locking, because mutexes aren't
 *    available in kernels where kthread isn't available.
 * 2: The real kthread interface uses the kthreadd kernel_thread to broker the
 *    creation of new kernel threads. This makes sense because kthreadd is part
 *    of the kernel, but doesn't make sense at all in the context of an
 *    individual module. So in our emulation, thread creation occurs in the
 *    context of a kthread_create call.
 * 3: Because kthreadd is responsible for creating kernel threads in the real
 *    kthread interface, there's no need to explicitly reparent any of them. We
 *    aren't using kthreadd, so we call daemonize to reparent, which also sets
 *    the name of the new kernel thread. That's why we don't set the name as
 *    the real kthread interface does (within kthread_create). Furthermore, to
 *    get the name to daemonize, we're forced to pass it through the
 *    kthread_start_info struct.
 * 4: Since our interface isn't in the kernel proper, we can't make use of
 *    get_task_struct/put_task_struct so as to acquire references to kernel
 *    threads that we're managing. To prevent races, we use an extra completion
 *    when stopping kernel threads. See the comments in compat_kthread_stop for
 *    more details.
 *
 * Like the real kthread interface, ours must be globally available so that we
 * can emulate functions like kthread_should_stop without using different
 * signatures.
 */

# include "compat_completion.h"
# include "compat_kernel.h"
# include "compat_sched.h"

struct compat_kthread_start_info {
   int (*fn)(void *);
   void *data;
   compat_completion created;
   char comm[TASK_COMM_LEN];
};

struct compat_kthread_stop_info {
   struct semaphore lock;
   struct task_struct *task;
   compat_completion woken;
   compat_completion stopped;
   int ret;
};

extern struct compat_kthread_stop_info compat_kthread_stop_info;

# define COMPAT_KTHREAD_DECLARE_STOP_INFO()                                    \
   struct compat_kthread_stop_info compat_kthread_stop_info = {                \
      .lock = __SEMAPHORE_INITIALIZER(compat_kthread_stop_info.lock, 1),       \
      .task = NULL,                                                            \
   }


static inline int
compat_kthread_should_stop(void)
{
   return (compat_kthread_stop_info.task == current);
}


static inline int
compat_kthread_stop(struct task_struct *_task)
{
   int ret;

   down(&compat_kthread_stop_info.lock);

   /*
    * We use a write memory barrier to ensure that all CPUs see _task after
    * the completions have been initialized.
    *
    * There's a race between kernel threads managed by kthread and the upcoming
    * call to wake_up_process. If the kernel thread wakes up after we set task
    * but before the call to wake_up_process, the thread's call to
    * compat_kthread_should_stop will return true and the thread will exit. At
    * that point, the call to wake_up_process will be on a dead task_struct.
    *
    * XXX: The real kthread interface protects against this race by grabbing
    * and releasing a reference to _task. We don't have that luxury, because
    * there is a range of kernels where put_task_struct isn't exported to
    * modules. In fact, no other modules call get_task_struct or
    * put_task_struct, so to do so from this context may be unwise. Instead,
    * we'll use an extra completion to ensure that the kernel thread only exits
    * after wake_up_process has been called.
    */
   compat_init_completion(&compat_kthread_stop_info.woken);
   compat_init_completion(&compat_kthread_stop_info.stopped);
   smp_wmb();

   compat_kthread_stop_info.task = _task;
   wake_up_process(_task);
   compat_complete(&compat_kthread_stop_info.woken);

   compat_wait_for_completion(&compat_kthread_stop_info.stopped);
   compat_kthread_stop_info.task = NULL;
   ret = compat_kthread_stop_info.ret;
   up(&compat_kthread_stop_info.lock);
   return ret;
}


# define compat_kthread_run(_fn, _data, _namefmt, ...)                         \
({                                                                             \
   struct task_struct *tsk;                                                    \
   tsk = compat_kthread_create(_fn, _data, _namefmt, ## __VA_ARGS__);          \
   if (!IS_ERR(tsk)) {                                                         \
      wake_up_process(tsk);                                                    \
   }                                                                           \
   tsk;                                                                        \
})


static inline int
compat_kthread(void *_data)
{
   int ret = -EINTR;
   struct compat_kthread_start_info *info;
   int (*fn)(void *data);
   void *data;

   info = (struct compat_kthread_start_info *)_data;
   fn = info->fn;
   data = info->data;

   compat_daemonize(info->comm);
   __set_current_state(TASK_UNINTERRUPTIBLE);
   compat_complete(&info->created);
   schedule();

   if (!compat_kthread_should_stop()) {
      ret = fn(data);
   }

   if (compat_kthread_should_stop()) {
      compat_wait_for_completion(&compat_kthread_stop_info.woken);
      compat_kthread_stop_info.ret = ret;
      compat_complete_and_exit(&compat_kthread_stop_info.stopped, 0);
      BUG();
   }
   return 0;
}


static inline struct task_struct *
compat_kthread_create(int (*_fn)(void *data),
                      void *_data,
                      const char _namefmt[],
                      ...)
{
   pid_t pid;
   struct task_struct *task = NULL;
   struct compat_kthread_start_info info;
   va_list args;

   info.fn = _fn;
   info.data = _data;
   compat_init_completion(&info.created);
   va_start(args, _namefmt);
   vsnprintf(info.comm, sizeof info.comm, _namefmt, args);
   va_end(args);
   pid = kernel_thread(compat_kthread, &info, CLONE_KERNEL);
   if (pid >= 0) {
      compat_wait_for_completion(&info.created);

      /*
       * find_task_by_pid must be called with tasklist_lock held or under
       * rcu_read_lock. As the latter doesn't exist in old kernels, we use the
       * former for convenience.
       */
      read_lock(&tasklist_lock);
      task = find_task_by_pid(pid);
      read_unlock(&tasklist_lock);

      /* XXX: Do we need to get a reference on task? */
   }
   return task;
}

#endif

#endif /* __COMPAT_KTHREAD_H__ */
