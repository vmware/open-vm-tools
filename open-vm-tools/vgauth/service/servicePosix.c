/*********************************************************
 * Copyright (C) 2011-2017, 2019-2021 VMware, Inc. All rights reserved.
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

#if defined(sun)
#include <sys/systeminfo.h>
#endif

#if defined(__FreeBSD__) || defined(__APPLE__)
# include <sys/sysctl.h>
#endif

/*
 * XXX we can probably remove a lot of these; carried over from hostinfoPosix.c
 * til we're building on Mac and can check.
 */
#if defined(__APPLE__)
#include <assert.h>
#include <CoreServices/CoreServices.h>
#include <mach-o/dyld.h>
#include <mach/host_info.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sys/mman.h>
#elif defined(__FreeBSD__)
#if !defined(RLIMIT_AS)
#  if defined(RLIMIT_VMEM)
#     define RLIMIT_AS RLIMIT_VMEM
#  else
#     define RLIMIT_AS RLIMIT_RSS
#  endif
#endif
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <paths.h>
#endif

#include "serviceInt.h"
#include "service.h"
#include "VGAuthBasicDefs.h"


#if !defined(_PATH_DEVNULL)
#define _PATH_DEVNULL "/dev/null"
#endif


/*
 ******************************************************************************
 * ServiceResetProcessState --                                           */ /**
 *
 * Based on bora/lib/misc/hostinfoPosix.c:Hostinfo_ResetProcessState()
 * Clean up signal handlers and file descriptors before an exec().
 *
 * @param[in]     keepFds    Array of fds to leave open.
 * @param[in]     numKeepFds Number of fds to leave open.
 *
 ******************************************************************************
 */

static void
ServiceResetProcessState(int *keepFds,
                         size_t numKeepFds)
{
   int s, fd;
   struct sigaction sa;
   struct rlimit rlim;

   /*
    * Disable itimers before resetting the signal handlers.
    * Otherwise, the process may still receive timer signals:
    * SIGALRM, SIGVTARLM, or SIGPROF.
    */

   struct itimerval it;
   it.it_value.tv_sec = it.it_value.tv_usec = 0;
   it.it_interval.tv_sec = it.it_interval.tv_usec = 0;
   setitimer(ITIMER_REAL, &it, NULL);
   setitimer(ITIMER_VIRTUAL, &it, NULL);
   setitimer(ITIMER_PROF, &it, NULL);

   for (s = 1; s <= NSIG; s++) {
      sa.sa_handler = SIG_DFL;
      sigfillset(&sa.sa_mask);
      sa.sa_flags = SA_RESTART;
      sigaction(s, &sa, NULL);
   }

   for (fd = (int) sysconf(_SC_OPEN_MAX) - 1; fd > STDERR_FILENO; fd--) {
      size_t i;

      for (i = 0; i < numKeepFds; i++) {
         if (fd == keepFds[i]) {
            break;
         }
      }
      if (i == numKeepFds) {
         (void) close(fd);
      }
   }
   if (getrlimit(RLIMIT_AS, &rlim) == 0) {
      rlim.rlim_cur = rlim.rlim_max;
      setrlimit(RLIMIT_AS, &rlim);
   }
}


/*
 ******************************************************************************
 * ServiceSuicide --                                                     */ /**
 *
 * @brief Service self cancel
 *
 * Reads the pid from pidPath and forces the process ID'ed there to quit.
 * Useful for shutdown scripts.
 *
 * @param[in]   pidPath     NUL-terminated UTF-8 path to read PID.
 *
 * @return FALSE if the process could not be canceled.
 ******************************************************************************
 */

gboolean
ServiceSuicide(const char *pidPath)
{
   char pidBuf[32];
   int pid;
   int ret;
   int errCode;
   gboolean bRet = FALSE;
   FILE *pidPathFp = g_fopen(pidPath, "r");

   if (NULL == pidPathFp) {
      Warning("%s: failed to open pid file '%s', error %u\n",
              __FUNCTION__, pidPath, errno);
      return FALSE;
   }

   if (fgets(pidBuf, sizeof pidBuf, pidPathFp)) {
      pid = atoi(pidBuf);
      if (pid <= 0) {
         Warning("%s bad pid %d read; skipping\n", __FUNCTION__,
                 pid);
         goto done;
      }
      Debug("%s: sending SIGTERM to service at pid %d\n", __FUNCTION__, pid);
      ret = kill(pid, SIGTERM);
      errCode = errno;
      if (0 != ret) {
         if (ESRCH == errCode) {
            Debug("%s: pid %d not found, returning success\n",
                  __FUNCTION__, pid);
            bRet = TRUE;
         } else {
            Warning("%s: kill(%d) failed, error %u\n",
                    __FUNCTION__, pid, errCode);
         }
      } else {
         bRet = TRUE;
      }
   }

done:
   if (pidPathFp) {
      fclose(pidPathFp);
   }
   return bRet;
}



/*
 ******************************************************************************
 * ServiceDaemonize --                                                   */ /**
 *
 * @brief Cross-platform daemon(3)-like wrapper.
 *
 * Stripped down from bora/lib/misc/hostinfoPosix.c:Hostinfo_Daemonize()
 *
 * The current process is restarted with the given arguments.
 * The process state is reset (see Hostinfo_ResetProcessState).
 * A new session is created (so the process has no controlling terminal).
 *
 * Restarts the current process as a daemon, given the path to the
 * process.  This means:
 *
 * * You're detached from your parent.  (Your parent doesn't
 *   need to wait for you to exit.)
 * * Your process no longer has a controlling terminal or
 *   process group.
 * * Your stdin/stdout/stderr fds are redirected to /dev/null. All
 *   other descriptors, except for the ones that are passed in the
 *   parameter keepFds, are closed.
 * * Your signal handlers are reset to SIG_DFL in the daemonized
 *   process, and all the signals are unblocked.
 * * Your main() function is called with the specified NULL-terminated
 *   argument list.
 *
 * (Don't forget that the first string in args is argv[0] -- the
 * name of the process).
 *
 *
 * All stdio file descriptors of the daemon process are redirected to /dev/null.
 *
 * If pidPath is non-NULL, then upon success, writes the PID
 * (as a US-ASCII string followed by a newline) of the daemon
 * process to that path.
 *
 * If 'flags' contains SERVICE_DAEMONIZE_LOCKPID and pidPath is
 * non-NULL, then an exclusive flock(2) is taken on pidPath to prevent
 * multiple instances of the service from running.
 *
 * @param[in]   path        NUL-terminated UTF-8 path to exec
 * @param[in]   args        NUL-terminated UTF-8 argv list
 * @param[in]   flags       Any flags.
 * @param[in]   pidPath     NUL-terminated UTF-8 path to write PID.
 *
 *
 * @return FALSE if the process could not be daemonized.  errno contains
 *      the error on failure.
 *      Doesn't return if the process was daemonized.
 *
 ******************************************************************************
 */

gboolean
ServiceDaemonize(const char *path,
                 char * const *args,
                 ServiceDaemonizeFlags flags,
                 const char *pidPath)
{
   /*
    * We use the double-fork method to make a background process whose
    * parent is init instead of the original process.
    *
    * We do this instead of calling daemon(), because daemon() is
    * deprecated on Mac OS 10.5 hosts, and calling it causes a compiler
    * warning.
    *
    * We must exec() after forking, because Mac OS library frameworks
    * depend on internal Mach ports, which are not correctly propagated
    * across fork calls.  exec'ing reinitializes the frameworks, which
    * causes them to reopen their Mach ports.
    */

   int pidPathFd = -1;
   int childPid;
   int pipeFds[2] = { -1, -1 };
   int err = EINVAL;
   sigset_t sig;
   int fd;
   int saveFds[2];
   int numSavedFds = 0;

   ASSERT_ON_COMPILE(sizeof (errno) <= sizeof err);
   ASSERT(args);
   ASSERT(path);

   if (pidPath) {
      if (!ServiceNetworkCreateSocketDir()) {
         return FALSE;
      }
      pidPathFd = g_open(pidPath, O_WRONLY | O_CREAT, 0644);
      if (pidPathFd == -1) {
         err = errno;
         Warning("%s: Couldn't open PID path [%s], error %u.\n",
                 __FUNCTION__, pidPath, err);
         errno = err;
         return FALSE;
      }

      /*
       * Lock this file to take a mutex on daemonizing this process. The child
       * will keep this file descriptor open for as long as it is running.
       *
       * flock(2) is a BSD extension (also supported on Linux) which creates a
       * lock that is inherited by the child after fork(2). fcntl(2) locks do
       * not have this property. Solaris only supports fcntl(2) locks.
       */
#ifndef sun
      if ((flags & SERVICE_DAEMONIZE_LOCKPID) &&
          flock(pidPathFd, LOCK_EX | LOCK_NB) == -1) {
         err = errno;
         Warning("%s: Lock held on PID path [%s], error %u, not daemonizing.\n",
                 __FUNCTION__, pidPath, err);
         errno = err;
         close(pidPathFd);
         return FALSE;
      }
#endif
      saveFds[numSavedFds++] = pidPathFd;
   }

   if (pipe(pipeFds) == -1) {
      err = errno;
      Warning("%s: Couldn't create pipe, error %u.\n", __FUNCTION__, err);
      pipeFds[0] = pipeFds[1] = -1;
      goto cleanup;
   }

   saveFds[numSavedFds++] = pipeFds[1];

   if (fcntl(pipeFds[1], F_SETFD, FD_CLOEXEC) == -1) {
      err = errno;
      Warning("%s: Couldn't set close-on-exec for fd %d, error %u.\n",
              __FUNCTION__, pipeFds[1], err);
      goto cleanup;
   }

   childPid = fork();

   switch (childPid) {
   case -1:
      err = errno;
      Warning("%s: Couldn't fork first child, error %u.\n", __FUNCTION__,
              err);
      goto cleanup;
   case 0:
      /* We're the first child.  Continue on. */
      break;
   default:
      {
         /* We're the original process.  Check if the first child exited. */
         int status;

         close(pipeFds[1]);
         waitpid(childPid, &status, 0);
         if (WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS) {
            Warning("%s: Child %d exited with error %d.\n",
                    __FUNCTION__, childPid, WEXITSTATUS(status));
            goto cleanup;
         } else if (WIFSIGNALED(status)) {
            Warning("%s: Child %d exited with signal %d.\n",
                    __FUNCTION__, childPid, WTERMSIG(status));
            goto cleanup;
         }

         /*
          * Check if the second child exec'ed successfully.  If it had
          * an error, it will write an int errno to this pipe before
          * exiting.  Otherwise, its end of the pipe will be closed on
          * exec and this call will fail as expected.
          * The assumption is that we don't get a partial read. In case,
          * it did happen, we can detect it by the number of bytes read.
          */

         while (TRUE) {
            int res = read(pipeFds[0], &err, sizeof err);

            if (res > 0) {
               Warning("%s: Child could not exec %s, read %d, error %u.\n",
                       __FUNCTION__, path, res, err);
               goto cleanup;
            } else if ((res == -1) && (errno == EINTR)) {
               continue;
            }
            break;
         }

         err = 0;
         goto cleanup;
      }
   }
   /*
    * Reset the signal mask to unblock all
    * signals. fork() clears pending signals.
    */

   ServiceResetProcessState(saveFds, numSavedFds);
   sigfillset(&sig);
   sigprocmask(SIG_UNBLOCK, &sig, NULL);

   if (setsid() == -1) {
      Warning("%s: Couldn't create new session, error %d.\n",
              __FUNCTION__, errno);

      _exit(EXIT_FAILURE);
   }

   switch (fork()) {
   case -1:
      {
         Warning("%s: Couldn't fork, error %d.\n",
                 __FUNCTION__, errno);

         return FALSE;
      }
   case 0:
      // We're the second child.  Continue on.
      break;
   default:
      /*
       * We're the first child.  We don't need to exist any more.
       *
       * Exiting here causes the child to be reparented to the
       * init process, so the original process doesn't need to wait
       * for the child we forked off.
       */

      _exit(EXIT_SUCCESS);
   }

   if (chdir("/") == -1) {
      int err = errno;

      Warning("%s: Couldn't chdir to /, error %u.\n", __FUNCTION__, err);

      _exit(EXIT_FAILURE);
   }

   (void) umask(0);

   fd = open(_PATH_DEVNULL, O_RDONLY);
   if (fd != -1) {
      dup2(fd, STDIN_FILENO);
      close(fd);
   }

   fd = open(_PATH_DEVNULL, O_WRONLY);
   if (fd != -1) {
      dup2(fd, STDOUT_FILENO);
      dup2(fd, STDERR_FILENO);
      close(fd);
   }

   if (pidPath) {
      int64_t pid;
      char pidString[32];
      int pidStringLen;

      ASSERT_ON_COMPILE(sizeof (pid_t) <= sizeof pid);
      ASSERT(pidPathFd >= 0);

      pid = getpid();
      pidStringLen = g_snprintf(pidString, sizeof pidString,
                                "%"FMT64"d\n", pid);
      if (pidStringLen <= 0) {
         err = EINVAL;

         if (write(pipeFds[1], &err, sizeof err) == -1) {
            Warning("%s: Couldn't write to parent pipe: %u, original "
                    "error: %u.\n", __FUNCTION__, errno, err);
         }
         _exit(EXIT_FAILURE);
      }

      if (ftruncate(pidPathFd, 0) == -1) {
         err = errno;
         Warning("%s: Couldn't truncate path [%s], error %d.\n",
                 __FUNCTION__, pidPath, err);

         if (write(pipeFds[1], &err, sizeof err) == -1) {
            Warning("%s: Couldn't write to parent pipe: %u, original "
                    "error: %u.\n", __FUNCTION__, errno, err);
         }
         _exit(EXIT_FAILURE);
      }

      if (write(pidPathFd, pidString, pidStringLen) != pidStringLen) {
         err = errno;
         Warning("%s: Couldn't write PID to path [%s], error %d.\n",
                 __FUNCTION__, pidPath, err);

         if (write(pipeFds[1], &err, sizeof err) == -1) {
            Warning("%s: Couldn't write to parent pipe: %u, original "
                    "error: %u.\n", __FUNCTION__, errno, err);
         }
         _exit(EXIT_FAILURE);
      }

      if (fsync(pidPathFd) == -1) {
         err = errno;
         Warning("%s: Couldn't flush PID to path [%s], error %d.\n",
                 __FUNCTION__, pidPath, err);

         if (write(pipeFds[1], &err, sizeof err) == -1) {
            Warning("%s: Couldn't write to parent pipe: %u, original "
                    "error: %u.\n", __FUNCTION__, errno, err);
         }
         _exit(EXIT_FAILURE);
      }
   }

   /*
    * XXX
    * The original code translated the path and argv into the default
    * locale -- we may need to do that again.
    */
   if (execv(path, args) == -1) {
      err = errno;
      Warning("%s: Couldn't exec %s, error %d.\n", __FUNCTION__, path, err);

      /* Let the original process know we failed to exec. */
      if (write(pipeFds[1], &err, sizeof err) == -1) {
         Warning("%s: Couldn't write to parent pipe: %u, "
                 "original error: %u.\n", __FUNCTION__, errno, err);
      }
      _exit(EXIT_FAILURE);
   }

   // NOT_REACHED

  cleanup:
   if (pipeFds[0] != -1) {
      close(pipeFds[0]);
   }
   if (pipeFds[1] != -1) {
      close(pipeFds[1]);
   }

   if (err != 0 && pidPath) {
      /*
       * Unlink pidPath on error before closing pidPathFd to avoid racing
       * with another process attempting to daemonize and unlinking the
       * file it created instead.
       */
      g_unlink(pidPath);
      errno = err;
   }

   if (pidPath) {
      close(pidPathFd);
   }
   return err == 0;
}

