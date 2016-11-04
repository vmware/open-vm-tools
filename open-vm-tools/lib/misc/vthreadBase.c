/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * vthreadBase.c --
 *
 *      Base thread management functionality.  Does not care whether threads
 *      are used or not.
 *
 *      For full thread management (e.g. creation/destruction), see lib/thread.
 *
 *      Major exposed functions and their properties:
 *         VThreadBase_CurName - Returns a thread name.  Will try to assign
 *                               a default name if none exists, but if called
 *                               reentrantly (e.g. due to ASSERT) will supply
 *                               a failsafe name instead.
 *         VThreadBase_CurID - Always populates a VThreadID; will Panic if
 *                             this fails (extremely unlikely).
 *         VThreadBase_SetName - Sets current thread name.  Assigns a VThreadID
 *                               if one is not already present.
 *
 *      Functions useful for implementing a full thread library:
 *         VThreadBase_InitWithTLS - Sets up thread with a specific VThreadID
 *                                   and name.  Client supplies TLS store.
 *         VThreadBase_SetNoIDFunc - The NoID function is called whenever an
 *                                   unknown thread is seen; it must call
 *                                   VThreadBase_InitWithTLS to assign a
 *                                   VThreadID.  Note that this function
 *                                   runs with all signals masked!
 *         VThreadBase_ForgetSelf - Clears the VThreadID for current thread,
 *                                  to clean up resource usage prior to thread
 *                                  exit.
 *      Historical quirks:
 *      * Default thread numbering starts at VTHREAD_MAX_VCPUs + 2
 *        to allow VThread_IsVCPU() to run efficiently.
 *      * Most other code uses VThread_Foo instead of VThreadBase_Foo; the
 *        public header file uses inlines to convert names.
 *
 *      VThreadBase is self-initializing; by default, threads will be given
 *      names like "vthread-1", "vthread-32", etc.  Use VThread_SetName to
 *      provide more meaningful names (often, this is the only initialization
 *      needed).
 *
 *      The default implementation supports an (effectively) unlimited number
 *      of threads, and OS-specific primitives may be used to start the
 *      threads.  If lib/thread is used on top of this library, the lib/thread
 *      NoID function may introduce a smaller limit.
 *
 *      On Linux we make use of a combination of __thread and pthread support
 *         (pthread being used to get a cleanup destructor).
 *      On Mac our only option is to use pthreads.
 *      On Windows we could use the compiler supported thread locals but
 *      that has issues when dynamically loaded from a library, so instead
 *      we use the safer Tls* functions.
 */

#if defined __linux__
#  define HAVE_TLS
#endif

#if defined _WIN32
#  include <windows.h>
#else
#  if defined(sun) && !defined(_XOPEN_SOURCE)
     /*
      * Solaris headers don't define constants we need unless
      * the Posix standard is somewhat modern.  Most of our builds
      * set this; we should chase down the oversight.
      */
#    define _XOPEN_SOURCE 500
#  endif
#  include <pthread.h>
#  include <signal.h>
#  include <errno.h>
#  include <limits.h>
#endif
#include <stdlib.h>
#include <stdio.h>   /* snprintf */

#include "vmware.h"
#include "vm_atomic.h"
#include "vthreadBase.h"
#include "str.h"
#include "util.h"
#include "hashTable.h"
#include "hostinfo.h"


/*
 * Table of thread types
 * =====================
 * OS          Thread type       TLS key type     Max TLS keys
 * -----------------------------------------------------------------
 * Windows     HANDLE / void *   DWORD            0xFFFFFFFF
 * (Posix)     (pthread_t)       (pthread_key_t)  (PTHREAD_KEYS_MAX)
 * Linux       unsigned long     unsigned int     1024
 * OS X 10.5   struct _opaque *  unsigned long    512
 * Solaris 10  unsigned int      unsigned int     128
 * FreeBSD 8   struct pthread *  int              256
 */
#if defined _WIN32
typedef DWORD VThreadBaseKeyType;
#define VTHREADBASE_INVALID_KEY (VThreadBaseKeyType)(TLS_OUT_OF_INDEXES)
#else
typedef pthread_key_t VThreadBaseKeyType;
/* PTHREAD_KEYS_MAX not defined on Android. */
#if defined __linux__ && !defined PTHREAD_KEYS_MAX
#define VTHREADBASE_INVALID_KEY (VThreadBaseKeyType)(1024)
#else
#define VTHREADBASE_INVALID_KEY (VThreadBaseKeyType)(PTHREAD_KEYS_MAX)
#endif
#endif

#ifdef HAVE_TLS
static __thread VThreadBaseData *tlsBaseCache = NULL;
static __thread VThreadID tlsIDCache = VTHREAD_INVALID_ID;
#endif

static void VThreadBaseInit(void);
static void VThreadBaseSimpleNoID(void);
static void VThreadBaseSimpleFreeID(void *tlsData);
static void VThreadBaseSafeDeleteTLS(void *data);

static struct {
   Atomic_Int   baseKey;
   Atomic_Int   threadIDKey;
   Atomic_Int   dynamicID;
   Atomic_Int   numThreads;
   Atomic_Ptr   nativeHash;
   void       (*noIDFunc)(void);
   void       (*freeIDFunc)(void *);
} vthreadBaseGlobals = {
   { VTHREADBASE_INVALID_KEY },
   { VTHREADBASE_INVALID_KEY },
   { VTHREAD_ALLOCSTART_ID },
   { 0 },
   { 0 },
   VThreadBaseSimpleNoID,
   VThreadBaseSimpleFreeID,
};

typedef enum VThreadLocal {
   VTHREAD_LOCAL_BASE,
   VTHREAD_LOCAL_ID
} VThreadLocal;


/*
 * Code to mask all asynchronous signals
 *
 * There are some stretches where an async signal will cause reentrancy,
 * and that breaks allocating a VThreadID and/or setting the TLS slot
 * atomically.  So mask all asynchronous signals; synchronous signals
 * (which are generally fatal) are still OK.
 *
 * Though these functions can return errors; it is not worthwhile to check
 * them because the inputs are hard-coded.  The only errors allowed
 * are EINVAL for invalid argument (and they are all hardcoded valid) and
 * EFAULT for the addresses (and a stack pointer will not EFAULT).  Besides,
 * these routines are used in code paths that cannot Panic without creating
 * a Panic-loop.
 */
#if defined _WIN32
#define NO_ASYNC_SIGNALS_START  do {
#define NO_ASYNC_SIGNALS_END    } while (0)
#else
#define NO_ASYNC_SIGNALS_START                         \
    do {                                               \
      sigset_t setMask, oldMask;                       \
      sigfillset(&setMask);                            \
      sigdelset(&setMask, SIGBUS);                     \
      sigdelset(&setMask, SIGSEGV);                    \
      sigdelset(&setMask, SIGILL);                     \
      sigdelset(&setMask, SIGABRT);                    \
      pthread_sigmask(SIG_BLOCK, &setMask, &oldMask);
#define NO_ASYNC_SIGNALS_END                           \
      pthread_sigmask(SIG_SETMASK, &oldMask, NULL);    \
   } while(0)
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBaseInitKeysWork --
 *
 *      Helper function to be only used by VThreadBaseInitKeys.
 *
 *-----------------------------------------------------------------------------
 */

static void
VThreadBaseInitKeyWork(Atomic_Int *key, void (*deletePtr)(void*))
{
   if (Atomic_Read(key) == VTHREADBASE_INVALID_KEY) {
      VThreadBaseKeyType newKey;

#if defined _WIN32
      newKey = TlsAlloc();
      VERIFY(newKey != VTHREADBASE_INVALID_KEY);
#else
      Bool success = pthread_key_create(&newKey, deletePtr) == 0;
      if (success && newKey == 0) {
         /*
          * Leak TLS key 0.  System libraries have a habit of destroying
          * it.  See bugs 702818 and 773420.
          */
         success = pthread_key_create(&newKey, deletePtr) == 0;
      }
      VERIFY(success);
#endif

      if (Atomic_ReadIfEqualWrite(key, VTHREADBASE_INVALID_KEY, newKey) !=
          VTHREADBASE_INVALID_KEY) {
         /* Race: someone else init'd */
#if defined _WIN32
         TlsFree(newKey);
#else
         pthread_key_delete(newKey);
#endif
      }
   }
}

/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBaseInitKeys --
 *
 *      Initialize the host-specific TLS slots.
 *
 *      Failure to allocate a TLS slot is immediately fatal.  Note that
 *      a TLS slot is generally allocated at the first of:
 *      - VThread_Init() (e.g. uses lib/thread)
 *      - VThread_SetName()
 *      - a Posix signal
 *      - a lock acquisition
 *      Since most Panic paths do look up a thread name (and thus need a TLS
 *      slot), a program that does not want to Panic-loop should call
 *      one of the above functions very early to "prime" the TLS slot.
 *
 * Results:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VThreadBaseInitKeys(void)
{
   VThreadBaseInitKeyWork(&vthreadBaseGlobals.baseKey,
                          &VThreadBaseSafeDeleteTLS);
   VThreadBaseInitKeyWork(&vthreadBaseGlobals.threadIDKey, NULL);
}

#ifdef VMX86_DEBUG
static INLINE Bool
VThreadBaseAreKeysInited(void)
{
   return Atomic_Read(&vthreadBaseGlobals.baseKey) != VTHREADBASE_INVALID_KEY &&
      Atomic_Read(&vthreadBaseGlobals.threadIDKey) != VTHREADBASE_INVALID_KEY;
}
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBaseSetLocal --
 *
 *      Sets the specified thread local variable to the supplied
 *      value.  This abstracts between pthread, the Windows TLS
 *      supports, and the use of __thread.  Requires that the TLS keys
 *      are initialized before calling this function.  We encourage
 *      the compiler to inline this with the expectation that "local"
 *      is a compile time constant.
 *
 *      To facilitate lazy initialization we special case
 *      VTHREAD_LOCAL_ID slightly and arrange so that if its value is
 *      read before it is initialized we return -1 instead of 0 (since
 *      0 is used as a real thread id).
 *
 *      Note that we use store the thread local value using pthreads
 *      even when HAVE_TLS is defined.  This way we continue to use
 *      the same pthread destructor path for cleanup as we would
 *      without HAVE_TLS.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
VThreadBaseSetLocal(VThreadLocal local, void *value)
{
   VThreadBaseKeyType key;
   void *adjustedValue = value;
   Bool success;
   ASSERT(VThreadBaseAreKeysInited());
   if (local == VTHREAD_LOCAL_BASE) {
      key = Atomic_Read(&vthreadBaseGlobals.baseKey);
   } else {
      ASSERT(local == VTHREAD_LOCAL_ID);
      /* VThreadGetLocal compensates for this, lets the default be -1.  */
      adjustedValue = (void*)((uintptr_t)adjustedValue + 1);
      key = Atomic_Read(&vthreadBaseGlobals.threadIDKey);
   }
   ASSERT(key != VTHREADBASE_INVALID_KEY);
#if defined _WIN32
   success = TlsSetValue(key, adjustedValue);
#else
   success = pthread_setspecific(key, adjustedValue) == 0;
#endif
#ifdef HAVE_TLS
   if (success) {
      if (local == VTHREAD_LOCAL_BASE) {
         tlsBaseCache = value;
      } else {
         ASSERT(local == VTHREAD_LOCAL_ID);
         tlsIDCache = (VThreadID)(uintptr_t)value;
      }
   }
#endif
   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBaseGetLocal --
 *
 *      Retrives the specified thread local variable.  This abstracts
 *      between pthread, the Windows TLS supports, and the use of
 *      __thread.  See VThreadSetLocal for more details.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void *
VThreadBaseGetLocal(VThreadLocal local) 
{
   void *result;
#ifdef HAVE_TLS
   if (local == VTHREAD_LOCAL_BASE) {
      result = tlsBaseCache;
   } else {
      ASSERT(local == VTHREAD_LOCAL_ID);
      result = (void*)(uintptr_t)tlsIDCache;
   }
#else 
   VThreadBaseKeyType key;
   Atomic_Int *keyPtr;

   keyPtr = local == VTHREAD_LOCAL_BASE ? &vthreadBaseGlobals.baseKey :
                                          &vthreadBaseGlobals.threadIDKey;
   key = Atomic_Read(keyPtr);
   if (UNLIKELY(key == VTHREADBASE_INVALID_KEY)) {
      VThreadBaseInitKeys();
      key = Atomic_Read(keyPtr);
   }      
   ASSERT(VThreadBaseAreKeysInited());

#if defined _WIN32
   result = TlsGetValue(key);
#else
   result = pthread_getspecific(key);
#endif
   if (local == VTHREAD_LOCAL_ID) {
      result = (void*)((uintptr_t)result - 1); /* See VThreadBaseSetLocal. */
   }
#endif

   return result;
}


static INLINE VThreadBaseData *
VThreadBaseGetBase(void)
{
   return (VThreadBaseData *)VThreadBaseGetLocal(VTHREAD_LOCAL_BASE);
}

static INLINE Bool
VThreadBaseSetBase(VThreadBaseData *base)
{
   return VThreadBaseSetLocal(VTHREAD_LOCAL_BASE, base);
}

static INLINE VThreadID
VThreadBaseGetID(void)
{
   return (VThreadID)(uintptr_t)VThreadBaseGetLocal(VTHREAD_LOCAL_ID);
}

static INLINE Bool
VThreadBaseSetID(VThreadID id)
{
   return VThreadBaseSetLocal(VTHREAD_LOCAL_ID, (void*)(uintptr_t)id);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBaseGetNativeHash --
 *
 *      Get the default hash table of native thread IDs.  This is used by
 *      the "simple" allocation function to enable re-using of VThreadIDs.
 *
 * Results:
 *      An atomic HashTable *.
 *
 * Side effects:
 *      Allocates the HashTable on first call.
 *
 *-----------------------------------------------------------------------------
 */

static HashTable *
VThreadBaseGetNativeHash(void)
{
   return HashTable_AllocOnce(&vthreadBaseGlobals.nativeHash,
                              128, HASH_INT_KEY | HASH_FLAG_ATOMIC, NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadGetAndInitBase --
 *
 *      Get the per-thread data, and assign if there is no data.
 *
 * Results:
 *      A VThreadBaseData *.  Will succeed or ASSERT.
 *
 * Side effects:
 *      Can assign a dynamic VThreadID to the current thread.
 *
 *-----------------------------------------------------------------------------
 */

static VThreadBaseData *
VThreadBaseGetAndInitBase(void)
{
   VThreadBaseData *base = VThreadBaseGetBase();

   ASSERT(vthreadBaseGlobals.noIDFunc != NULL);

   if (UNLIKELY(base == NULL)) {
      VThreadBaseInit();
      base = VThreadBaseGetBase();
   }
   ASSERT(base != NULL);

   return base;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBase_CurID --
 *
 *      Get the current thread ID.
 *
 * Results:
 *      A VThreadID.  Always succeeds.
 *
 * Side effects:
 *      May assign a dynamic VThreadID if this thread is not known.
 *
 *-----------------------------------------------------------------------------
 */

VThreadID
VThreadBase_CurID(void)
{
   VThreadID tid = VThreadBaseGetID();
   if (UNLIKELY(tid == VTHREAD_INVALID_ID)) {
      VThreadBaseInit();
      tid = VThreadBaseGetID();
   }
   ASSERT(tid != VTHREAD_INVALID_ID);
   ASSERT(tid == VThreadBaseGetAndInitBase()->id);
   return tid;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBase_CurName --
 *
 *      Get the current thread name.
 *
 *      This function always returns, at some level of reentrancy.  That is,
 *      the first call either returns successfully or Panics; the Panic may
 *      reentrantly call this function, and that reentrant call always
 *      returns.
 *
 * Results:
 *      The current thread name.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

const char *
VThreadBase_CurName(void)
{
   static Atomic_Int curNameRecursion;
   VThreadBaseData *base = VThreadBaseGetBase();

   if (LIKELY(base != NULL)) {
      return base->name;
   } else if (Atomic_Read(&curNameRecursion) == 0) {
      /* Unnamed thread, try to name it. */
      Atomic_Inc(&curNameRecursion);
      base = VThreadBaseGetAndInitBase(); /* Assigns name as side effect */
      Atomic_Dec(&curNameRecursion);

      return base->name;
   } else {
      /*
       * Unnamed thread, but naming it failed (recursed back to here).
       * The heuristic is not perfect (a second unnamed thread could
       * be looking for a name while the first thread names itself),
       * but getting here nonrecursively is unlikely and we cannot
       * do better about detection without thread-local storage, and
       * in the recursive case thread-local storage won't exist after
       * a failure to set up thread-local storage in the first place.
       *
       * This clause function should not ASSERT, Panic or call a Str_
       * function (that can ASSERT or Panic), as the Panic handler is
       * very likey to query the thread name and end up right back here.
       * Thus, use a static buffer for a partly-sane name and hope the
       * Panic handler dumps enough information to figure out what went
       * wrong.
       */

      static char name[48];
      uintptr_t hostTid;

#if defined(_WIN32)
      hostTid = GetCurrentThreadId();
#elif defined(__linux__)
      hostTid = pthread_self();
#elif defined(__APPLE__)
      hostTid = (uintptr_t)(void*)pthread_self();
#else
      hostTid = 0;
#endif
      snprintf(name, sizeof name - 1 /* keep buffer NUL-terminated */,
               "host-%"FMTPD"u", hostTid);

      return name;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBase_InitWithTLS --
 *
 *      (Atomic) VThreadBase initialization, using caller-managed memory.
 *      Gets the VThreadID and thread name from within that caller-managed
 *      memory, so both must be populated.
 *
 *      Always "succeeds" in initialization; the return value is useful
 *      as an indication of whether this was the first initialization.
 *
 *      Note: will NEVER overwrite an existing TLS allocation.  May return a
 *      different VThreadID than requested; this is logged and considered a bug,
 *      code that cares about the assigned VThreadID should be using lib/thread
 *      to manage threads, not native OS primitives.  (However, there is code
 *      like that today, so we cannot ASSERT.)
 *
 * Results:
 *      TRUE if this is the first initialization (e.g. TLS uses supplied
 *      pointer), FALSE otherwise.
 *
 * Side effects:
 *      Sets TLS if this is the first initialization.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VThreadBase_InitWithTLS(VThreadBaseData *base)  // IN: caller-managed storage
{
   Bool firstTime, success;

   VThreadBaseInitKeys(); /* Require key allocation before TLS read */
   ASSERT(base != NULL && base->id != VTHREAD_INVALID_ID);

   NO_ASYNC_SIGNALS_START;
   if (VThreadBaseGetBase() == NULL) {
      /*
       * Windows potentially has the async-signal problem mentioned
       * below due to APCs, but in practice it will not happen:
       * APCs only get serviced at a few points (Sleep or WaitFor calls),
       * and we obviously do not make those between TlsGetValue and
       * TlsSetValue.
       */
      /*
       * The code between the check of pthread_getspecific and the eventually
       * call to pthread_setspecific MUST run with async signals blocked, see
       * bugs 295686 & 477318.  Here, the problem is that we could
       * accidentally set the TLS slot twice (it's not atomic).
       */
      success = VThreadBaseSetBase(base) && VThreadBaseSetID(base->id);
      firstTime = TRUE;
   } else {
      success = TRUE;
      firstTime = FALSE;
   }
   NO_ASYNC_SIGNALS_END;
   /* Try not to ASSERT while signals are blocked */
   VERIFY(success);
   ASSERT(!firstTime || (base == VThreadBaseGetBase()));

   if (firstTime) {
      Atomic_Inc(&vthreadBaseGlobals.numThreads);
   } else {
      VThreadBaseData *realBase = VThreadBaseGetBase();

      /*
       * If this happens, it means:
       * 1) A thread was created w/o lib/thread but caller tried to initialize
       *    it with a specific VThreadID.  This shouldn't happen: callers
       *    should either use lib/thread or not try to initialize VThreadIDs.
       * Or,
       * 2) An asynchronous signal interrupted the process of assigning
       *    a VThreadID and resulted in multiple allocation.  This either
       *    means the cooking function is broken - it should block signals - or
       *    an ASSERT triggered while setting up the VThreadID.
       */
      Log("VThreadBase reinitialization, old: %d %s, new: %d %s.\n",
          realBase->id, realBase->name, base->id, base->name);
   }

   return firstTime;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBaseSafeDeleteTLS --
 *
 *      Safely delete the TLS slot.  Called when manually "forgetting" a thread,
 *      or (for Posix) at TLS destruction (so we can forget the pthread_t).
 *
 *      Some of the cleanups have to be performed with a valid TLS slot so
 *      e.g. AllocTrack knows the current thread.  Note that the argument is
 *      'void *' only so this can serve as a TLS destructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Temporarily sets TLS, and restores it before returning.
 *
 *-----------------------------------------------------------------------------
 */

static void
VThreadBaseSafeDeleteTLS(void *tlsData)
{
   VThreadBaseData *data = tlsData;

   if (data != NULL) {
      if (vthreadBaseGlobals.freeIDFunc != NULL) {
         Bool success;
         VThreadBaseData tmpData = *data;

         /*
          * Cleanup routines (specifically, Log()) need to be called with
          * valid TLS, so switch to a stack-based TLS slot containing just
          * enough for the VThreadBase services, clean up, then clear the
          * TLS slot.
          */
         success = VThreadBaseSetBase(&tmpData);
         VERIFY(success);

         if (vmx86_debug) {
            Log("Forgetting VThreadID %d (\"%s\").\n", data->id, data->name);
         }
         (*vthreadBaseGlobals.freeIDFunc)(data);

         success = VThreadBaseSetBase(NULL) &&
                   VThreadBaseSetID(VTHREAD_INVALID_ID);
         VERIFY(success);
      }
      Atomic_Dec(&vthreadBaseGlobals.numThreads);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBase_ForgetSelf --
 *
 *      Forget the TLS parts of a thread.
 *
 *      If not intending to reallocate TLS, avoid querying the thread's
 *      VThread_CurID or VThread_CurName between this call and thread
 *      destruction.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
VThreadBase_ForgetSelf(void)
{
   VThreadBaseSafeDeleteTLS(VThreadBaseGetBase());
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBase_SetName --
 *
 *      Override the default thread name with a new name.
 *
 *      Historical: this subsumes the behavior of the old 
 *      lib/nothread VThread_Init, replacing it with something that is
 *      optional.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      If current thread does not have a TLS block, one is allocated.
 *
 *-----------------------------------------------------------------------------
 */

void
VThreadBase_SetName(const char *name)  // IN: new name
{
   uint32 len = strlen(name);
   VThreadBaseData *base = VThreadBaseGetAndInitBase();

   ASSERT(name);

   if (len >= sizeof base->name) {
      if (vmx86_debug) {
         Warning("%s: thread name (%s) exceeds maximum length (%u)\n",
                 __FUNCTION__, name, (uint32) sizeof base->name -1);
      }

      len = sizeof base->name - 1;
   }

   memcpy(base->name, name, len);
   base->name[len] = '\0';
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBaseGetNative --
 *
 *      Gets a native representation of the thread ID, which can be stored
 *      in a pointer.
 *
 * Results:
 *      Native representation of a thread ID.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void *
VThreadBaseGetNative(void)
{
   /* All thread types must fit into a uintptr_t so we can set them atomically. */
#ifdef _WIN32
   /*
    * On Windows, use a ThreadId instead of the thread handle, to avoid
    * holding a reference that is hard to clean up.
    */
   ASSERT_ON_COMPILE(sizeof (DWORD) <= sizeof (void*));
   return (void *)(uintptr_t)GetCurrentThreadId();
#else
   ASSERT_ON_COMPILE(sizeof (pthread_t) <= sizeof (void*));
   return (void *)(uintptr_t)pthread_self();
#endif
}


#ifdef _WIN32
/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBaseNativeIsAlive --
 *
 *      Determine if the thread described by the native thread ID is alive.
 *
 *      The algorithm is not perfect - native thread IDs can be reused
 *      by the host OS.  But in such cases we will simply fail to reclaim
 *      VThreadIDs, and such cases are rare.
 *
 * Results:
 *      TRUE if alive, may have (rare) false positives.
 *      FALSE if definitely dead.
 *
 * Side effects:
 *      Makes OS calls to determine thread liveliness.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
VThreadBaseNativeIsAlive(void *native)
{
   // Different access level due to impersonation see PR#780775
   HANDLE hThread = OpenThread(Hostinfo_OpenThreadBits(), FALSE,
                               (DWORD)(uintptr_t)native);

   if (hThread == NULL) {
      /*
       * An access denied error tells us that the process is alive despite
       * the inability of accessing its information. Commonly, access denied
       * occurs when a process is trying to completely protect itself (e.g.
       * a virus checker).
       */

      return (GetLastError() == ERROR_ACCESS_DENIED) ? TRUE : FALSE;
   } else {
      DWORD exitCode;
      BOOL success = GetExitCodeThread(hThread, &exitCode);

      ASSERT(success);  /* No known ways GetExitCodeThread can fail */
      CloseHandle(hThread);

      return exitCode == STILL_ACTIVE;
   }
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBaseSimpleNoID --
 *
 *      Default implementation of a function that is called whenever a thread
 *      is found that contains no VThreadID.
 *
 *      This implementation recycles VThreadIDs of dead threads, trying to keep
 *      the VThreadID namespace compacted as much as possible.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      After running, this thread has a VThreadID with a default name.
 *      (Or Panics before returning.)
 *
 *-----------------------------------------------------------------------------
 */

static void
VThreadBaseSimpleNoID(void)
{
   VThreadID newID;
   Bool reused = FALSE;
   Bool result;
   void *newNative = VThreadBaseGetNative();
   HashTable *ht = VThreadBaseGetNativeHash();
   VThreadBaseData *base;

   /* Require key allocation before TLS read */
   ASSERT(VThreadBaseAreKeysInited());

   /* Before allocating a new ID, try to reclaim any old IDs. */
   for (newID = 0;
        newID < Atomic_Read(&vthreadBaseGlobals.dynamicID);
        newID++) {
      void *newKey = (void *)(uintptr_t)newID;

      /*
       * Windows: any entry that is found and not (alive or NULL)
       *    is reclaimable.  The check is slightly racy, but the race
       *    would only cause missing a reclaim which isn't a problem.
       * Posix: thread exit is hooked (via TLS destructor) and sets
       *    entries to NULL, so any entry that is NULL is reclaimable.
       */
#ifdef _WIN32
      void *oldNative;
      reused = HashTable_Lookup(ht, newKey, &oldNative) &&
               (oldNative == NULL ||
                !VThreadBaseNativeIsAlive(oldNative)) &&
               HashTable_ReplaceIfEqual(ht, newKey, oldNative, newNative);
#else
      reused = HashTable_ReplaceIfEqual(ht, newKey, NULL, newNative);
#endif
      if (reused) {
         break;
      }
   }

   if (!reused) {
      void *newKey;

      newID = Atomic_ReadInc32(&vthreadBaseGlobals.dynamicID);
      /*
       * Detect VThreadID overflow (~0 is used as a sentinel).
       * Leave a space of ~10 IDs, since the increment and bounds-check
       * are not atomic.
       */
      VERIFY(newID < VTHREAD_INVALID_ID - 10);

      newKey = (void *)(uintptr_t)newID;
      result = HashTable_Insert(ht, newKey, newNative);
      VERIFY(result);
   }

   /* ID picked.  Now do the important stuff. */
   base = Util_SafeCalloc(1, sizeof *base);
   base->id = newID;
   Str_Sprintf(base->name, sizeof base->name, "vthread-%u", newID);

   result = VThreadBase_InitWithTLS(base);
   ASSERT(result);

   if (vmx86_debug && reused) {
      Log("VThreadBase reused VThreadID %d.\n", newID);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBaseSimpleFreeID --
 *
 *      Default TLS storage destructor.
 *
 *      The SimpleNoID function uses malloc'd memory to allow an unlimited
 *      number of VThreads in the process, and uses a hash table to track
 *      live VThreadIDs so to allow VThreadID recycling.  Both of these
 *      require cleanup.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VThreadBaseSimpleFreeID(void *tlsData)
{
   HashTable *ht = VThreadBaseGetNativeHash();
   VThreadBaseData *data = tlsData;
   const void *hashKey = (const void *)(uintptr_t)data->id;

   HashTable_ReplaceOrInsert(ht, hashKey, NULL);
   free(data);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBase_SetNoIDFunc --
 *
 *      Sets the hook function to be called when a thread is found with
 *      no VThreadID.  The hook function is expected to call
 *      VThreadBase_InitWithTLS() with a valid new ID.
 *
 *      On Posix, this callback is called with signals masked to prevent
 *      accidental double-allocation of IDs.  On Windows, the constraint is
 *      that the callback cannot service an APC between allocating an ID and
 *      initializing the thread with that ID, as such is likely
 *      to accidentally double-allocate IDs.
 *
 *      An optional destructor can be supplied to clean up memory.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
VThreadBase_SetNoIDFunc(void (*hookFunc)(void),       // IN: new hook function
                        void (*destroyFunc)(void *))  // IN/OPT: new TLS destructor
{
   ASSERT(hookFunc);

   /*
    * The hook function can only be set once, and must be set before
    * any VThreadIDs are allocated so that the hook can control the VThreadID
    * namespace.
    *
    * If the process has had only a single thread, that thread can be forgotten
    * via VThreadBase_ForgetSelf() and this function safely called.
    */
   ASSERT(vthreadBaseGlobals.noIDFunc == VThreadBaseSimpleNoID &&
          Atomic_Read(&vthreadBaseGlobals.numThreads) == 0);

   vthreadBaseGlobals.noIDFunc = hookFunc;
   vthreadBaseGlobals.freeIDFunc = destroyFunc;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBaseInit --
 *
 *      Performs basic initialization of this thread including setting
 *      up the thread local storage and assigning a thread id.
 *
 *-----------------------------------------------------------------------------
 */

static void
VThreadBaseInit(void)
{
   Atomic_Init();
   VThreadBaseInitKeys();

   /*
    * The code between the last pthread_getspecific and the eventual
    * call to pthread_setspecific either needs to run with async signals
    * blocked or tolerate reentrancy.  Simpler to just block the signals.
    * See bugs 295686 & 477318.  Here, the problem is that we could allocate
    * two VThreadIDs (via a signal during the NoID callback).
    */
   
   NO_ASYNC_SIGNALS_START;
   if (VThreadBaseGetBase() == NULL) {
      (*vthreadBaseGlobals.noIDFunc)();
   }
   NO_ASYNC_SIGNALS_END;
}


#if !defined _WIN32
/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBase_IsInSignal --
 *
 *      Accessor for whether current thread is or is not in a signal.
 *      lib/sig handles keeping this accurate.
 *
 * Results:
 *      Returns TRUE if a signal handler is somewhere on the stack.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VThreadBase_IsInSignal(void)
{
   VThreadBaseData *base = VThreadBaseGetAndInitBase();

   return Atomic_Read(&base->signalNestCount) > 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBase_SetIsInSignal --
 *
 *      Marks the current thread as or as not being inside a signal
 *      handler.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      As above.
 *
 *-----------------------------------------------------------------------------
 */

void
VThreadBase_SetIsInSignal(VThreadID tid,    // IN:
                          Bool isInSignal)  // IN:
{
   VThreadBaseData *base = VThreadBaseGetAndInitBase();

   /* It is an error to clear isInSignal while not in a signal. */
   ASSERT(Atomic_Read(&base->signalNestCount) > 0 || isInSignal);

   Atomic_Add(&base->signalNestCount, isInSignal ? 1 : -1);
}
#endif
