/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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

#if defined sun || defined __APPLE__
#   include <utmpx.h>
#endif

#ifdef __FreeBSD__
#include "ifaddrs.h"
#endif

#ifdef USERWORLD
#include <vm_basic_types.h>
#include <vmkuserstatus.h>
#include <vmkuseruptime.h>
#endif

#include "vm_assert.h"
#include "system.h"
#include "debug.h"
#include "posix.h"
#include "unicode.h"
#include "dynbuf.h"
#include "hashTable.h"
#include "strutil.h"
#include "vmstdio.h"

#define MAX_IFACES      4
#define LOOPBACK        "lo"
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
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
 *-----------------------------------------------------------------------------
 *
 * System_GetTimeMonotonic --
 *
 *      See POSIX clock_gettime(CLOCK_MONOTONIC).
 *
 * Results:
 *      Returns monotonically increasing time in hundredths of a second on
 *      success, -1 on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

uint64
System_GetTimeMonotonic(void)
{
   /*
    * Dummy variable b/c times(NULL) segfaults on FreeBSD 3.2 --greg
    */
   struct tms tp;

#if !defined VM_64BIT
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
#else  // VM_64BIT
#if defined sun || defined __APPLE__
   /*
    * PR 1710952 and PR 2136820
    * times() on Solaris & Mac can return a lower value than the
    * one in a previous call. As a workaround, we return the last
    * cached value when we get a lower value from times().
    */
   static Atomic_uint64 last = { 0 };

   while (1) {
      uint64 last1 = Atomic_Read64(&last);
      uint64 now = times(&tp);

      if (now > last1) {
         uint64 last2 = Atomic_ReadIfEqualWrite64(&last, last1, now);
         /* check if another thread changed last, and try again if true */
         if (last2 != last1) {
            continue;
         }
         return now;
      } else {
         return last1;
      }
   }
#else
   return times(&tp);
#endif
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * System_Uptime --
 *
 *      Retrieve the time (in hundredth of s.) since the system has started.
 *
 * Result:
 *      Returns the uptime on success or -1 on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

uint64
System_Uptime(void)
{
   uint64 uptime = -1;

#ifdef USERWORLD
   {
      VmkuserStatus_Code status;
      uint64 sysUptime;
      status = VmkuserUptime_GetUptime(&sysUptime);
      if (VmkuserStatus_IsOK(status)) {
         uptime = sysUptime / 10000;
      }
   }
#elif defined(__linux__)
   {
      FILE *procStream;
      char *buf = NULL;
      size_t bufSize;
      uint64 sec;
      unsigned int csec;

      if (((procStream = Posix_Fopen("/proc/uptime", "r")) != NULL) &&
          (StdIO_ReadNextLine(procStream, &buf, 80, &bufSize) == StdIO_Success) &&
          (sscanf(buf, "%"FMT64"u.%2u", &sec, &csec) == 2)) {
         uptime = sec * 100 + csec;
      } else {
         Warning("%s: Unable to parse /proc/uptime.\n", __func__);
      }

      free(buf);

      if (procStream) {
         fclose(procStream);
      }
   }
#elif defined sun || defined __APPLE__
   {
      struct utmpx *boot, tmp;

      tmp.ut_type = BOOT_TIME;
      if ((boot = getutxid(&tmp)) != NULL) {
         struct timeval now;
         struct timeval *boottime = &boot->ut_tv;

         gettimeofday(&now, NULL);
         uptime =
            (now.tv_sec * 100 + now.tv_usec / 10000) -
            (boottime->tv_sec * 100 + boottime->tv_usec / 10000);
      } else {
         Warning("%s: Unable to determine boot time.\n", __func__);
      }

      endutxent();
   }
#else // FreeBSD
   {
      /*
       * FreeBSD: src/usr.bin/w/w.c rev 1.59:
       *   "Obtain true uptime through clock_gettime(CLOCK_MONOTONIC,
       *    struct *timespec) instead of subtracting 'bootime' from 'now'."
       */
      struct timespec ts;

      if (clock_gettime(CLOCK_MONOTONIC, &ts) != -1) {
         uptime = ts.tv_sec * 100 + ts.tv_nsec / 10000000;
      } else {
         Warning("%s: clock_gettime: %d\n", __func__, errno);
      }
   }
#endif

   return uptime;
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
   char *cmd;

   if (reboot) {
#if defined(REBOOT_COMMAND)
      cmd = REBOOT_COMMAND;
#elif defined(sun)
      cmd = "/usr/sbin/shutdown -g 0 -i 6 -y";
#elif defined(USERWORLD)
      cmd = "/bin/reboot";
#else
      cmd = "/sbin/shutdown -r now";
#endif
   } else {
#if defined(SHUTDOWN_COMMAND)
      cmd = SHUTDOWN_COMMAND;
#elif __FreeBSD__
      cmd = "/sbin/shutdown -p now";
#elif defined(sun)
      cmd = "/usr/sbin/shutdown -g 0 -i 5 -y";
#elif defined(USERWORLD)
      cmd = "/bin/halt";
#else
      cmd = "/sbin/shutdown -h now";
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
System_GetEnv(Bool global,           // IN
              const char *valueName) // IN: UTF-8
{
   char *result;

   result = Posix_Getenv(valueName);

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
              const char *valueName,  // IN: UTF-8
              const char *value)      // IN: UTF-8
{
   return Posix_Setenv(valueName, value, 1);
} // System_SetEnv


/*
 *-----------------------------------------------------------------------------
 *
 * System_GetNodeName --
 *
 *      Returns the guest's configured node name.  Does not necessarily
 *      correspond to a proper DNS host name.
 *
 * Results:
 *      On success, returns TRUE with node name written to outBuf.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
System_GetNodeName(size_t outBufSize, // IN:  size of output buffer
                   char *outBuf)      // OUT: output buffer
{
   ASSERT(outBuf);

   if (gethostname(outBuf, outBufSize) < 0) {
      Debug("Error, gethostname failed\n");
      return FALSE;
   }

   return TRUE;
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
 *        VMWARE_FOO="1foo"               -> FOO="foo"
 *        VMWARE_FOO="1"                  -> FOO=""
 *        VMWARE_FOO="0"                  -> FOO is unset in the native environment
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
          *
          * XXX Should we move this marker to a separate header?
          */
         char *realKey = &key[prefixLength];
         char *realValue = (value[0] == '0')
                           ? NULL
                           : Util_SafeStrdup(&value[1]);
         free(value);
         value = NULL;
         HashTable_ReplaceOrInsert(environTable, realKey, realValue);
      } else {
         void *hashed = HashTable_LookupOrInsert(environTable, key, value);
         if (hashed != value) {
            /*
             * The key already exists in the hashtable and its value was
             * not replaced. We need to free the memory allocated for 'value'.
             */
            free(value);
            value = NULL;
         }
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
