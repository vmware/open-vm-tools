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

#ifdef __APPLE__
   #include <IOKit/IOTypes.h>
   #include <CoreFoundation/CFNumber.h>
   #include <CoreFoundation/CFDictionary.h>
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

#ifdef __APPLE__
EXTERN char *Util_CFStringToUTF8CString(CFStringRef s);
EXTERN char *Util_IORegGetStringProperty(io_object_t entry, CFStringRef property);
EXTERN Bool Util_IORegGetNumberProperty(io_object_t entry, CFStringRef property,
                                        CFNumberType type, void *val);
EXTERN Bool Util_IORegGetBooleanProperty(io_object_t entry, CFStringRef property,
                                         Bool *boolVal);
EXTERN CFMutableDictionaryRef UtilMacos_CreateCFDictionary(
   unsigned int numPairs, ...);
EXTERN io_service_t Util_IORegGetDeviceObjectByName(const char *deviceName);
EXTERN char *Util_GetBSDName(const char *deviceName);
EXTERN char *Util_IORegGetDriveType(const char *deviceName);
EXTERN char *Util_GetMacOSDefaultVMPath();
#endif // __APPLE__


EXTERN uint32 CRC_Compute(uint8 *buf, int len);
EXTERN uint32 Util_Checksum32(uint32 *buf, int len);
EXTERN uint32 Util_Checksum(uint8 *buf, int len);
EXTERN uint32 Util_Checksumv(void *iov, int numEntries);
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

EXTERN char *Util_GetSafeTmpDir(Bool useConf);

EXTERN int Util_MakeSafeTemp(ConstUnicode tag,
                             Unicode *presult);

#if defined(__linux__) || defined(__FreeBSD__) || defined(sun)
EXTERN Bool Util_GetProcessName(pid_t pid, char *bufOut, size_t bufOutSize);
#endif

// backtrace functions and utilities

#define UTIL_BACKTRACE_LINE_LEN (255)
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

void Util_LogWrapper(void *ignored, const char *fmt, ...);

int Util_CompareDotted(const char *s1, const char *s2);

#if defined(__linux__)
void Util_PrintLoadedObjects(void *addr_inside_exec);
#endif

/*
 * In util_shared.h
 */
EXTERN Bool Util_Throttle(uint32 count);

/*
 *----------------------------------------------------------------------
 *
 * Util_BufferIsEmpty --
 *
 *    Determine wether or not the buffer of 'len' bytes starting at 'base' is
 *    empty (i.e. full of zeroes)
 *
 * Results:
 *    TRUE if yes
 *    FALSE if no
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool Util_BufferIsEmpty(void const *base, // IN
                                      size_t len)       // IN
{
   uint32 const *p32;
   uint32 const *e32;
   uint16 const *p16;

   ASSERT_ON_COMPILE(sizeof(uint32) == 4);

   p32 = (uint32 const *)base;
   e32 = p32 + len / 4;
   for (; p32 < e32; p32++) {
      if (*p32) {
         return FALSE;
      }
   }

   len &= 0x3;
   p16 = (uint16 const *)p32;

   if (len & 0x2) {
      if (*p16) {
         return FALSE;
      }

      p16++;
   }

   if (   len & 0x1
       && *(uint8 const *)p16) {
      return FALSE;
   }

   return TRUE;
};


EXTERN Bool Util_MakeSureDirExistsAndAccessible(char const *path,
						unsigned int mode);

#ifdef N_PLAT_NLM
#   define DIRSEPS	      "\\"
#   define DIRSEPC	      '\\'
#   define VALID_DIRSEPS      "\\/:"
#elif _WIN32
#   define DIRSEPS	      "\\"
#   define DIRSEPS_W	      L"\\"
#   define DIRSEPC	      '\\'
#   define DIRSEPC_W	      L'\\'
#   define VALID_DIRSEPS      "\\/"
#   define VALID_DIRSEPS_W    L"\\/"
#else
#   define DIRSEPS	      "/"
#   define DIRSEPC	      '/'
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

#define Util_SafeMalloc(_size) \
   UtilSafeMallocInternal(-1, (_size), __FILE__, __LINE__)

#define Util_SafeMallocBug(_bugNr, _size) \
   UtilSafeMallocInternal((_bugNr), (_size), __FILE__, __LINE__)

#define Util_SafeRealloc(_ptr, _size) \
   UtilSafeReallocInternal(-1, (_ptr), (_size), __FILE__, __LINE__)

#define Util_SafeReallocBug(_bugNr, _ptr, _size) \
   UtilSafeReallocInternal((_bugNr), (_ptr), (_size), __FILE__, __LINE__)

#define Util_SafeCalloc(_nmemb, _size) \
   UtilSafeCallocInternal(-1, (_nmemb), (_size), __FILE__, __LINE__)

#define Util_SafeCallocBug(_bugNr, _nmemb, _size) \
   UtilSafeCallocInternal((_bugNr), (_nmemb), (_size), __FILE__, __LINE__)

#define Util_SafeStrndup(_str, _size) \
   UtilSafeStrndupInternal(-1, (_str), (_size), __FILE__, __LINE__)

#define Util_SafeStrndupBug(_bugNr, _str, _size) \
   UtilSafeStrndupInternal((_bugNr), (_str), (_size), __FILE__, __LINE__)

#define Util_SafeStrdup(_str) \
   UtilSafeStrdupInternal(-1, (_str), __FILE__, __LINE__)

#define Util_SafeStrdupBug(_bugNr, _str) \
   UtilSafeStrdupInternal((_bugNr), (_str), __FILE__, __LINE__)

static INLINE void *
UtilSafeMallocInternal(int bugNumber, size_t size, char *file, int lineno)
{
   void *result = malloc(size);

   if (result == NULL && size != 0) {
      if (bugNumber == -1) {
         Panic("Unrecoverable memory allocation failure at %s:%d\n",
               file, lineno);
      } else {
         Panic("Unrecoverable memory allocation failure at %s:%d.  Bug "
               "number: %d\n", file, lineno, bugNumber);
      }
   }
   return result;
}

static INLINE void *
UtilSafeReallocInternal(int bugNumber, void *ptr, size_t size,
                        char *file, int lineno)
{
   void *result = realloc(ptr, size);

   if (result == NULL && size != 0) {
      if (bugNumber == -1) {
         Panic("Unrecoverable memory allocation failure at %s:%d\n",
               file, lineno);
      } else {
         Panic("Unrecoverable memory allocation failure at %s:%d.  Bug "
               "number: %d\n", file, lineno, bugNumber);
      }
   }
   return result;
}

static INLINE void *
UtilSafeCallocInternal(int bugNumber, size_t nmemb, size_t size,
                       char *file, int lineno)
{
   void *result = calloc(nmemb, size);

   if (result == NULL && nmemb != 0 && size != 0) {
      if (bugNumber == -1) {
         Panic("Unrecoverable memory allocation failure at %s:%d\n",
               file, lineno);
      } else {
         Panic("Unrecoverable memory allocation failure at %s:%d.  Bug "
               "number: %d\n", file, lineno, bugNumber);
      }
   }
   return result;
}

#ifdef VMX86_SERVER
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ > 1)
// XXX Prevents an "inlining failed" warning in vmkproxy.c.  Ugh.
static INLINE char *
UtilSafeStrdupInternal(int bugNumber, const char *s, char *file,
                       int lineno) __attribute__((always_inline));
#endif
#endif

static INLINE char *
UtilSafeStrdupInternal(int bugNumber, const char *s, char *file,
                       int lineno)
{
   char *result;

   if (s == NULL) {
      return NULL;
   }
#ifdef _WIN32
   if ((result = _strdup(s)) == NULL) {
#else
   if ((result = strdup(s)) == NULL) {
#endif
      if (bugNumber == -1) {
         Panic("Unrecoverable memory allocation failure at %s:%d\n",
               file, lineno);
      } else {
         Panic("Unrecoverable memory allocation failure at %s:%d.  Bug "
               "number: %d\n", file, lineno, bugNumber);
      }
   }
   return result;
}

/*
 *-----------------------------------------------------------------------------
 *
 * UtilSafeStrndupInternal --
 *
 *      Returns a string consisting of first n characters of 's' if 's' has
 *      length >= 'n', otherwise returns a string duplicate of 's'.
 *
 * Results:
 *      Pointer to the duplicated string.
 *
 * Side effects:
 *      May Panic if ran out of memory.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE char *
UtilSafeStrndupInternal(int bugNumber,    // IN
                        const char *s,    // IN
                        size_t n,         // IN
			char *file,       // IN
                        int lineno)       // IN
{
   size_t size;
   char *copy;
   const char *null;

   if (s == NULL) {
      return NULL;
   }

   null = (char *)memchr(s, '\0', n);
   size = null ? null - s: n;
   copy = (char *)malloc(size + 1);

   if (copy == NULL) {
      if (bugNumber == -1) {
         Panic("Unrecoverable memory allocation failure at %s:%d\n",
               file, lineno);
      } else {
         Panic("Unrecoverable memory allocation failure at %s:%d.  Bug "
               "number: %d\n", file, lineno, bugNumber);
      }
   }

   copy[size] = '\0';
   return (char *)memcpy(copy, s, size);
}


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
      memset(buf, 0, bufSize);
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
Util_ZeroString(char *str)  // IN
{
   if (str != NULL) {
      memset(str, 0, strlen(str));
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
      memset(buf, 0, bufSize);
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
      Util_Zero(str, strlen(str));
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
      Util_Zero(str, wcslen(str) * sizeof(wchar_t));
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
