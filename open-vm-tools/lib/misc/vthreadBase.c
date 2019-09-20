/*********************************************************
 * Copyright (C) 2010-2019 VMware, Inc. All rights reserved.
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
 *         VThreadBase_CurID - Returns a VThreadID.
 *         VThreadBase_SetName - Sets current thread name.
 *
 *      Functions useful for implementing a full thread library:
 *         VThreadBase_ForgetSelf - Clears the thread name for current thread,
 *                                  to clean up resource usage prior to thread
 *                                  exit. (Only needed if __thread support is
 *                                  unavailable).
 *      Historical quirks:
 *      * Most other code uses VThread_Foo instead of VThreadBase_Foo; the
 *        public header file uses inlines to convert names.
 *
 *      By default, threads will be given names like "vthread-123",
 *      "vthread-987", etc. to match IDs provided by the host operating system.
 *      Use VThread_SetName to provide more meaningful names.
 *
 *      On most platforms, make use of __thread.
 *      On a few platforms (see vthreadBase.h definition of VMW_HAVE_TLS),
 *         use pthread getspecific/setspecific for thread name and signal count.
 */

#if defined _WIN32
#  include <windows.h>
#else
#  if defined(__sun__) && !defined(_XOPEN_SOURCE)
     /*
      * Solaris headers don't define constants we need unless
      * the Posix standard is somewhat modern.  Most of our builds
      * set this; we should chase down the oversight.
      */
#    define _XOPEN_SOURCE 500
#  endif
#  if defined __linux__
#    include <sys/syscall.h>   // for gettid(2)
#  endif
#  include <pthread.h>
#endif
#include <stdlib.h>
#include <stdio.h>   /* snprintf */

#include "vmware.h"
#include "vthreadBase.h"
#include "util.h"


/*
 * Custom code for platforms without thread-local storage.
 * On these platforms, use constructors/destructors to manage
 * pthread keys.
 */
#if !defined(VMW_HAVE_TLS)
static pthread_key_t sigNestCountKey;
static pthread_key_t vthreadNameKey;

/*
 * To avoid needing initialization, manage via constructor/destructor.
 * We only do this on Apple and Android, where:
 * - Apple reserves the first several pthread_key_t for libc
 * - Android sets bit 31 in all valid pthread_key_t
 * Thus, the default 0 value indicates unset.
 */
__attribute__((constructor))
static void VThreadBaseConstruct(void)
{
   (void) pthread_key_create(&sigNestCountKey, NULL);
   (void) pthread_key_create(&vthreadNameKey, NULL);
}
__attribute__((destructor))
static void VThreadBaseDestruct(void)
{
   /*
    * TLS destructors have a wrinkle: if you unload a library THEN
    * a thread exits, the TLS destructor will SEGV because it's unloaded.
    * Net result, we must either leak a TLS key or the value in the TLS
    * slot. This path chooses to leak the value in the TLS slot... and
    * as the value is an integer and not a pointer, there is no leak.
    */
   if (vthreadNameKey != 0) {
      (void) pthread_key_delete(vthreadNameKey);
   }
   if (sigNestCountKey != 0) {
      (void) pthread_key_delete(sigNestCountKey);
   }
}
#endif

#ifdef VMW_HAVE_TLS

#if !defined _WIN32
/*
 * Signal counting code. Signal counting operates self-contained,
 * having no particular dependency on the rest of VThreadBase
 * (especially the VThreadBaseData memory allocation).
 */
static __thread int sigNestCount;
#endif

/*
 * VThread name code.
 * Common case: thread-local storage is available, and this is easy.
 * Uncommon case: iOS/Android lack thread-local storage for the versions
 *   we currently use.
 *   Problems:
 *      PR 1686126 shows that malloc() on the thread-name path can lead to
 *      deadlock (due to signal within malloc()), yet somehow we must come up
 *      with some sort of name always.
 *      PR 1626963 shows that even if we could safely allocate memory, we
 *      cannot free it: if we were to pthread_key_create(..., &free) and this
 *      library were unloaded via dlclose() before all threads exit, the
 *      call to free() (actually, our library's .plt thunk) would crash.
 *   Solution:
 *      Solve the simpler problem. An external SetName is assumed to not
 *      be inside a signal handler (malloc is safe) AND is assumed there
 *      will be an eventual ForgetSelf (avoids leak). This isn't strictly
 *      true of all cases, but remember this only applies to iOS/Android in
 *      the first place. If SetName has not been called on the current thread,
 *      make up a thread-specific name and store it in a (non-thread-safe)
 *      global buffer as a best-effort solution.
 * Discarded ideas:
 *   1) allocating thread-name memory on first use for anonymous threads.
 *      Guaranteed to leak memory due to PR 1626963 (above).
 *   2) use Android prctl(PR_GET_NAME) + iOS pthread_getname_np(). Storing names
 *      works great, but both APIs require caller-supplied buffers ... which
 *      leads right back to needing a per-thread buffer, and the malloc()
 *      problem above. Except it's worse: we could find we have a perfectly
 *      valid name, but cannot allocate a buffer to return it.
 *   3) Re-plumbing VThreadBase_CurName() to take a caller-supplied buffer
 *      instead of returning a pointer; that would be an ugly, invasive API
 *      change that de-optimizes "modern" OSes by forcing them to strcpy when
 *      they could just return a pointer.
 *   4) Use 2-4 pthread_keys for storage (4-8 chars per key). Same problem as
 *      (2) above... this stores the data fine, but still does not provide
 *      a per-thread buffer for returning the data.
 *   5) Return a static string "<unnamed>" for *all* unknown threads.
 *      The picked solution above does have a race in that two unnamed threads
 *      generating a default name at the same time could race and one thread
 *      may get the wrong name; this option avoids that problem by refusing to
 *      name unnamed threads AT ALL.
 *      Discarded because best-effort thread identification seems better than
 *      technically-correct-but-less-useful refusal to provide any name.
 * To summarize in two points: a per-thread buffer is pretty much essential
 * for correctness, and minimize effort for "old" platforms.
 */

static __thread char vthreadName[VTHREADBASE_MAX_NAME];

#endif /* VMW_HAVE_TLS */


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBaseGetStableID --
 *
 *      Stable thread-id function.
 *
 *      In contrast to VThreadBase_GetKernelID below, this stable ID is safe
 *      for caching. Unfortunately, it tends to not be human readable,
 *      is not understood by the kernel, and makes no sense passed to
 *      any other process.
 *
 *      Win32 was always safe; for POSIX, we instead make use of the fact
 *      that pthread_t values (by definition) have to be stable across process
 *      fork. That is:
 *         pthread_t before = pthread_self();
 *         fork();
 *         pthread_t after = pthread_self();
 *         ret = pthread_equal(before, after);  <---- POSIX requires equality
 *      POSIX leaves the exact mechanism unspecified, but in practice
 *      most POSIX OSes make pthread_t a pointer and make use of the fact that
 *      the address space is fully cloned so the pointer will not change.
 *      (An exception is Solaris, which uses integer LWP indexes but
 *      clones the per-process LWP table at fork).
 *
 *      The assumption above is technically non-portable, as POSIX also
 *      permits pthread_t to be a structure. We do not support any OS
 *      which uses a structure definition.
 *
 * Results:
 *      Some sort of stable ID.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static uintptr_t
VThreadBaseGetStableID(void)
{
#if defined _WIN32
   return GetCurrentThreadId();
#elif defined __sun__
   /* pthread_t is a uint_t index into a per-process LWP table */
   return pthread_self();
#else
   /* pthread_t is (hopefully) an opaque pointer type */
   return (uintptr_t)(void *)pthread_self();
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBase_CurID --
 *
 *      Get the current thread ID.
 *
 * Results:
 *      A VThreadID. Always succeeds.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VThreadID
VThreadBase_CurID(void)
{
   return VThreadBaseGetStableID();
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBase_GetKernelID --
 *
 *      Native thread-id function. USE WITH GREAT CAUTION.
 *
 *      Details below. In short, the ID returned by this function is both
 *      'pretty' (tends to be short, e.g. readable as a decimal) and 'native'
 *      in that it is useful for correlating with kernel threads.
 *      However, this ID is not fork-safe on at least Linux.
 *
 *      In practice, VThreadBase chooses to use this ID for thread names only.
 *
 *      Most POSIX: With most modern threading implementations, threads are
 *        "lightweight processes" (LWP), so any native TID changes after
 *        a fork(). Which leads to pthread_atfork() - you can find out that TID
 *        changed, but it's up to you to fix up all cached copies.
 *        (A clever soul might suggest just continuing to use the old TID.
 *        That clever soul is not so clever, having forgotten that POSIX OSes
 *        recycle LWPs so all it takes is a couple of forks for you to have a
 *        cached TID on one thread match the native TID on another thread. Hope
 *        you didn't need that TID for correctness!).
 *        The good news is nearly all POSIX has a pthread NP API (non-portable)
 *        to provide the right thing.
 *      Linux (glibc): is the exception to "nearly all". The *only* way to get a
 *        system ID is via gettid() syscall. Which is a syscall and thus
 *        expensive relative to every other OS. This code implements the pthread
 *        NP wrapper that glibc *should* have.
 *      Windows: good news. Not having a fork() API means the 'pretty'
 *        ID returned here is stable forever. No special cases.
 *      Solaris: good news. Solaris implements the LWP namespace *per process*,
 *        which it clones on fork, meaning the forked child still gets the
 *        same LWP IDs. Likely a legacy of SunOS which had forkall().
 *
 *      Obviously, specific mechanisms for obtaining native IDs are *highly*
 *      non-portable, as indicated by the _np suffixes.
 *
 * Results:
 *      Some sort of system ID (e.g. LWP, Task, ...)
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#if defined(__linux__) && !defined(__ANDROID__)
static pid_t
vmw_pthread_getthreadid_np(void)
{
   static __thread struct {
      pid_t pid;
      pid_t tid;
   } cache = { 0, 0 };

   /*
    * Linux (well, glibc) gets TWO things wrong, but the combination makes
    * a right, oddly enough. (1) There is no gettid function, because glibc
    * people decided anybody who needs a system ID is wrong (glibc bug 6399)
    * and should instead do a system call to get it. (2) they 'optimized'
    * getpid() to cache its result (which depends on forking only via POSIX
    * calls and not via syscalls), then decided they knew better than Linus
    * when he told them this was wrong.
    * BUT... the getpid cache can be used to make a sufficiently-correct and
    * fast gettid cache.
    */
   if (UNLIKELY(cache.pid != getpid())) {
      cache.pid = getpid();
      cache.tid = syscall(__NR_gettid);
   }
   return cache.tid;
}
#endif

uint64
VThreadBase_GetKernelID(void)
{
#if defined _WIN32
   return /* DWORD */ GetCurrentThreadId();
#elif defined __APPLE__
   /* Available as of 10.6 */
   uint64 hostTid;  // Mach Task ID
   pthread_threadid_np(NULL /* current thread */, &hostTid);
   return hostTid;
#elif defined __ANDROID__
   return /* pid_t */ gettid();  // Thank you, Bionic.
#elif defined __linux__
   return vmw_pthread_getthreadid_np();
#elif defined __sun__
   return /* uint_t */ pthread_self();  // Solaris uses LWP as pthread_t
#elif defined __FreeBSD__
#  if 0
   // Requires FreeBSD 9, we currently use FreeBSD 6. Include <pthread_np.h>
   return /* int */ pthread_getthreadid_np();
#  endif
   // Best effort until FreeBSD header update
   return (uint64)(uintptr_t)(void *)pthread_self();
#else
#  error "Unknown platform"
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBaseSafeName --
 *
 *      Generates a "safe" name for the current thread into a buffer.
 *
 *      Always succeeds, never recurses.
 *
 * Results:
 *      The current thread name.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VThreadBaseSafeName(char *buf,   // OUT: new name
                    size_t len)  // IN:
{
   /*
    * This function should not ASSERT, Panic or call a Str_
    * function (that can ASSERT or Panic), as the Panic handler is
    * very likely to query the thread name and end up right back here.
    */
   snprintf(buf, len - 1 /* keep buffer NUL-terminated */,
            "host-%"FMT64"u", VThreadBase_GetKernelID());
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBase_CurName --
 *
 *      Get the current thread name.
 *
 *      Always succeeds, never recurses.
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
#if defined VMW_HAVE_TLS
   if (UNLIKELY(vthreadName[0] == '\0')) {
      /*
       * Unnamed thread. If the thread's name mattered, it would
       * have called VThreadBase_SetName() earlier.
       *
       * Pick an arbitrary name and store it in thread-local storage.
       */

      VThreadBaseSafeName(vthreadName, sizeof vthreadName);
   }
   return vthreadName;
#else
   const char *raw;

   raw = (vthreadNameKey != 0) ? pthread_getspecific(vthreadNameKey)
                                : NULL;
   if (LIKELY(raw != NULL)) {
      return raw;  // fast path
   } else {
      /*
       * Unnamed thread. If the thread's name mattered, it would
       * have called VThreadBase_SetName() earlier.
       *
       * Use a static buffer for a partly-sane name and hope the
       * Panic handler dumps enough information to figure out what went
       * wrong. If better accuracy is needed, add thread-local storage
       * support for the platform.
       */

      static char name[48];
      VThreadBaseSafeName(name, sizeof name);
      return name;
   }
#endif
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
 *      If TLS is not supported, allocates thread-local memory which leaks
 *      on thread exit. (If this leak is a problem, implement TLS).
 *
 *-----------------------------------------------------------------------------
 */

void
VThreadBase_SetName(const char *name)  // IN: new name
{
   ASSERT(name != NULL);

   if (vmx86_debug && strlen(name) >= VTHREADBASE_MAX_NAME) {
      Warning("%s: thread name (%s) exceeds maximum length (%u)\n",
              __FUNCTION__, name, (uint32)VTHREADBASE_MAX_NAME - 1);
   }

#if defined VMW_HAVE_TLS
   /*
    * Never copy last byte; this ensures NUL-term is always present.
    * The NUL-term is always present because vthreadName is static,
    * but gcc-8 generates a warning if it doesn't see it being explicilty
    * set.
    */

   strncpy(vthreadName, name, sizeof vthreadName - 1);
   vthreadName[sizeof vthreadName - 1] = '\0';
#else
   do {
      char *buf;

      ASSERT(vthreadNameKey != 0);
      ASSERT(!VThreadBase_IsInSignal());  // Cannot alloc in signal handler

      /*
       * The below code is racy (signal between get/set), but the
       * race is benign. The signal handler would either get a safe
       * name or "" (if it interrupts before setspecific or before strncpy,
       * respectively). But as signal handlers may not call SetName,
       * this will not double-allocate.
       */
      buf = pthread_getspecific(vthreadNameKey);
      if (buf == NULL) {
         buf = Util_SafeCalloc(1, VTHREADBASE_MAX_NAME);
         pthread_setspecific(vthreadNameKey, buf);
      }

      strncpy(buf, name, VTHREADBASE_MAX_NAME - 1);
   } while(0);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBase_SetNamePrefix --
 *
 *      Override the default thread name with a new name based on
 *      the supplied prefix. Format is "{prefix}-{id}"
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
VThreadBase_SetNamePrefix(const char *prefix)  // IN: name prefix
{
   char buf[VTHREADBASE_MAX_NAME];

   ASSERT(prefix != NULL);

   snprintf(buf, sizeof buf, "%s-%" FMT64 "u",
            prefix, VThreadBase_GetKernelID());
   buf[sizeof buf - 1] = '\0';  // snprintf does not ensure NUL-term
   VThreadBase_SetName(buf);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBase_ForgetSelf --
 *
 *      Forget the TLS parts of a thread.
 *
 *      If not intending to reallocate TLS, avoid querying the thread's
 *      VThread_CurName between this call and thread destruction.
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
#if !defined VMW_HAVE_TLS
   char *buf;
#endif

   if (vmx86_debug) {
      Log("Forgetting VThreadID %" FMTPD "d (\"%s\").\n",
          VThread_CurID(), VThread_CurName());
   }

   /*
    * The VThreadID is fixed (see StableID above).
    * Only the name needs clearing.
    */
#if defined VMW_HAVE_TLS
   memset(vthreadName, '\0', sizeof vthreadName);
#else
   ASSERT(vthreadNameKey != 0);
   ASSERT(!VThreadBase_IsInSignal());

   buf = pthread_getspecific(vthreadNameKey);
   pthread_setspecific(vthreadNameKey, NULL);
   free(buf);
#endif
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
#if defined VMW_HAVE_TLS
   return sigNestCount > 0;
#else
   return (intptr_t)pthread_getspecific(sigNestCountKey) > 0;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThreadBase_SetIsInSignal --
 *
 *      Marks the current thread as or as not being inside a signal handler.
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
VThreadBase_SetIsInSignal(Bool isInSignal)  // IN:
{
#if defined VMW_HAVE_TLS
   sigNestCount += (isInSignal ? 1 : -1);
#else
   intptr_t cnt;
   ASSERT(sigNestCountKey != 0);
   cnt = (intptr_t)pthread_getspecific(sigNestCountKey);
   cnt += (isInSignal ? 1 : -1);
   (void) pthread_setspecific(sigNestCountKey, (void *)cnt);
#endif
}
#endif
