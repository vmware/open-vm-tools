/*********************************************************
 * Copyright (C) 2011-2016 VMware, Inc. All rights reserved.
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
 *
 *    Stolen from apps/lib/vmtoolslib
 */


#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "service.h"

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

static SignalHandler sigHandler = { FALSE, };
G_LOCK_DEFINE_STATIC(gLock);

typedef struct SignalSource {
   GSource     src;
   int         signum;
} SignalSource;

/** Type of callback used by the signal event source. */
typedef gboolean (*SignalSourceCb)(const siginfo_t *, gpointer);


/*
 ******************************************************************************
 * SignalSourceReadSigInfo --                                            */ /**
 *
 * Reads one siginfo_t struct from the pipe if data is available, and
 * place it in the global state variable. This allows us to, eventually,
 * service all the processed signals, although in a not very efficient
 * way...
 ******************************************************************************
 */

static inline void
SignalSourceReadSigInfo(void)
{
   if (sigHandler.wakeupFd.revents & G_IO_IN) {
      siginfo_t info;
      ssize_t nbytes = read(sigHandler.wakeupFd.fd, &info, sizeof info);
      if (nbytes == -1) {
         g_warning("Signal source: reading from wake up fd failed.");
         return;
      } else {
         /* XXX: Maybe we should handle this in some other way? */
         ASSERT(nbytes == sizeof info);
         ASSERT(info.si_signo < MAX_SIGNALS);
      }
      memcpy(&sigHandler.currSignal, &info, sizeof info);
      // make coverity happy
      if (info.si_signo >= 0 && info.si_signo < MAX_SIGNALS) {
         sigHandler.signals[info.si_signo] = SIG_SRC_SIGNALED;
      }
      sigHandler.wakeupFd.revents = 0;
   }
}


/*
 ******************************************************************************
 * SignalSourceSigHandler --                                             */ /**
 *
 * Handles a signal. Writes the signal information to the wakeup pipe.
 *
 * According to signal(7), "write()" is safe to call from a signal handling
 * context. If the write fails, though, signal delivery might be delayed.
 *
 * @param[in]  signum      Signal received.
 * @param[in]  info        Information about the signal.
 * @param[in]  context     Unused.
 ******************************************************************************
 */

static void
SignalSourceSigHandler(int signum,
                       siginfo_t *info,
                       void *context)
{
   ssize_t bytes;
   siginfo_t dummy;
   if (signum >= MAX_SIGNALS) {
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
   bytes = write(sigHandler.wakeupPipe[1], info, sizeof *info);

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


/*
 ******************************************************************************
 * SignalSourcePrepare --                                                */ /**
 *
 * Does nothing.
 *
 * @param[in]  _src        Unused.
 * @param[out] timeout     Set to -1.
 *
 * @return FALSE
 ******************************************************************************
 */

static gboolean
SignalSourcePrepare(GSource *_src,
                    gint *timeout)
{
   *timeout = -1;
   return FALSE;
}


/*
 ******************************************************************************
 * SignalSourceCheck --                                                  */ /**
 *
 * Checks whether the process received the signal the source is watching.
 *
 * @param[in]  _src     The event source.
 *
 * @return Whether the source's signal was received.
 ******************************************************************************
 */

static gboolean
SignalSourceCheck(GSource *_src)
{
   SignalSource *src = (SignalSource *) _src;
   SignalSourceReadSigInfo();
   return (sigHandler.signals[src->signum] == SIG_SRC_SIGNALED);
}


/*
 ******************************************************************************
 * SignalSourceDispatch --                                               */ /**
 *
 * Calls the callback associated with the handle, if any. Resets the source's
 * signal state to "not signaled".
 *
 * @param[in]  _src        The event source.
 * @param[in]  _callback   The callback to be called.
 * @param[in]  data        User-supplied data.
 *
 * @return The return value of the callback, or FALSE if the callback is NULL.
 ******************************************************************************
 */

static gboolean
SignalSourceDispatch(GSource *_src,
                     GSourceFunc _callback,
                     gpointer data)
{
   SignalSourceCb callback = (SignalSourceCb) _callback;
   SignalSource *src = (SignalSource *) _src;
   sigHandler.signals[src->signum] = SIG_SRC_IDLE;
   return (callback != NULL) ? callback(&sigHandler.currSignal, data)
                             : FALSE;
}


/*
 ******************************************************************************
 * SignalSourceFinalize --                                               */ /**
 *
 * Destroys the event source. Nothing needs to be done.
 *
 * @param[in]  src      The event source.
 ******************************************************************************
 */

static void
SignalSourceFinalize(GSource *src)
{
}


/*
 ******************************************************************************
 * ServiceNewSignalSource --                                             */ /**
 *
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
 ******************************************************************************
 */

GSource *
ServiceNewSignalSource(int signum)
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

   ASSERT(signum < MAX_SIGNALS);
   ASSERT(signum != SIGKILL && signum != SIGSTOP);

   G_LOCK(gLock);
   if (!sigHandler.initialized) {
      memset(&sigHandler, 0, sizeof sigHandler);
      if (pipe(sigHandler.wakeupPipe) == -1 ||
          fcntl(sigHandler.wakeupPipe[0], F_SETFL, O_RDONLY | O_NONBLOCK) < 0 ||
          fcntl(sigHandler.wakeupPipe[1], F_SETFL, O_WRONLY | O_NONBLOCK) < 0) {
         ASSERT(FALSE);
      }
      sigHandler.wakeupFd.fd = sigHandler.wakeupPipe[0];
      sigHandler.wakeupFd.events = G_IO_IN | G_IO_ERR;
      sigHandler.handler.sa_sigaction = SignalSourceSigHandler;
      sigHandler.handler.sa_flags = SA_SIGINFO;
      sigHandler.initialized = TRUE;
   }
   G_UNLOCK(gLock);

   /*
    * Sets the signal handler if it hasn't been set yet. The code is
    * racy, but it's OK if 2 threads are trying to install the same
    * signal handler (one will win and all will be fine).
    */
   if (sigHandler.signals[signum] == SIG_SRC_UNHANDLED) {
      if (sigaction(signum, &sigHandler.handler, NULL) == -1) {
         g_warning("Cannot set signal handler: %s\n", strerror(errno));
         return NULL;
      }
      sigHandler.signals[signum] = SIG_SRC_IDLE;
   }

   ret = (SignalSource *) g_source_new(&srcFuncs, sizeof *ret);
   ret->signum = signum;

   g_source_add_poll(&ret->src, &sigHandler.wakeupFd);
   return &ret->src;
}
