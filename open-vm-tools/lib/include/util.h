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
 * util.h --
 *
 *    misc util functions
 */

#ifndef UTIL_H
#define UTIL_H

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include <stdarg.h>
#include <string.h>
#ifndef VMKBOOT
#include <stdlib.h>
#endif

#ifdef _WIN32
   #ifdef USERLEVEL
      #include <tchar.h>   /* Needed for MBCS string functions */
      #include <windows.h> /* for definition of HANDLE */
   #endif
#else
   #include <unistd.h>
   #include <sys/types.h>
   #include "errno.h"
#endif
#include "vm_assert.h"
#include "vm_basic_defs.h"
#include "unicodeTypes.h"
#include "utilZero.h"

#if defined(__cplusplus)
extern "C" {
#endif

uint32 CRC_Compute(const uint8 *buf, int len);
uint32 Util_Checksum32(const uint32 *buf, int len);
uint32 Util_Checksum(const uint8 *buf, int len);
uint32 Util_Checksumv(void *iov, int numEntries);
uint32 Util_HashString(const char *str);
char *Util_ExpandString(const char *fileName);
void Util_ExitThread(int);
NORETURN void Util_ExitProcessAbruptly(int);
int Util_HasAdminPriv(void);
#if defined _WIN32 && defined USERLEVEL
int Util_TokenHasAdminPriv(HANDLE token);
#endif

Bool Util_Data2Buffer(char *buf, size_t bufSize, const void *data0,
                      size_t dataSize);
Bool Util_Data2BufferEx(char *buf, size_t bufSize, const void *data0,
                        size_t dataSize, char sep);

char *Util_GetCanonicalPath(const char *path);
#ifdef _WIN32
char *Util_CompatGetCanonicalPath(const char *path);
char *Util_GetCanonicalPathForHash(const char *path);
char *Util_CompatGetLowerCaseCanonicalPath(const char* path);
#endif
int Util_BumpNoFds(uint32 *cur, uint32 *wanted);
Bool Util_CanonicalPathsIdentical(const char *path1, const char *path2);
Bool Util_IsAbsolutePath(const char *path);

char *Util_DeriveFileName(const char *source,
                          const char *name,
                          const char *ext);

typedef struct UtilSingleUseResource UtilSingleUseResource;
UtilSingleUseResource *Util_SingleUseAcquire(const char *name);
void Util_SingleUseRelease(UtilSingleUseResource *res);

#ifndef _WIN32
Bool Util_IPv4AddrValid(const char *addr);
Bool Util_IPv6AddrValid(const char *addr);
Bool Util_IPAddrValid(const char *addr);
#endif

#if defined(__linux__) || defined(__FreeBSD__) || defined(sun)
Bool Util_GetProcessName(pid_t pid, char *bufOut, size_t bufOutSize);
#endif

#if defined __linux__ && !defined VMX86_SERVER
Bool Util_IsPhysicalSSD(const char* device);
#endif

// backtrace functions and utilities

#define UTIL_BACKTRACE_LINE_LEN (511)
typedef void (*Util_OutputFunc)(void *data, const char *fmt, ...);

void Util_Backtrace(int bugNr);
void Util_BacktraceFromPointer(uintptr_t *basePtr);
void Util_BacktraceFromPointerWithFunc(uintptr_t *basePtr,
                                       Util_OutputFunc outFunc,
                                       void *outFuncData);
void Util_BacktraceWithFunc(int bugNr,
                            Util_OutputFunc outFunc,
                            void *outFuncData);

void Util_BacktraceToBuffer(uintptr_t *basePtr,
                            uintptr_t *buffer, int len);

// sleep functions

void Util_Usleep(long usec);
void Util_Sleep(unsigned int sec);

int Util_CompareDotted(const char *s1, const char *s2);

/*
 * This enum defines how Util_GetOpt should handle non-option arguments:
 *
 * UTIL_NONOPT_PERMUTE: Permute argv so that all non-options are at the end.
 * UTIL_NONOPT_STOP:    Stop when first non-option argument is seen. (This is
 *                      the standard POSIX behavior.)
 * UTIL_NONOPT_ALL:     Return each non-option argument as if it were
 *                      an option with character code 1.
 */
typedef enum { UTIL_NONOPT_PERMUTE, UTIL_NONOPT_STOP, UTIL_NONOPT_ALL } Util_NonOptMode;
struct option;
int Util_GetOpt(int argc, char * const *argv, const struct option *opts,
                Util_NonOptMode mode, Bool manualErrorHandling);


#if defined(VMX86_STATS)
Bool Util_QueryCStResidency(uint32 *numCpus, uint32 *numCStates,
                            uint64 **transitns, uint64 **residency,
                            uint64 **transTime, uint64 **residTime);
#endif

#define UTIL_FASTRAND_SEED_MAX (0x7fffffff)
Bool Util_Throttle(uint32 count);
uint32 Util_FastRand(uint32 seed);

// Not thread safe!
void Util_OverrideHomeDir(const char *path);

Bool Util_MakeSureDirExistsAndAccessible(char const *path,
                                         unsigned int mode);

#if _WIN32
#   define DIRSEPS	      "\\"
#   define DIRSEPS_W	      L"\\"
#   define DIRSEPC	      '\\'
#   define DIRSEPC_W	      L'\\'
#   define VALID_DIRSEPS      "\\/"
#   define VALID_DIRSEPS_W    L"\\/"
#   define CUR_DIRS_W         L"."
#   define CUR_DIRC_W         L'.'
#else
#   define DIRSEPS	      "/"
#   define DIRSEPC	      '/'
#   define VALID_DIRSEPS      DIRSEPS
#endif

#define CURR_DIRS             "."
#define CURR_DIRC             '.'

/*
 *-----------------------------------------------------------------------
 *
 * Util_Safe[Malloc, Realloc, Calloc, Strdup] and
 * Util_Safe[Malloc, Realloc, Calloc, Strdup]Bug --
 *
 *      These functions work just like the standard C library functions
 *      (except Util_SafeStrdup[,Bug]() accept NULL, see below),
 *      but will not fail. Instead they Panic(), printing the file and
 *      line number of the caller, if the underlying library function
 *      fails.  The Util_SafeFnBug functions print bugNumber in the
 *      Panic() message.
 *
 *      These functions should only be used when there is no way to
 *      gracefully recover from the error condition.
 *
 *      The internal versions of these functions expect a bug number
 *      as the first argument.  If that bug number is something other
 *      than -1, the panic message will include the bug number.
 *
 *      Since Util_SafeStrdup[,Bug]() do not need to return NULL
 *      on error, they have been extended to accept the null pointer
 *      (and return it).  The competing view is that they should
 *      panic on NULL.  This is a convenience vs. strictness argument.
 *      Convenience wins.  -- edward
 *
 * Results:
 *      The freshly allocated memory.
 *
 * Side effects:
 *      Panic() if the library function fails.
 *
 *--------------------------------------------------------------------------
 */

void *UtilSafeMalloc0(size_t size);
void *UtilSafeMalloc1(size_t size,
                      int bugNumber, const char *file, int lineno);

void *UtilSafeRealloc0(void *ptr, size_t size);
void *UtilSafeRealloc1(void *ptr, size_t size,
                      int bugNumber, const char *file, int lineno);

void *UtilSafeCalloc0(size_t nmemb, size_t size);
void *UtilSafeCalloc1(size_t nmemb, size_t size,
                      int bugNumber, const char *file, int lineno);

char *UtilSafeStrdup0(const char *s);
char *UtilSafeStrdup1(const char *s,
                      int bugNumber, const char *file, int lineno);

char *UtilSafeStrndup0(const char *s, size_t n);
char *UtilSafeStrndup1(const char *s, size_t n,
                      int bugNumber, const char *file, int lineno);

/*
 * Debug builds carry extra arguments into the allocation functions for
 * better error reporting. Non-debug builds don't pay this extra overhead.
 */
#ifdef VMX86_DEBUG

#define Util_SafeMalloc(_size) \
   UtilSafeMalloc1((_size), -1, __FILE__, __LINE__)

#define Util_SafeMallocBug(_bugNr, _size) \
   UtilSafeMalloc1((_size),(_bugNr), __FILE__, __LINE__)

#define Util_SafeRealloc(_ptr, _size) \
   UtilSafeRealloc1((_ptr), (_size), -1, __FILE__, __LINE__)

#define Util_SafeReallocBug(_bugNr, _ptr, _size) \
   UtilSafeRealloc1((_ptr), (_size), (_bugNr), __FILE__, __LINE__)

#define Util_SafeCalloc(_nmemb, _size) \
   UtilSafeCalloc1((_nmemb), (_size), -1, __FILE__, __LINE__)

#define Util_SafeCallocBug(_bugNr, _nmemb, _size) \
   UtilSafeCalloc1((_nmemb), (_size), (_bugNr), __FILE__, __LINE__)

#define Util_SafeStrndup(_str, _size) \
   UtilSafeStrndup1((_str), (_size), -1, __FILE__, __LINE__)

#define Util_SafeStrndupBug(_bugNr, _str, _size) \
   UtilSafeStrndup1((_str), (_size), (_bugNr), __FILE__, __LINE__)

#define Util_SafeStrdup(_str) \
   UtilSafeStrdup1((_str), -1, __FILE__, __LINE__)

#define Util_SafeStrdupBug(_bugNr, _str) \
   UtilSafeStrdup1((_str), (_bugNr), __FILE__, __LINE__)

#else  /* VMX86_DEBUG */

#define Util_SafeMalloc(_size) \
   UtilSafeMalloc0((_size))

#define Util_SafeMallocBug(_bugNr, _size) \
   UtilSafeMalloc0((_size))

#define Util_SafeRealloc(_ptr, _size) \
   UtilSafeRealloc0((_ptr), (_size))

#define Util_SafeReallocBug(_ptr, _size) \
   UtilSafeRealloc0((_ptr), (_size))

#define Util_SafeCalloc(_nmemb, _size) \
   UtilSafeCalloc0((_nmemb), (_size))

#define Util_SafeCallocBug(_bugNr, _nmemb, _size) \
   UtilSafeCalloc0((_nmemb), (_size))

#define Util_SafeStrndup(_str, _size) \
   UtilSafeStrndup0((_str), (_size))

#define Util_SafeStrndupBug(_bugNr, _str, _size) \
   UtilSafeStrndup0((_str), (_size))

#define Util_SafeStrdup(_str) \
   UtilSafeStrdup0((_str))

#define Util_SafeStrdupBug(_bugNr, _str) \
   UtilSafeStrdup0((_str))

#endif  /* VMX86_DEBUG */


void *Util_Memdup(const void *src, size_t size);
void *Util_Memcpy(void *dest, const void *src, size_t count);
void  Util_Memfree(void *ptr);

Bool Util_ConstTimeMemDiff(const void *secret, const void *guess, size_t len);
Bool Util_ConstTimeStrDiff(const char *secret, const char *guess);


#ifndef VMKBOOT
/*
 *-----------------------------------------------------------------------------
 *
 * Util_FreeList --
 * Util_FreeStringList --
 *
 *      Free a list (actually a vector) of allocated objects.
 *      The list (vector) itself is also freed.
 *
 *      The list either has a specified length or is
 *      argv-style NULL terminated (if length is negative).
 *
 *      The list can be NULL, in which case no operation is performed.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      errno or Windows last error is preserved.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Util_FreeList(void **list,      // IN/OUT/OPT: the list to free
              ssize_t length)   // IN: the length
{
   // See Posix_Free.
   int err;

   if (list == NULL) {
      ASSERT(length <= 0);
      return;
   }

   err = errno;

   if (length >= 0) {
      ssize_t i;

      for (i = 0; i < length; i++) {
         free(list[i]);
         DEBUG_ONLY(list[i] = NULL);
      }
   } else {
      void **s;

      for (s = list; *s != NULL; s++) {
         free(*s);
         DEBUG_ONLY(*s = NULL);
      }
   }
   free(list);
   errno = err;
}

static INLINE void
Util_FreeStringList(char **list,      // IN/OUT/OPT: the list to free
                    ssize_t length)   // IN: the length
{
   Util_FreeList((void **) list, length);
}
#endif /* VMKBOOT */


/*
 *-----------------------------------------------------------------------------
 *
 * Util_Memcpy32 --
 *
 *      Special purpose version of memcpy that requires nbytes be a
 *      multiple of 4.  This assumption lets us have a very small,
 *      inlineable implementation.
 *
 * Results:
 *      dst
 *
 * Side effects:
 *      See above.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void *
Util_Memcpy32(void *dst,
              const void *src,
              size_t nbytes)
{
   ASSERT((nbytes % 4) == 0);
#if defined __GNUC__ && (defined(__i386__) || defined(__x86_64__))
   do {
      int dummy0, dummy1, dummy2;
      __asm__ __volatile__(
           "cld \n\t"
           "rep ; movsl"  "\n\t"
        : "=&c" (dummy0), "=&D" (dummy1), "=&S" (dummy2)
        : "0" (nbytes / 4), "1" ((long) dst), "2" ((long) src)
        : "memory", "cc"
      );
      return dst;
   } while (0);
#else
   return memcpy(dst, src, nbytes);
#endif
}

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* UTIL_H */
