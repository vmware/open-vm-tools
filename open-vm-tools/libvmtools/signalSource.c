/*********************************************************
 * Copyright (C) 2008-2016,2019 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/**
 * @file signalSource.c
 *
 *    A GSource implementation that is activated by OS signals.
 *
 *    Caveat: if the process is receiving a lot of signals in a short period
 *    of time, it's possible that the sources will not be notified for all
 *    the instances of a particular signal. So this mechanism shouldn't be
 *    used for reliable event delivery.
 */

#include "vm_assert.h"
#include "vmware/tools/utils.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

typedef enum {
   SIG_SRC_UNHANDLED = 0,
   SIG_SRC_IDLE,
   SIG_SRC_SIGNALED
} SignalState;

/* Use NSIG if it's defined, otherwise use a hardcoded limit. */
#if defined(NSIG)
#  define MAX_SIGNALS   NSIG
#else
#  define MAX_SIGNALS   64
#endif

typedef struct SignalHandler {
   gboolean                initialized;
   int                     wakeupPipe[2];
   struct sigaction        handler;
   GPollFD                 wakeupFd;
   SignalState             signals[MAX_SIGNALS];
   siginfo_t               currSignal;
} SignalHandler;

static SignalHandler gHandler;
G_LOCK_DEFINE_STATIC(gLock);

typedef struct SignalSource {
   GSource     src;
   int         signum;
} SignalSource;


/**
 * Reads one siginfo_t struct from the pipe if data is available, and
 * place it in the global state variable. This allows us to, eventually,
 * service all the processed signals, although in a not very efficient
 * way...
 */

static inline void
SignalSourceReadSigInfo(void)
{
   if (gHandler.wakeupFd.revents & G_IO_IN) {
      siginfo_t info;
      ssize_t nbytes = read(gHandler.wakeupFd.fd, &info, sizeof info);

      if (nbytes == -1) {
         g_warning("Signal source: reading from wake up fd failed.\n");
         return;
      } else if (nbytes < sizeof info) {
         g_warning("Signal source: reading from wake up fd returned %"FMTSZ"d,"
                   " expected %"FMTSZ"u.\n", nbytes, sizeof info);
         return;
      } else if (info.si_signo < 0 || info.si_signo >= MAX_SIGNALS) {
         g_warning("Signal source: bad signal number %d.\n", info.si_signo);
         return;
      }
      memcpy(&gHandler.currSignal, &info, sizeof info);
      gHandler.signals[info.si_signo] = SIG_SRC_SIGNALED;
      gHandler.wakeupFd.revents = 0;
   }
}


/**
 * Handles a signal. Writes the signal information to the wakeup pipe.
 *
 * According to signal(7), "write()" is safe to call from a signal handling
 * context. If the write fails, though, signal delivery might be delayed.
 *
 * @param[in]  signum      Signal received.
 * @param[in]  info        Information about the signal.
 * @param[in]  context     Unused.
 */

static void
SignalSourceSigHandler(int signum,
                       siginfo_t *info,
                       void *context)
{
   ssize_t bytes;
   siginfo_t dummy;
   if (signum >= MAX_SIGNALS || signum < 0) {
      return;
   }

   if (info == NULL) {
      /*
       * Solaris seems to call the handler with a NULL info struct in certain
       * situations. I noticed it when hitting CTRL-C (sending SIGINT) in a
       * terminal; calling "kill [pid]" (sending SIGTERM) seemed to work fine.
       * Still, it's better to handle this case.
       */
      memset(&dummy, 0, sizeof dummy);
      dummy.si_signo = signum;
      info = &dummy;
   }
   bytes = write(gHandler.wakeupPipe[1], info, sizeof *info);

   if (bytes == -1) {
      if (errno == EAGAIN) {
         /*
          * Pipe is full. If this ever becomes a problem, more pipes will
          * have to be created...
          */
         g_warning("Too many signals queued, this shouldn't happen.\n");
         ASSERT(FALSE);
      } else {
         g_warning("Could not queue signal %d (error %d: %s)\n",
                   signum, errno, strerror(errno));
      }
   }
}


/**
 * Does nothing.
 *
 * @param[in]  _src        Unused.
 * @param[out] timeout     Set to -1.
 *
 * @return FALSE
 */

static gboolean
SignalSourcePrepare(GSource *_src,
                    gint *timeout)
{
   *timeout = -1;
   return FALSE;
}


/**
 * Checks whether the process received the signal the source is watching.
 *
 * @param[in]  _src     The event source.
 *
 * @return Whether the source's signal was received.
 */

static gboolean
SignalSourceCheck(GSource *_src)
{
   SignalSource *src = (SignalSource *) _src;
   SignalSourceReadSigInfo();
   return (gHandler.signals[src->signum] == SIG_SRC_SIGNALED);
}


/**
 * Calls the callback associated with the handle, if any. Resets the source's
 * signal state to "not signaled".
 *
 * @param[in]  _src        The event source.
 * @param[in]  _callback   The callback to be called.
 * @param[in]  data        User-supplied data.
 *
 * @return The return value of the callback, or FALSE if the callback is NULL.
 */

static gboolean
SignalSourceDispatch(GSource *_src,
                     GSourceFunc _callback,
                     gpointer data)
{
   SignalSourceCb callback = (SignalSourceCb) _callback;
   SignalSource *src = (SignalSource *) _src;
   gHandler.signals[src->signum] = SIG_SRC_IDLE;
   return (callback != NULL) ? callback(&gHandler.currSignal, data)
                             : FALSE;
}


/**
 * Destroys the event source. Nothing needs to be done.
 *
 * @param[in]  src      The event source.
 */

static void
SignalSourceFinalize(GSource *src)
{
}

/**
 * @addtogroup vmtools_utils
 * @{
 */

/**
 * Creates a new source for the given signal.
 *
 * Rather than processing the events in the signal handling context, the main
 * loop is woken up and callbacks are processed in the main loop's thread.
 *
 * The same "wakeup" file descriptors are used for all sources, so if sources
 * are added to different main loop instances, all of them will be woken up
 * if any signal for which handlers are registered occurs.
 *
 * This code assumes that the rest of the app is not setting signal
 * handlers directly, at least for signals for which glib sources have
 * been set up.
 *
 * Also note that on older Linux systems (pre-NPTL), some real-time signals
 * are used by the pthread library and shouldn't be used by applications.
 *
 * Example of setting a handler for a signal:
 *
 * @code
 *
 *    GSource *src = VMTools_NewSignalSource(signum);
 *    g_source_set_callback(src, MyCallback, myData, NULL);
 *    g_source_attach(src, myContext);
 *
 * @endcode
 *
 * @note This API is not available on Win32.
 *
 * @param[in]  signum   Signal to watch.
 *
 * @return Pointer to the new source, NULL if failed to set signal handler.
 */

GSource *
VMTools_NewSignalSource(int signum)
{
   static GSourceFuncs srcFuncs = {
      SignalSourcePrepare,
      SignalSourceCheck,
      SignalSourceDispatch,
      SignalSourceFinalize,
      NULL,
      NULL
   };
   SignalSource *ret;

   ASSERT(0 <= signum && signum < MAX_SIGNALS);
   ASSERT(signum != SIGKILL && signum != SIGSTOP);

   G_LOCK(gLock);
   if (!gHandler.initialized) {
      if (pipe(gHandler.wakeupPipe) == -1 ||
          fcntl(gHandler.wakeupPipe[0], F_SETFL, O_RDONLY | O_NONBLOCK) < 0 ||
          fcntl(gHandler.wakeupPipe[1], F_SETFL, O_WRONLY | O_NONBLOCK) < 0) {
         ASSERT(FALSE);
      }
      gHandler.wakeupFd.fd = gHandler.wakeupPipe[0];
      gHandler.wakeupFd.events = G_IO_IN | G_IO_ERR;
      gHandler.handler.sa_sigaction = SignalSourceSigHandler;
      gHandler.handler.sa_flags = SA_SIGINFO;
      gHandler.initialized = TRUE;
   }
   G_UNLOCK(gLock);

   /*
    * Sets the signal handler if it hasn't been set yet. The code is
    * racy, but it's OK if 2 threads are trying to install the same
    * signal handler (one will win and all will be fine).
    */
   if (gHandler.signals[signum] == SIG_SRC_UNHANDLED) {
      if (sigaction(signum, &gHandler.handler, NULL) == -1) {
         g_warning("Cannot set signal handler: %s\n", strerror(errno));
         return NULL;
      }
      gHandler.signals[signum] = SIG_SRC_IDLE;
   }

   ret = (SignalSource *) g_source_new(&srcFuncs, sizeof *ret);
   ret->signum = signum;

   g_source_add_poll(&ret->src, &gHandler.wakeupFd);
   return &ret->src;
}

/** @}  */

