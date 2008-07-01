/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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


/*
 * systemLinux.c --
 *
 *    System-specific routines for all guest applications.
 *
 *    Linux implementation
 *
 */

#if !defined(__linux__) && !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
#   error This file should not be compiled
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/times.h>
#include <netdb.h>
#ifdef sun
# include <sys/sockio.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
/* <netinet/in.h> must precede <arpa/in.h> for FreeBSD to compile. */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/ioctl.h>

#ifdef __FreeBSD__
#include "ifaddrs.h"
#endif

#include "vm_assert.h"
#include "system.h"
#include "debug.h"
#include "posix.h"
#include "unicode.h"

#define MAX_IFACES      4
#define LOOPBACK        "lo"
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif


/*
 * System_Uptime --
 *
 *    Retrieve the time (in hundredth of s.) since the system has started.
 *
 *    Note: On 32-bit Linux, whether you read /proc/uptime (2 system calls: seek(2)
 *          and read(2)) or times(2) (1 system call), the uptime information
 *          comes from the 'jiffies' kernel variable, whose type is 'unsigned
 *          long'. This means that on a ix86 with HZ == 100, it will wrap after
 *          497 days. This function can detect the wrapping and still return
 *          a correct, monotonic, 64 bit wide value if it is called at least
 *          once every 497 days.
 *      
 * Result:
 *    The value on success
 *    -1 on failure (never happens in this implementation)
 *
 * Side effects:
 *    None
 *
 */

uint64
System_Uptime(void)
{
   /*
    * Dummy variable b/c times(NULL) segfaults on FreeBSD 3.2 --greg
    */
   struct tms tp; 

#if !defined (VM_X86_64)
   static uint64 base = 0;
   static unsigned long last = 0;
   uint32  current;


   ASSERT(sizeof(current) == sizeof(clock_t));

   current = times(&tp);     // 100ths of a second

   if (current < last) {
      /* The 'jiffies' kernel variable wrapped */
      base += (uint64)1 << (sizeof(current) * 8);
   }

   return base + (last = current);
#else  // VM_X86_64

   return times(&tp);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * System_GetCurrentTime --
 *
 *      Get the time in seconds & microseconds since XXX from
 *      the guest OS.
 *
 * Results:
 *      
 *      TRUE/FALSE: success/failure
 *
 * Side effects:
 *
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
System_GetCurrentTime(int64 *secs,  // OUT
                      int64 *usecs) // OUT
{
   struct timeval tv;

   ASSERT(secs);
   ASSERT(usecs);
   
   if (gettimeofday(&tv, NULL) < 0) {
      return FALSE;
   }

   *secs = tv.tv_sec;
   *usecs = tv.tv_usec;

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * System_AddToCurrentTime --
 *
 *      Adjust the current system time by adding the given number of
 *      seconds & milliseconds.
 *
 * Results:
 *      
 *      TRUE/FALSE: success/failure
 *
 * Side effects:
 *
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
System_AddToCurrentTime(int64 deltaSecs,  // IN
                        int64 deltaUsecs) // IN
{
   struct timeval tv;
   int64 secs;
   int64 usecs;
   int64 newTime;
   
   if (!System_GetCurrentTime(&secs, &usecs)) {
      return FALSE;
   }
   
   newTime = (secs + deltaSecs) * 1000000L + (usecs + deltaUsecs);
   ASSERT(newTime > 0);
   
   /*
    * timeval.tv_sec is a 32-bit signed integer. So, Linux will treat
    * newTime as a time before the epoch if newTime is a time 68 years 
    * after the epoch (beacuse of overflow). 
    *
    * If it is a 64-bit linux, everything should be fine. 
    */
   if (sizeof tv.tv_sec < 8 && newTime / 1000000L > MAX_INT32) {
      Log("System_AddToCurrentTime() overflow: deltaSecs=%"FMT64"d, secs=%"FMT64"d\n",
          deltaSecs, secs);

      return FALSE;
   }
 
   tv.tv_sec = newTime / 1000000L;
   tv.tv_usec = newTime % 1000000L;

   if (settimeofday(&tv, NULL) < 0) {
      return FALSE;
   }
   
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * System_GetTimeAsString --
 *
 *      Returns the current time as a formatted string, useful for prepending
 *      to debugging output.
 *
 *      For example: "Oct 05 18:03:24.948: "
 *
 * Results:
 *      On success, allocates and returns a string containing the formatted
 *      time.
 *      On failure, returns NULL.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Unicode
System_GetTimeAsString(void)
{
   struct timeval tv;
   time_t sec;
   int msec;
   size_t charsWritten;
   size_t bufSize = 8; // Multiplied by 2 for the initial allocation.
   char *buf = NULL;
   Unicode dateTime = NULL;
   Unicode output = NULL;

   if (gettimeofday(&tv, NULL)) {
      goto out;
   }
   sec = tv.tv_sec;
   msec = tv.tv_usec / 1000;

   /*
    * Loop repeatedly trying to format the time into a buffer, doubling the
    * buffer with each failure. This should be safe as the manpage for
    * strftime(3) seems to suggest that it only fails if the buffer isn't large
    * enough.
    *
    * The resultant string is encoded according to the current locale.
    */
   do {
      char *newBuf;
      bufSize *= 2;
      
      newBuf = realloc(buf, bufSize);
      if (newBuf == NULL) {
         goto out;
      }
      buf = newBuf;
      charsWritten = strftime(buf, bufSize, "%b %d %H:%M:%S", localtime(&sec));
   } while (charsWritten == 0);

   /*
    * Append the milliseconds field, but only after converting the date/time
    * string from encoding specified in the current locale to an opaque type.
    */
   dateTime = Unicode_Alloc(buf, STRING_ENCODING_DEFAULT);
   if (dateTime == NULL) {
      goto out;
   }
   output = Unicode_Format("%s.%03d: ", dateTime, msec);

  out:
   free(buf);
   Unicode_Free(dateTime);
   return output;
}


/*
 *----------------------------------------------------------------------
 *
 * System_IsACPI --
 *
 *    Is this an ACPI system?
 *
 * Results:
 *      
 *    TRUE if this is an ACPI system.
 *    FALSE if this is not an ACPI system.   
 *
 * Side effects:
 *
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
System_IsACPI(void)
{
   ASSERT(FALSE);

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *  
 * System_Shutdown -- 
 *
 *   Initiate system shutdown.
 * 
 * Return value: 
 *    None.
 * 
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
System_Shutdown(Bool reboot)  // IN: "reboot or shutdown" flag
{
   static char *cmd;

   if (reboot) {
      cmd = "shutdown -r now";
   } else {
#if __FreeBSD__
      cmd = "shutdown -p now";
#else
      cmd = "shutdown -h now";
#endif
   }
   system(cmd);
}



/*
 *----------------------------------------------------------------------
 *
 * System_GetEnv --
 *    Read environment variables.
 *
 * Results:
 *    A copy of the environment variable encoded in UTF-8.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

char *
System_GetEnv(Bool global,       // IN
              char *valueName)   // IN: UTF-8
{
   char *result;
   
#if defined(sun)
   result = NULL;
#else
   result = Posix_Getenv(valueName);
#endif

   if (NULL != result) {
      result = strdup(result);
   }

   return(result);
} // System_GetEnv


/*
 *----------------------------------------------------------------------
 *
 * System_SetEnv --
 *
 *    Write environment variables.
 *
 *    On Linux, this only affects the local process. The global flag
 *    is ignored.
 *
 * Results:
 *    0 if success, -1 otherwise.
 *
 * Side effects:
 *    Changes the environment variable.
 *
 *----------------------------------------------------------------------
 */

int
System_SetEnv(Bool global,      // IN
              char *valueName,  // IN: UTF-8
              char *value)      // IN: UTF-8
{
#if defined(sun)
   return(-1);
#else
   return Posix_Setenv(valueName, value, 1);
#endif
} // System_SetEnv


/*
 *-----------------------------------------------------------------------------
 *
 * System_WritePidfile --
 *
 *      Write a PID into a pidfile.
 *
 *      Originally from the POSIX guestd as GuestdWritePidfile.
 *
 * Return value:
 *      TRUE on success
 *      FALSE on failure (detail is displayed on stderr)
 *
 * Side effects:
 *      This function is not thread-safe.  May display error messages on
 *      stderr.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
System_WritePidfile(char const *fileName, // IN: Path where we'll write pid
                    pid_t pid)            // IN: C'mon, really?
{
   FILE *pidFile;
   Bool success = FALSE;

   pidFile = fopen(fileName, "w+");
   if (pidFile == NULL) {
      fprintf(stderr, "Unable to open the \"%s\" PID file: %s.\n\n", fileName,
              strerror(errno));

      return FALSE;
   }

   if (fprintf(pidFile, "%"FMTPID"\n", pid) < 0) {
      fprintf(stderr, "Unable to write the \"%s\" PID file: %s.\n\n", fileName,
              strerror(errno));
   } else {
      success = TRUE;
   }

   if (fclose(pidFile)) {
      fprintf(stderr, "Unable to close the \"%s\" PID file: %s.\n\n", fileName,
              strerror(errno));

      return FALSE;
   }

   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * System_Daemon --
 *
 *      Analog to daemon(3), but optionally guarantees child's PID is written
 *      to a pidfile before the parent exits.
 *
 *      Originally from the short-lived Mac OS guestd, as Mac OS X does not
 *      provide daemon(3).  Additionally, the "optionally guaranteed pidfile"
 *      concept is another motivation for this function.
 *
 * Results:
 *      TRUE on success.
 *      FALSE on any failure (e.g., fork, setsid, WritePidfile, etc.)
 *
 * Side effects:
 *      Parent will exit if fork() succeeds.
 *      Caller is expected to be able to catch SIGPIPE.
 *
 *-----------------------------------------------------------------------------
 */

Bool
System_Daemon(Bool nochdir,
                 // IN: If TRUE, will -not- chdir("/").
              Bool noclose,
                 // IN: If TRUE, will -not- redirect stdin, stdout, and stderr
                 //     to /dev/null.
              const char *pidFile)
                 // IN: If non-NULL, will write the child's PID to this file.
{
   int fds[2];
   pid_t child;
   char buf;

   if (pipe(fds) == -1) {
      fprintf(stderr, "pipe failed: %s\n", strerror(errno));
      return FALSE;
   }

   child = fork();
   if (child == -1) {
      fprintf(stderr, "fork failed: %s\n", strerror(errno));
      return FALSE;
   }

   if (child) { /* Parent */
      ssize_t actual;

      /* Close unused write end of the pipe. */
      close(fds[1]);

      /*
       * Wait for the child to finish its critical initialization before the
       * parent exits.
       */
      do {
         actual = read(fds[0], &buf, sizeof buf);
      } while (actual == -1 && errno == EINTR);

      if (actual == -1) {
         fprintf(stderr, "read from pipe failed: %s\n", strerror(errno));
         _exit(EXIT_FAILURE);
      }

      _exit(EXIT_SUCCESS);
   } else { /* Child */
      /* Close unused read end of the pipe. */
      close(fds[0]);

      /*
       * The parent's caller might want to kill the child as soon as the
       * parent exits, so better guarantee that by that time the child's PID
       * has been written to the PID file.
       *
       * Note that because the parent knows the child's PID, the parent
       * could do this if needed. But because the parent cannot do the
       * setsid() below, the child might as well do both things here.
       */
      if (pidFile) {
         if (!System_WritePidfile(pidFile, getpid())) {
            goto kidfail;
         }
      }

      /*
       * The parent's caller might want to destroy the session as soon as
       * the parent exits, so better guarantee that by that time the child
       * has created its own new session.
       */
      if (setsid() == -1) {
         fprintf(stderr, "setsid failed: %s\n", strerror(errno));
         goto kidfail;
      }

      /*
       * The child has finished its critical initialization. Notify the
       * parent that it can exit.
       *
       * We are writing the first byte to the pipe. This cannot possibly
       * block (otherwise communication over a pipe would be impossible).
       * Consequently it cannot be interrupted by a signal either.
       *
       * See pipe(7) for more information.
       *
       * The caller registered a signal handler for SIGPIPE, right?!  We
       * won't treat EPIPE as an error, because we'd like to carry on with
       * our own life, even if our parent -did- abandon us.  ;_;
       */
      write(fds[1], &buf, sizeof buf);
      close(fds[1]);

      if (!nochdir && (chdir("/") == -1)) {
         fprintf(stderr, "chdir failed: %s\n", strerror(errno));
         return FALSE;
      }

      if (!noclose) {
         /*
          * The child has finished its initialization, and does not need to
          * output anything to stderr anymore. Re-assign all standard file
          * file descriptors.
          */
         int nullFd;

         nullFd = open("/dev/null", O_RDWR);
         if (nullFd == -1) {
            fprintf(stderr, "open of /dev/null failed: %s\n",
                    strerror(errno));
            return FALSE;
         }

         if ((dup2(nullFd, STDIN_FILENO) == -1) ||
             (dup2(nullFd, STDOUT_FILENO) == -1) ||
             (dup2(nullFd, STDERR_FILENO) == -1)) {
            fprintf(stderr, "dup2 failed: %s\n", strerror(errno));
            close(nullFd);
            return FALSE;
         }
      }
   }

   return TRUE;

kidfail:
   close(fds[1]);
   return FALSE;
}
