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
#if !defined(__APPLE__)
#include <sys/timex.h>
#endif
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
#include "dynbuf.h"
#include "hashTable.h"
#include "strutil.h"

#define MAX_IFACES      4
#define LOOPBACK        "lo"
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

/*
 * The interval between two ticks (in usecs) can only be altered by 10%,
 * and the default value is 10000. So the values 900000L and 1000000L
 * divided by USER_HZ, which is 100.
 */
#ifdef __linux__
#   define USER_HZ               100			/* from asm/param.h  */
#   define TICK_INCR_NOMINAL    (1000000L / USER_HZ)	/* nominal tick increment */
#   define TICK_INCR_MAX        (1100000L / USER_HZ)	/* maximum tick increment */
#   define TICK_INCR_MIN        (900000L / USER_HZ)	/* minimum tick increment */
#endif


/*
 * Data types
 */

/*
 * DynBuf container used by SNEForEachCallback.
 *
 * The SNE prefix is short for "System Native Environ".
 */

typedef struct {
   DynBuf *nativeEnvironStrings;        // FOO=BAR<NUL>BAZ=BOOM<NUL><NUL>
   DynBuf *nativeEnvironOffsets;        // off_ts relative to nativeEnvironStrings->data
} SNEBufs;


/*
 * Local function prototypes
 */


/*
 * "System Native Environ" (SNE) helpers.
 */
static HashTable *SNEBuildHash(const char **nativeEnviron);
static const char **SNEHashToEnviron(HashTable *environTable);
static int SNEForEachCallback(const char *key, void *value, void *clientData);


/*
 * Global functions
 */


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
 *----------------------------------------------------------------------
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
 *      TRUE/FALSE: success/failure
 *
 * Side effects:
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
 * System_EnableTimeSlew --
 *
 *      Slew the clock so that the time difference is covered within
 *      the timeSyncPeriod. timeSyncPeriod is the interval of the time
 *      sync loop and we intend to catch up delta us.
 *
 *      timeSyncPeriod is ignored on FreeBSD and Solaris.
 *
 * Results:
 *      TRUE/FALSE: success/failure
 *
 * Side effects:
 *      This changes the tick frequency and hence needs to be reset
 *      after the time sync is achieved.
 *
 *----------------------------------------------------------------------
 */

Bool
System_EnableTimeSlew(int64 delta,            // IN: Time difference in us
                      uint32 timeSyncPeriod)  // IN: Time interval in 100th of a second
{
#if defined(__FreeBSD__) || defined(sun)

   struct timeval tx;
   struct timeval oldTx;
   int error;

   tx.tv_sec = delta / 1000000L;
   tx.tv_usec = delta % 1000000L;

   error = adjtime(&tx, &oldTx);
   if (error) {
      Log("%s: adjtime failed\n", __FUNCTION__);
      return FALSE;
   }
   Log("%s: time slew start.\n", __FUNCTION__);
   return TRUE;

#elif defined(__linux__) /* For Linux. */

   struct timex tx;
   int error;
   uint64 tick;
   uint64 timeSyncPeriodUS = timeSyncPeriod * 10000L;

   ASSERT(timeSyncPeriod);

   /*
    * Set the tick so that delta time is corrected in timeSyncPeriod period.
    * tick is the number of microseconds added per clock tick. We adjust this
    * so that we get the desired delta + the timeSyncPeriod in timeSyncPeriod
    * interval.
    */
   tx.modes = ADJ_TICK;
   tick = (timeSyncPeriodUS + delta) / ((timeSyncPeriod / 100) * USER_HZ);
   if (tick > TICK_INCR_MAX) {
      tick = TICK_INCR_MAX;
   } else if (tick < TICK_INCR_MIN) {
      tick = TICK_INCR_MIN;
   }
   tx.tick = tick;

   error = adjtimex(&tx);
   if (error == -1) {
      Log("%s: adjtimex failed: %d %s\n", __FUNCTION__, error, strerror(errno));
         return FALSE;
   }
   Log("%s: time slew start: %ld\n", __FUNCTION__, tx.tick);
   return TRUE;

#else /* Apple */

   return FALSE;

#endif
}


/*
 *----------------------------------------------------------------------
 *
 * System_DisableTimeSlew --
 *
 *      Disable time slewing, setting the tick frequency to default.
 *
 * Results:
 *      TRUE/FALSE: success/failure
 *
 * Side effects:
 *      If failed to disable the tick frequency, system time will
 *      not reflect the actual time - will be behind.
 *
 *----------------------------------------------------------------------
 */

Bool
System_DisableTimeSlew(void)
{
#if defined(__FreeBSD__) || defined(sun)

   struct timeval tx = {0};
   int error;

   error = adjtime(&tx, NULL);
   if (error) {
      Log("%s: adjtime failed\n", __FUNCTION__);
      return FALSE;
   }
   return TRUE;

#elif defined(__linux__) /* For Linux. */

   struct timex tx;
   int error;

   tx.modes = ADJ_TICK;
   tx.tick = TICK_INCR_NOMINAL;

   error = adjtimex(&tx);
   if (error == -1) {
      Log("%s: adjtimex failed: %d %s\n", __FUNCTION__, error,
            strerror(errno));
      return FALSE;
   }
   Log("%s: time slew end - %d\n", __FUNCTION__, error);
   return TRUE;

#else /* Apple */
   return TRUE;

#endif
}


/*
 *----------------------------------------------------------------------
 *
 * System_IsTimeSlewEnabled --
 *
 *      Returns TRUE if time slewing has been enabled.
 *
 * Results:
 *      TRUE/FALSE: enabled/disabled
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
System_IsTimeSlewEnabled(void)
{
#if defined(__FreeBSD__) || defined(sun)

   struct timeval oldTx;
   int error;

   /* 
    * Solaris needs first argument non-NULL and zero
    * to get the old timeval value.
    */
#if defined(sun)
   struct timeval tx = {0};
   error = adjtime(&tx, &oldTx);
#else
   error = adjtime(NULL, &oldTx);
#endif
   if (error) {
      Log("%s: adjtime failed: %s.\n", __FUNCTION__, strerror(errno));
      return FALSE;
   }
   return ((oldTx.tv_sec || oldTx.tv_usec) ? TRUE : FALSE);

#elif defined(__linux__) /* For Linux. */

   struct timex tx = {0};
   int error;

   error = adjtimex(&tx);
   if (error == -1) {
      Log("%s: adjtimex failed: %d %s\n", __FUNCTION__, error,
            strerror(errno));
      return FALSE;
   }
   return ((tx.tick == TICK_INCR_NOMINAL) ? FALSE : TRUE);

#else /* Apple */

   return FALSE;

#endif
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
 *      TRUE/FALSE: success/failure
 *
 * Side effects:
 *      This function disables any time slewing to correctly set the guest
 *      time.
 *
 *----------------------------------------------------------------------
 */

Bool
System_AddToCurrentTime(int64 deltaSecs,  // IN
                        int64 deltaUsecs) // IN
{
   struct timeval tv;
   int64 newTime;
   int64 secs;
   int64 usecs;
   
   if (!System_GetCurrentTime(&secs, &usecs)) {
      return FALSE;
   }
   
   if (System_IsTimeSlewEnabled()) {
      System_DisableTimeSlew();
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
 *    TRUE if this is an ACPI system.
 *    FALSE if this is not an ACPI system.   
 *
 * Side effects:
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
   if (system(cmd) == -1) {
      fprintf(stderr, "Unable to execute %s command: \"%s\"\n",
              reboot ? "reboot" : "shutdown", cmd);
   }
}


/*
 *-----------------------------------------------------------------------------
 *  
 * System_IsUserAdmin -- 
 *
 *    On Windows this functions checks if the calling user has membership in
 *    the Administrators group (for NT platforms). On POSIX machines, we simply
 *    check if the user's effective UID is root.
 * 
 * Return value: 
 *    TRUE if the user has an effective UID of root.
 *    FALSE if not.
 * 
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
System_IsUserAdmin(void)
{
   return geteuid() == 0;
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
 *----------------------------------------------------------------------
 *
 * System_UnsetEnv --
 *
 *    Unset environment variable. 
 *
 * Results:
 *    0 if success, -1 otherwise.
 *
 * Side effects:
 *    Unsets the environment variable.
 *
 *----------------------------------------------------------------------
 */

int
System_UnsetEnv(const char *valueName) // IN: UTF-8
{
#if defined(sun)
   return(-1);
#else
   Posix_Unsetenv(valueName);
   return 0;
#endif
} // System_UnsetEnv


/*
 *----------------------------------------------------------------------
 *
 * System_SetLDPath --
 *
 *    Set LD_LIBRARY_PATH. If native is TRUE, use VMWARE_LD_LIBRARY_PATH 
 *    as the value (and ignore the path argument, which should be set to
 *    NULL in this case). If native is FALSE, use the passed in path (and
 *    if that path is NULL, unsetenv the value).
 *
 * Results:
 *    The previous value of the environment variable. The caller is
 *    responsible for calling free() on this pointer.
 *
 * Side effects:
 *    Manipulates the value of LD_LIBRARY_PATH variable. 
 *
 *----------------------------------------------------------------------
 */

char *
System_SetLDPath(const char *path,      // IN: UTF-8
                 const Bool native)     // IN: If TRUE, ignore path and use VMware value
{
   char *vmldpath = NULL;
   char *tmppath = NULL;
   char *oldpath;

   ASSERT(!native || path == NULL);

   /*
    * Get the original LD_LIBRARY_PATH, so the installed applications
    * don't try to use our versions of the libraries.
    *
    * From bora/apps/lib/lui/browser.cc::ResetEnvVars():
    *
    * For each variable we care about, there are three possible states:
    * 1) The variable was not considered for saving.
    *    This is indicated by VMWARE_<foo>'s first character not being
    *    set to either "1" or "0".
    *    We should not manipulate <foo>.
    * 2) The variable was considered but was not set.
    *    VMWARE_<foo> is set to "0".
    *    We should unset <foo>.
    * 3) The variable was considered and was set.
    *    VMWARE_<foo> is set to "1" + "<value of foo>".
    *    We should set <foo> back to the saved value.
    *
    * XXX: There are other variables that may need resetting (for a list, check
    * bora/apps/lib/lui/browser.cc or the wrapper scripts), but that would
    * require some more refactoring of this code (not using system(3), for
    * example).
    */
   oldpath = System_GetEnv(TRUE, "LD_LIBRARY_PATH");
   if (native == TRUE) {
      char *p = NULL;
      vmldpath = tmppath = System_GetEnv(TRUE, "VMWARE_LD_LIBRARY_PATH");
      if (vmldpath && strlen(vmldpath) && vmldpath[0] == '1') {
         vmldpath++;
      } else {
         vmldpath = p = Util_SafeStrdup("");
      }
      if (System_SetEnv(TRUE, "LD_LIBRARY_PATH", vmldpath) == -1) {
         Debug("%s: failed to set LD_LIBRARY_PATH\n", __FUNCTION__);
      }
      free(tmppath);
      free(p);
   } else if (path) {
      /*
       * Set LD_LIBRARY_PATH to the specified value. 
       */
      System_SetEnv(TRUE, "LD_LIBRARY_PATH", (char *) path);
   } else {
      System_UnsetEnv("LD_LIBRARY_PATH");
   }
   return oldpath;
} // System_SetLDPath


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
      if (write(fds[1], &buf, sizeof buf) == -1) {
         fprintf(stderr, "write failed: %s\n", strerror(errno));
         close(fds[1]);
         return FALSE;
      }
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


/*
 *-----------------------------------------------------------------------------
 *
 * System_GetNativeEnviron --
 *
 *      Returns a copy of the native / unwrapped environment.
 *
 *      VMware's compatibility library wrappers override certain environment
 *      variables to make use of shipped libraries.  This creates the
 *      "compatibility environment".  Overridden variables are saved into
 *      corresponding VMWARE_-prefixed variables.  This routine recreates the
 *      "native environment" by restoring VMWARE_-prefixed variable values to
 *      their native equivalents.
 *
 *      Every value created by the wrapper begins with a 1 or 0 to indicate
 *      whether the value was set in the native environment.  Based on this:
 *        VMWARE_FOO="1foo"     -> FOO="foo"
 *        VMWARE_FOO="1"        -> FOO=""
 *        VMWARE_FOO="0"        -> FOO is unset in the native environment
 *
 *      Variables without the VMWARE_ prefix are just copied over to the new
 *      environment.  Note, of course, that VMWARE_-prefixed variables take
 *      precedence.  (I.e., when we encounter a VMWARE_ prefix, we'll
 *      unconditionally record the value.  For all other variables, we'll
 *      copy only if that variable had not already been set.)
 *
 * Results:
 *      An array of strings representing the native environment.  (I.e.,
 *      use it as you would environ(7).)  NB:  Memory allocation failures
 *      are considered fatal.
 *
 * Side effects:
 *      This routine allocates memory.  It's up to the caller to free it by
 *      passing the returned value to System_FreeNativeEnviron.
 *
 *      This routine assumes that keys will show up in environ only once each.
 *
 *-----------------------------------------------------------------------------
 */

const char **
System_GetNativeEnviron(const char **compatEnviron)
                           // IN: original "compatibility" environment
{
   HashTable *environTable;
   const char **nativeEnviron;

   environTable = SNEBuildHash(compatEnviron);
   nativeEnviron = SNEHashToEnviron(environTable);

   HashTable_Free(environTable);

   return nativeEnviron;
}


/*
 *-----------------------------------------------------------------------------
 *
 * System_FreeNativeEnviron --
 *
 *      Frees memory allocated by System_GetNativeEnviron.
 *
 * Results:
 *      Frees memory.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
System_FreeNativeEnviron(const char **nativeEnviron)
                            // IN: environment returned by System_GetNativeEnviron
{
   /*
    * nativeEnviron is an array of strings, and all of the strings are located in a
    * single contiguous buffer.  This makes freeing both array and buffer easy.
    */
   char *stringBuf = (char *)*nativeEnviron;

   free(stringBuf);
   free(nativeEnviron);
}


/*
 * Local functions
 */


/*
 *-----------------------------------------------------------------------------
 *
 * SNEBuildHash --
 *
 *      Compile hash table of environment variables.  See System_GetNativeEnviron
 *      for rules on precedence.
 *
 * Results:
 *      Pointer to populated hash table.  This cannot fail, as any memory failures
 *      within the HashTable and StrUtil libraries are considered fatal.
 *
 * Side effects:
 *      Caller is responsible for freeing returned table with HashTable_Free.
 *
 *-----------------------------------------------------------------------------
 */

static HashTable *
SNEBuildHash(const char **compatEnviron)
                // IN: original "compatibility" environment
{
   HashTable *environTable;
   const char **p;

   /*
    * Number of buckets picked arbitrarily.  We're more interested in having an
    * associative array than raw table performance.
    */
   environTable = HashTable_Alloc(64, HASH_STRING_KEY | HASH_FLAG_COPYKEY, free);

   for (p = compatEnviron; p && *p; p++) {
      const size_t prefixLength = sizeof "VMWARE_" - 1;
      char *key;
      char *value;
      unsigned int index;

      index = 0;
      key = StrUtil_GetNextToken(&index, *p, "=");
      if (!key) {
         /* XXX Must empty environment variables still contain a '=' delimiter? */
         Debug("%s: Encountered environment entry without '='.\n", __func__);
         continue;
      }

      /*
       * Copy the value beginning after the '=' delimiter (even if it's empty).
       */
      ++index;
      value = Util_SafeStrdup(&(*p)[index]);

      if (StrUtil_StartsWith(key, "VMWARE_") &&
          key[prefixLength] != '\0' &&
          (value[0] == '0' || value[0] == '1')) {
         /*
          * Okay, this appears to be one of the wrapper's variables, so let's
          * figure out the original environment variable name (by just indexing
          * past the prefix) and value (by indexing past the "was this variable
          * in the native environment?" marker).
          */
         char *realKey = &key[prefixLength];
         char *realValue = (value[0] == '0') ? NULL : Util_SafeStrdup(&value[1]);
         HashTable_ReplaceOrInsert(environTable, realKey, realValue);
      } else {
         HashTable_LookupOrInsert(environTable, key, value);
      }

      /*
       * The hash table makes a copy of our key, and it takes ownership of inserted
       * values (via our passed freeing function).  So that means we're responsible
       * for freeing 'key', but -not- 'value'.
       */
      free(key);
   }

   return environTable;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SNEHashToEnviron --
 *
 *      Builds up an array of strings representing a new environment based on
 *      the caller's hash table.
 *
 * Results:
 *      Pointer to the new environment.  As memory allocation failures are
 *      considered fatal, this routine will not return NULL.
 *
 * Side effects:
 *      This is expected to be returned to System_GetNativeEnviron's caller,
 *      and s/he is to free it via System_FreeNativeEnviron.
 *
 *-----------------------------------------------------------------------------
 */

static const char **
SNEHashToEnviron(HashTable *environTable)   // IN:
{
   DynBuf nativeEnvironOffsets;
   DynBuf nativeEnvironStrings;

   SNEBufs anonBufs = {
      .nativeEnvironStrings = &nativeEnvironStrings,
      .nativeEnvironOffsets = &nativeEnvironOffsets,
   };

   const char **nativeEnviron;
   off_t *offsetIter;
   char *stringsBase;

   unsigned int numStrings;
   unsigned int i;

   DynBuf_Init(&nativeEnvironStrings);
   DynBuf_Init(&nativeEnvironOffsets);

   /*
    * Write out strings and string offsets to the dynbufs.
    */
   HashTable_ForEach(environTable, SNEForEachCallback, &anonBufs);
   ASSERT_MEM_ALLOC(DynBuf_Trim(&nativeEnvironStrings));

   /*
    * Allocate final buffer to contain the string array.  Include extra entry
    * for terminator.
    */
   numStrings = DynBuf_GetSize(&nativeEnvironOffsets) / sizeof *offsetIter;
   nativeEnviron = Util_SafeCalloc(1 + numStrings, sizeof *nativeEnviron);

   stringsBase = DynBuf_Get(&nativeEnvironStrings);
   offsetIter = DynBuf_Get(&nativeEnvironOffsets);

   for (i = 0; i < numStrings; i++, offsetIter++) {
      nativeEnviron[i] = stringsBase + *offsetIter;
   }
   nativeEnviron[i] = NULL;

   /*
    * Cleanup.
    *
    * Note the subtle difference in how these are handled:
    *   - The offsets are used only to build the array of strings, which is why that
    *     dynbuf is destroyed.
    *   - The buffer containing all of the environment strings, however, is persistent
    *     and is actually returned to the caller (indirectly via nativeEnviron).  As
    *     such it's only detached from the DynBuf.
    */
   DynBuf_Destroy(&nativeEnvironOffsets);
   DynBuf_Detach(&nativeEnvironStrings);

   return nativeEnviron;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SNEForEachCallback --
 *
 *      Given a key (environment variable name) and value, appends a string of
 *      "key=value" to the nativeEnvironStrings DynBuf.  Also appends a pointer
 *      to the new string to the nativeEnviron DynBuf.
 *
 * Results:
 *      Always zero.  Memory allocation failures are considered fatal.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
SNEForEachCallback(const char *key,     // IN: environment variable
                   void *value,         // IN: environment value
                   void *clientData)    // IN/OUT: DynBuf container (SNEBufs)
{
   DynBuf *nativeEnvironStrings = ((SNEBufs *)clientData)->nativeEnvironStrings;
   DynBuf *nativeEnvironOffsets = ((SNEBufs *)clientData)->nativeEnvironOffsets;
   size_t itemSize;
   char *itemBuf;
   off_t itemOffset;

   /*
    * A NULL value indicates that this variable is not to be set.
    */
   if (value == NULL) {
      return 0;
   }

   /* Determine the length of the new string inc. '=' delimiter and NUL. */
   itemSize = strlen(key) + strlen(value) + sizeof "=";
   itemBuf = Util_SafeMalloc(itemSize);

   /* Create new "key=value" string. */
   snprintf(itemBuf, itemSize, "%s=%s", key, (char *)value);

   ASSERT_MEM_ALLOC(DynBuf_AppendString(nativeEnvironStrings, itemBuf));

   /*
    * Get the relative offset of our newly added string (relative to the DynBuf's base
    * address), and then append that to nativeEnvironOffsets.
    */
   itemOffset = DynBuf_GetSize(nativeEnvironStrings) - itemSize;
   ASSERT_MEM_ALLOC(DynBuf_Append(nativeEnvironOffsets, &itemOffset, sizeof itemOffset));

   free(itemBuf);

   return 0;
}
