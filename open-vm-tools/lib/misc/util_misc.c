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
 * util.c --
 *
 *    misc util functions
 */

#if defined(_WIN32)
#include <winsock2.h> // also includes windows.h
#include <io.h>
#include <process.h>
#endif

#include "vm_ctype.h"
#include "safetime.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>

#if !defined(_WIN32) && !defined(N_PLAT_NLM)
#  if defined(linux)
#    include <sys/syscall.h> // for SYS_gettid
#  endif
#  include <unistd.h>
#  include <pwd.h>
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <pthread.h>
#endif

#include "vmware.h"
#include "msg.h"
#include "util.h"
#include "str.h"
/* For HARD_EXPIRE --hpreg */
#include "vm_version.h"
#include "su.h"
#include "escape.h"
#include "posix.h"

#if defined(_WIN32)
#include "win32u.h"
#include "win32util.h"
#endif
/*
 * ESX with userworld VMX
 */
#if defined(VMX86_SERVER)
#include "hostType.h"
#include "user_layout.h"
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * Util_GetCanonicalPath --
 *
 *      Canonicalizes a path name.
 *
 * Results:
 *      A freshly allocated canonicalized path name.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char*
Util_GetCanonicalPath(const char *path) // IN
{
   char *canonicalPath = NULL;
#if defined(__linux__) || defined(__APPLE__)
   canonicalPath = Posix_RealPath(path);
#elif defined(_WIN32)
   char driveSpec[4];
   Bool remoteDrive = FALSE;

   if (!path || !*path) {
      return NULL;
   }

   memcpy(driveSpec, path, 3);
   driveSpec[3] = '\0';

   if (strchr(VALID_DIRSEPS, driveSpec[0]) &&
       strchr(VALID_DIRSEPS, driveSpec[1])) {
      remoteDrive = TRUE;
   } else {
      remoteDrive = (GetDriveTypeA(driveSpec) == DRIVE_REMOTE);
   }

   /*
    * If the path is *potentially* a path to remote share, we do not
    * call GetLongPathName, because if the remote server is unreachable,
    * that function could hang. We sacrifice two things by doing so:
    * 1. The UNC path could refer to the local host and we incorrectly 
    *    assume remote.
    * 2. We do not resolve 8.3 names for remote paths.
    */
   if (remoteDrive) {
      canonicalPath = strdup(path);
   } else {
      canonicalPath = W32Util_RobustGetLongPath(path);
   }

#else
   NOT_IMPLEMENTED();
#endif
   return canonicalPath;
}


#if defined(_WIN32)
/*
 *-----------------------------------------------------------------------------
 *
 * Util_GetCanonicalPathForHash --
 *
 *      Utility function to both get the canonical version of the input path
 *      and produce a unique case-insensitive version of the path suitable
 *      for use as a seed to hash functions.
 *
 * Results:
 *       Canonicalized UTF-8 pathname suitable for use in hashing. 
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
Util_GetCanonicalPathForHash(const char *path) // IN: UTF-8
{
   char *ret = NULL;
   char *cpath = Util_GetCanonicalPath(path);
   
   if (cpath != NULL) {
      ret = Unicode_FoldCase(cpath);
      free(cpath);
   }   

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UtilGetLegacyEncodedString --
 *
 *      Takes a UTF-8 string, and allocates a new string in legacy encoding.
 *      This is necessary to maintain compatibility with older versions of
 *      the product, which may have stored strings (paths) in legacy
 *      encoding.  Hence, the use of WideCharToMultiByte().
 *
 * Results:
 *      An allocated string in legacy encoding (MBCS when applicable).  
 *      NULL on failure.
 *      
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static char*
UtilGetLegacyEncodedString(const char *path) // IN: UTF-8
{
   char *ret = NULL;
   char *cpath = Util_GetCanonicalPath(path);
  
   if (cpath != NULL) {
      char *apath = NULL;
      int retlen;
      WCHAR *wcpath = Unicode_GetAllocUTF16(cpath);

      /* First get the length of multibyte string */
      int alen = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, wcpath, -1, 
                                     NULL, 0, NULL, NULL);
      if (alen > 0) {
         /* Now get the converted string */
         ret = Util_SafeMalloc(alen);
         retlen = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, wcpath, -1, 
                                      ret, alen, NULL, NULL);
         if (retlen != alen) {
            free(ret);
            ret = NULL;
         }
      }
      free(cpath);
      free(wcpath);
   }
  
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_CompatGetCanonicalPath --
 *
 *      Canonicalizes a path name (compatibility version).
 *
 * Results:
 *      A freshly allocated canonicalized path name in legacy encoding
 *      (MBCS when applicable).
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char*
Util_CompatGetCanonicalPath(const char *path) // IN: UTF-8
{
   char *cpath = Util_GetCanonicalPath(path);
   char *ret = NULL;

   if (cpath != NULL) {
      ret = UtilGetLegacyEncodedString(cpath);
      free(cpath);
   }
   
   return ret;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Util_CanonicalPathsIdentical --
 *
 *      Utility function to compare two paths that have already been made
 *      canonical. This function exists to mask platform differences in 
 *      path case-sensitivity.
 *
 *      XXX: This implementation makes assumptions about the host filesystem's
 *           case sensitivity without any regard to what filesystem the provided
 *           paths actually use. There are many ways to break this assumption,
 *           on any of our supported host OSes! The return value of this function
 *           cannot be trusted.
 *
 * Results:
 *      TRUE if the paths are equivalenr, FALSE if they are not.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Util_CanonicalPathsIdentical(const char *path1, // IN
                             const char *path2) // IN
{
   ASSERT(path1);
   ASSERT(path2);
#if defined(linux)
   return (strcmp(path1, path2) == 0);
#elif defined(_WIN32)
   return (_stricmp(path1, path2) == 0);
#elif defined(__APPLE__)
   return (strcasecmp(path1, path2) == 0);
#else
   NOT_IMPLEMENTED();
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_IsAbsolutePath --
 *
 *      Checks if the given path is absolute.
 *
 * Results:
 *      TRUE if the path is absolute, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Util_IsAbsolutePath(const char *path)  // IN: path to check
{
#if defined(__linux__) || defined(__APPLE__)
   return path && path[0] == DIRSEPC;
#elif defined(_WIN32)
   if (!path) {
      return FALSE;
   }

   // <Drive letter>:\path
   if (CType_IsAlpha(path[0]) && path[1] == ':' && path[2] == DIRSEPC) {
      return TRUE;
   }

   // UNC paths
   if (path[0] == DIRSEPC && path[1] == DIRSEPC) {
      return TRUE;
   }

   return FALSE;
#else
   NOT_IMPLEMENTED();
#endif
   NOT_REACHED();
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_GetPrime --
 *
 *      Find next prime.
 *
 * Results:
 *      The smallest prime number greater than or equal to n0.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

unsigned
Util_GetPrime(unsigned n0)
{
   unsigned i, ii, n, nn;

   /*
    * Keep the main algorithm clean by catching edge cases here.
    * There is no 32-bit prime larger than 4294967291.
    */

   ASSERT_NOT_IMPLEMENTED(n0 <= 4294967291U);
   if (n0 <= 2) {
      return 2;
   }

   for (n = n0 | 1;; n += 2) {
      /*
       * Run through the numbers 3,5, ..., sqrt(n) and check that none divides
       * n.  We exploit the identity (i + 2)^2 = i^2 + 4i + 4 to incrementially
       * maintain the square of i (and thus save a multiplication each
       * iteration).
       *
       * 65521 is the largest prime below 0xffff, which is where
       * we can stop.  Using it instead of 0xffff avoids overflowing ii.
       */
      nn = MIN(n, 65521U * 65521U);
      for (i = 3, ii = 9;; ii += 4*i+4, i += 2) {
         if (ii > nn) {
            return n;
         }
         if (n % i == 0) {
            break;
         }
      }
   }
}


#ifndef N_PLAT_NLM
#if defined(linux)
/*
 *-----------------------------------------------------------------------------
 *
 * gettid --
 *
 *      Retrieve unique thread identification suitable for kill or setpriority.
 *	Do not call this function directly, use Util_GetCurrentThreadId() instead.
 *
 * Results:
 *      Unique thread identification on success.
 *      (pid_t)-1 on error, errno set (when kernel does not support this call)
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE
pid_t gettid(void)
{
#if defined(SYS_gettid)
   return (pid_t)syscall(SYS_gettid);
#else
   return -1;
#endif
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Util_GetCurrentThreadId --
 *
 *      Retrieves a unique thread identification suitable to identify a thread
 *      to kill it or change its scheduling priority.
 *
 * Results:
 *      Unique thread identification on success.
 *	ASSERTs on failure (should not happen).
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Util_ThreadID
Util_GetCurrentThreadId(void)
{
#if defined(linux)
   /*
    * It is possible that two threads can enter gettid() path simultaneously,
    * both eventually clearing useTid to zero. It does not matter - only
    * problem which can happen is that useTid will be set to zero twice.
    * And it has no impact on useTid value...
    */

   static int useTid = 1;
   pid_t tid;

   // ESX with userworld VMX
#if defined(VMX86_SERVER)
   if (HostType_OSIsVMK()) {
      return User_GetTid();
   }
#endif

   if (useTid) {
      tid = gettid();
      if (tid != (pid_t)-1) {
         return tid;
      }
      ASSERT(errno == ENOSYS);
      useTid = 0;
   }
   tid = getpid();
   ASSERT(tid != (pid_t)-1);
   return tid;
#elif defined(sun)
   pid_t tid;

   tid = getpid();
   ASSERT(tid != (pid_t)-1);
   return tid;
#elif defined(__APPLE__) || defined(__FreeBSD__)
   ASSERT_ON_COMPILE(sizeof(Util_ThreadID) == sizeof(pthread_t));
   return pthread_self();
#elif defined(_WIN32)
   return GetCurrentThreadId();
#else
#error "Unknown platform"
#endif
}

#endif // N_PLAT_NLM

