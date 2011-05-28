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
#include <stdlib.h>

#ifdef _WIN32
   #ifdef USERLEVEL
      #include <tchar.h>   /* Needed for MBCS string functions */
      #include <windows.h> /* for definition of HANDLE */
   #endif
#else
   #include <sys/types.h>
#endif

#include "vm_assert.h"
#include "unicodeTypes.h"


/*
 * Define the Util_ThreadID type.
 */
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <pthread.h>
typedef pthread_t Util_ThreadID;
#elif defined(_WIN32)
typedef DWORD Util_ThreadID;
#else // Linux et al
#include <unistd.h>
typedef pid_t Util_ThreadID;
#endif


EXTERN uint32 CRC_Compute(const uint8 *buf, int len);
EXTERN uint32 Util_Checksum32(const uint32 *buf, int len);
EXTERN uint32 Util_Checksum(const uint8 *buf, int len);
EXTERN uint32 Util_Checksumv(void *iov, int numEntries);
EXTERN uint32 Util_HashString(const char *str);
EXTERN Unicode Util_ExpandString(ConstUnicode fileName);
EXTERN void Util_ExitThread(int);
EXTERN NORETURN void Util_ExitProcessAbruptly(int);
EXTERN int Util_HasAdminPriv(void);
#if defined _WIN32 && defined USERLEVEL
EXTERN int Util_TokenHasAdminPriv(HANDLE token);
EXTERN int Util_TokenHasInteractPriv(HANDLE token);
#endif
EXTERN Bool Util_Data2Buffer(char *buf, size_t bufSize, const void *data0,
                             size_t dataSize);
EXTERN char *Util_GetCanonicalPath(const char *path);
#ifdef _WIN32
EXTERN char *Util_CompatGetCanonicalPath(const char *path);
EXTERN char *Util_GetCanonicalPathForHash(const char *path);
EXTERN char *Util_CompatGetLowerCaseCanonicalPath(const char* path);
#endif
EXTERN int Util_BumpNoFds(uint32 *cur, uint32 *wanted);
EXTERN Bool Util_CanonicalPathsIdentical(const char *path1, const char *path2);
EXTERN Bool Util_IsAbsolutePath(const char *path);
EXTERN unsigned Util_GetPrime(unsigned n0);
EXTERN Util_ThreadID Util_GetCurrentThreadId(void);

EXTERN char *Util_DeriveFileName(const char *source,
                                 const char *name,
                                 const char *ext);

EXTERN char *Util_CombineStrings(char **sources, int count);
EXTERN char **Util_SeparateStrings(char *source, int *count);

typedef struct UtilSingleUseResource UtilSingleUseResource;
UtilSingleUseResource *Util_SingleUseAcquire(const char *name);
void Util_SingleUseRelease(UtilSingleUseResource *res);


#if defined(__linux__) || defined(__FreeBSD__) || defined(sun)
EXTERN Bool Util_GetProcessName(pid_t pid, char *bufOut, size_t bufOutSize);
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

int Util_CompareDotted(const char *s1, const char *s2);

/*
 * This enum defines how Util_GetOpt should handle non-option arguments:
 *
 * UTIL_NONOPT_PERMUTE: Permute argv so that all non-options are at the end.
 * UTIL_NONOPT_STOP:    Stop when first non-option argument is seen.
 * UTIL_NONOPT_ALL:     Return each non-option argument as if it were
 *                      an option with character code 1.
 */
typedef enum { UTIL_NONOPT_PERMUTE, UTIL_NONOPT_STOP, UTIL_NONOPT_ALL } Util_NonOptMode;
struct option;
int Util_GetOpt(int argc, char * const *argv, const struct option *opts,
                Util_NonOptMode mode);


#if defined(VMX86_STATS)
Bool Util_QueryCStResidency(uint32 *numCpus, uint32 *numCStates,
                            uint64 **transitns, uint64 **residency,
                            uint64 **transTime, uint64 **residTime);
#endif

/*
 * In util_shared.h
 */
EXTERN Bool Util_Throttle(uint32 count);
EXTERN uint32 Util_FastRand(uint32 seed);


/*
 *-----------------------------------------------------------------------------
 *
 * Util_ValidateBytes --
 *
 *      Check that memory is filled with the specified value.
 *
 * Results:
 *      NULL   No error
 *      !NULL  First address that doesn't have the proper value
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void *
Util_ValidateBytes(const void *ptr,  // IN: ptr to check
                   size_t size,      // IN: size of ptr
                   uint8 byteValue)  // IN: memory must be filled with this
{
   uint8 *p;
   uint8 *end;
   uint64 bigValue;

   ASSERT(ptr);

   if (size == 0) {
      return NULL;
   }

   p = (uint8 *) ptr;
   end = p + size;

   /* Compare bytes until a "nice" boundary is achieved. */
   while ((uintptr_t) p % sizeof bigValue) {
      if (*p != byteValue) {
         return p;
      }

      p++;

      if (p == end) {
         return NULL;
      }
   }

   /* Compare using a "nice sized" chunk for a long as possible. */
   memset(&bigValue, (int) byteValue, sizeof bigValue);

   while (p + sizeof bigValue <= end) {
      if (*((uint64 *) p) != bigValue) {
         /* That's not right... let the loop below report the exact address. */
         break;
      }

      size -= sizeof bigValue;
      p += sizeof bigValue;
   }

   /* Handle any trailing bytes. */
   while (p < end) {
      if (*p != byteValue) {
         return p;
      }

      p++;
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_BufferIsEmpty --
 *
 *    Determine if the specified buffer of 'len' bytes starting at 'base'
 *    is empty (i.e. full of zeroes).
 *
 * Results:
 *    TRUE  Yes
 *    FALSE No
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Util_BufferIsEmpty(void const *base,  // IN:
                   size_t len)        // IN:
{
   return Util_ValidateBytes(base, len, '\0') == NULL;
}


EXTERN Bool Util_MakeSureDirExistsAndAccessible(char const *path,
                                                unsigned int mode);

#if _WIN32
#   define DIRSEPS	      "\\"
#   define DIRSEPS_W	      L"\\"
#   define DIRSEPC	      '\\'
#   define DIRSEPC_W	      L'\\'
#   define VALID_DIRSEPS      "\\/"
#   define VALID_DIRSEPS_W    L"\\/"
#else
#   define DIRSEPS	      "/"
#   define DIRSEPC	      '/'
#   define VALID_DIRSEPS      DIRSEPS
#endif


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

EXTERN void *Util_SafeInternalMalloc(int bugNumber, size_t size,
                                     const char *file, int lineno);

EXTERN void *Util_SafeInternalRealloc(int bugNumber, void *ptr, size_t size,
                                      const char *file, int lineno);

EXTERN void *Util_SafeInternalCalloc(int bugNumber, size_t nmemb,
                                     size_t size, const char *file, int lineno);

EXTERN char *Util_SafeInternalStrdup(int bugNumber, const char *s,
                                     const char *file, int lineno);

EXTERN char *Util_SafeInternalStrndup(int bugNumber, const char *s, size_t n,
                                      const char *file, int lineno);

#define Util_SafeMalloc(_size) \
   Util_SafeInternalMalloc(-1, (_size), __FILE__, __LINE__)

#define Util_SafeMallocBug(_bugNr, _size) \
   Util_SafeInternalMalloc((_bugNr), (_size), __FILE__, __LINE__)

#define Util_SafeRealloc(_ptr, _size) \
   Util_SafeInternalRealloc(-1, (_ptr), (_size), __FILE__, __LINE__)

#define Util_SafeReallocBug(_bugNr, _ptr, _size) \
   Util_SafeInternalRealloc((_bugNr), (_ptr), (_size), __FILE__, __LINE__)

#define Util_SafeCalloc(_nmemb, _size) \
   Util_SafeInternalCalloc(-1, (_nmemb), (_size), __FILE__, __LINE__)

#define Util_SafeCallocBug(_bugNr, _nmemb, _size) \
   Util_SafeInternalCalloc((_bugNr), (_nmemb), (_size), __FILE__, __LINE__)

#define Util_SafeStrndup(_str, _size) \
   Util_SafeInternalStrndup(-1, (_str), (_size), __FILE__, __LINE__)

#define Util_SafeStrndupBug(_bugNr, _str, _size) \
   Util_SafeInternalStrndup((_bugNr), (_str), (_size), __FILE__, __LINE__)

#define Util_SafeStrdup(_str) \
   Util_SafeInternalStrdup(-1, (_str), __FILE__, __LINE__)

#define Util_SafeStrdupBug(_bugNr, _str) \
   Util_SafeInternalStrdup((_bugNr), (_str), __FILE__, __LINE__)


/*
 *-----------------------------------------------------------------------------
 *
 * Util_Zero --
 *
 *      Zeros out bufSize bytes of buf. NULL is legal.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      See above.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Util_Zero(void *buf,       // OUT
          size_t bufSize)  // IN
{
   if (buf != NULL) {
#if defined _WIN32 && defined USERLEVEL
      /*
       * Simple memset calls might be optimized out.  See CERT advisory
       * MSC06-C.
       */
      SecureZeroMemory(buf, bufSize);
#else
      memset(buf, 0, bufSize);
#endif
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_ZeroString --
 *
 *      Zeros out a NULL-terminated string. NULL is legal.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      See above.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Util_ZeroString(char *str)  // IN/OUT
{
   if (str != NULL) {
      Util_Zero(str, strlen(str));
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_ZeroFree --
 *
 *      Zeros out bufSize bytes of buf, and then frees it. NULL is
 *      legal.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	buf is zeroed, and then free() is called on it.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Util_ZeroFree(void *buf,       // OUT
              size_t bufSize)  // IN
{
   if (buf != NULL) {
      Util_Zero(buf, bufSize);
      free(buf);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_ZeroFreeString --
 *
 *      Zeros out a NULL-terminated string, and then frees it. NULL is
 *      legal.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	str is zeroed, and then free() is called on it.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Util_ZeroFreeString(char *str)  // IN
{
   if (str != NULL) {
      Util_ZeroString(str);
      free(str);
   }
}


#ifdef _WIN32
/*
 *-----------------------------------------------------------------------------
 *
 * Util_ZeroFreeStringW --
 *
 *      Zeros out a NUL-terminated wide-character string, and then frees it.
 *      NULL is legal.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	str is zeroed, and then free() is called on it.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Util_ZeroFreeStringW(wchar_t *str)  // IN
{
   if (str != NULL) {
      Util_Zero(str, wcslen(str) * sizeof *str);
      free(str);
   }
}
#endif // _WIN32


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
Util_FreeList(void **list,      // IN/OUT: the list to free
              ssize_t length)   // IN: the length
{
   if (list == NULL) {
      ASSERT(length <= 0);
      return;
   }

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
}

static INLINE void
Util_FreeStringList(char **list,      // IN/OUT: the list to free
                    ssize_t length)   // IN: the length
{
   Util_FreeList((void **) list, length);
}
#endif /* UTIL_H */
