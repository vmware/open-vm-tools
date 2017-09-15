/*********************************************************
 * Copyright (C) 2003-2016 VMware, Inc. All rights reserved.
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
 * impersonatePosix.c --
 *
 * Description:
 *      Posix specific functions to impersonate as specific users.
 */

#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#if !defined(VMX86_TOOLS)
#include <pthread.h>
#endif
#include <stdio.h>

#include "impersonateInt.h"
#include "su.h"
#include "posix.h"

#if !defined(VMX86_TOOLS)
static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER; 
static pthread_key_t threadLocalStorageKey = INVALID_PTHREAD_KEY_VALUE;
static void ThreadLocalFree(void *ptr);
#else
static ImpersonationState *impLinux = NULL;
#endif

static Bool ImpersonateDoPosix(struct passwd *pwd);

/*
 *----------------------------------------------------------------------------
 *
 * ImpersonateInit --
 *
 *      Linux specific initialization (thread local storage for linux)
 *
 * Results:  
 *      None.
 *
 * Side effects:
 *      Memory created.
 * 
 *----------------------------------------------------------------------------
 */

void
ImpersonateInit(void)
{
#if !defined(VMX86_TOOLS)
   int status;

   status = pthread_key_create(&threadLocalStorageKey, ThreadLocalFree);
   if (status != 0) {
      Warning("Impersonate: key_create failed: %d\n", status);
      VERIFY(status == 0);
      return;
   }
   VERIFY(threadLocalStorageKey != INVALID_PTHREAD_KEY_VALUE);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * ThreadLocalFree --
 *
 *      A wrapper for "free()".  This function is called when a thread
 *      terminates so that the thread-local state can be deallocated.
 *
 *----------------------------------------------------------------------
 */
#if !defined(VMX86_TOOLS)
static void
ThreadLocalFree(void *ptr)
{
   ImpersonationState *imp = (ImpersonationState*) ptr;

   IMPWARN(("Impersonate: ThreadLocalFree(0x%08x)\n", imp));

   ASSERT(imp);
   ASSERT(imp->impersonatedUser == NULL);
   ASSERT(imp->refCount == 0);

   free(imp);
}
#endif

/*
 *----------------------------------------------------------------------------
 *
 * ImpersonateGetTLS --
 *
 *      This function abstracts away the differences between Linux and 
 *      Windows for obtaining a pointer to thread-local state.
 *
 * Results:  
 *      Returns pointer to thread-local state.
 *
 * Side effects:
 *      On Linux this function will allocate state on the first occasion
 *      that a particular thread calls this function for the first time.
 * 
 *----------------------------------------------------------------------------
 */

ImpersonationState *
ImpersonateGetTLS(void)
{
   ImpersonationState *ptr = NULL;
   int status;

   /* If a prior call has already allocated state, then use it */
#if !defined(VMX86_TOOLS)
   /* If a prior call has already allocated state, then use it */
   ptr = pthread_getspecific(threadLocalStorageKey);
#else
	ptr = impLinux;
#endif
   if (ptr != NULL) {
      return ptr;
   }

   /* No state allocated, so we need to allocate it */
   ptr = calloc(1, sizeof *ptr);
   VERIFY(ptr);
#if !defined(VMX86_TOOLS)
   status = pthread_setspecific(threadLocalStorageKey, ptr);
#else
	impLinux = ptr;
	status = 0;
#endif
   if (status != 0) {
      Warning("Impersonate: setspecific: %d\n", status);
      VERIFY(status == 0);
   }

   return ptr;
}

/*
 *----------------------------------------------------------------------------
 *
 * ImpersonateRunas --
 *
 *      Impersonate as the appropriate runas user. In linux this is always
 *      the config file owner regardless the calling context. 
 *
 * Results:  
 *      TRUE if impersonation succeeds, FALSE otherwise.
 *
 * Side effects:
 *      imp.impersonatedUser may be updated.
 * 
 *----------------------------------------------------------------------------
 */

Bool
ImpersonateRunas(const char *cfg,        // IN
                 const char *caller,     // IN
                 AuthToken callerToken)  // IN
{
   /*
    * In linux, this call always impersonates as the owner of the config file.
    */

   ASSERT(!caller && !callerToken);
   return ImpersonateOwner(cfg);
}


/*
 *----------------------------------------------------------------------------
 *
 * ImpersonateOwner --
 *
 *      Impersonate the owner of the config file. Only makes sense on linux.
 *
 * Results:  
 *      TRUE if impersonation succeeds, false otherwise.
 *
 * Side effects:
 *      imp.impersonatedUser may be updated.
 * 
 *----------------------------------------------------------------------------
 */

Bool 
ImpersonateOwner(const char *file)          // IN
{
   struct stat buf;
   char buffer[BUFSIZ];
   struct passwd pw;
   struct passwd *ppw = &pw;
   int error;

   if (Posix_Stat(file, &buf) == -1) {
      Warning("Failed to lookup owner for: %s. Reason: %s\n", file, 
              Err_Errno2String(errno));
      return FALSE;
   }

   if ((error = Posix_Getpwuid_r(buf.st_uid, &pw, buffer, BUFSIZ, &ppw)) != 0 || !ppw) {
      if (error == 0) {
         error = ENOENT;
      }
      Warning("Failed to lookup user with uid: %" FMTUID ". Reason: %s\n", buf.st_uid,
              Err_Errno2String(error));
      return FALSE;
   }
   
   return ImpersonateDoPosix(ppw);
}


/*
 *----------------------------------------------------------------------
 *
 * ImpersonateUndo -- Linux specific
 *
 *      Change back into the superuser 
 *
 * Side effects:
 *
 *	EUID is set back to the superuser, and environment variables are
 *	updated back.
 *
 *----------------------------------------------------------------------
 */

Bool
ImpersonateUndo(void)
{
   char buffer[BUFSIZ];
   struct passwd pw;
   struct passwd *ppw = &pw;
   ImpersonationState *imp = NULL;
   int ret;
   int error;

#if !defined(VMX86_TOOLS)
   pthread_mutex_lock(&mut);
#endif

   imp = ImpersonateGetTLS();
   ASSERT(imp);
   //ASSERT(imp->impersonatedUser);
   
   if ((error = Posix_Getpwuid_r(0, &pw, buffer, BUFSIZ, &ppw)) != 0 || !ppw) {
      if (error == 0) {
         error = ENOENT;
      }
      ret = error;
      Warning("Failed to get password entry for uid 0: %s\n",
              Err_Errno2String(error));
      goto exit;
   }

#if __APPLE__
   NOT_IMPLEMENTED();
#else
   /* Return to root */
   ret = Id_SetEUid(ppw->pw_uid);
   if (ret < 0) {
      goto exit;
   }
#endif

   ret = Id_SetGid(ppw->pw_gid);
   if (ret < 0) {
      goto exit;
   }

   /* 
    * The call to initgroups leaks memory in versions of glibc earlier than 2.1.93.
    * See bug 10042. -jhu 
    */

   ret = initgroups(ppw->pw_name, ppw->pw_gid);
   if (ret < 0) {
      goto exit;
   }

   /* Restore root's environment */
   Posix_Setenv("USER", ppw->pw_name, 1);
   Posix_Setenv("HOME", ppw->pw_dir, 1);
   Posix_Setenv("SHELL", ppw->pw_shell, 1);

   free((char *)imp->impersonatedUser);
   imp->impersonatedUser = NULL;
   ret = 0;

exit:
   VERIFY(ret == 0);
#if !defined(VMX86_TOOLS)
   pthread_mutex_unlock(&mut);
#endif
   return (ret ? FALSE : TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * ImpersonateDoPosix --
 *
 *      Impersonate as the user corresponding to the passwd entry
 *      XXX: Mostly copied from vmsd_impersonate.c
 *
 * Results:  
 *      TRUE if impersonation succeeds, FALSE otherwise.
 *
 * Side effects:
 *      imp.impersonatedUser is updated.
 * 
 *----------------------------------------------------------------------------
 */

Bool
ImpersonateDoPosix(struct passwd *pwd)            // IN
{
   int ret = 0;
   ImpersonationState *imp = NULL;

#if !defined(VMX86_TOOLS)
   pthread_mutex_lock(&mut);
#endif

   imp = ImpersonateGetTLS();
   ASSERT(imp);

   if (pwd->pw_uid == geteuid()) {
      imp->refCount++;
      IMPWARN(("ImpersonateDoPosix (%s : %x : %x) refcount = %d\n",
               imp->impersonatedUser, getpid(), imp, imp->refCount));
      goto unlock;
   }

   ASSERT(getuid() == 0);
   VERIFY(geteuid() == 0);

   ret = Id_SetGid(pwd->pw_gid);
   if (ret < 0) {
      goto exit;
   }
   
   /* 
    * The call to initgroups leaks memory in versions of glibc earlier than
    * 2.1.93.See bug 10042. -jhu 
    */
   
   ret = initgroups(pwd->pw_name, pwd->pw_gid);
   if (ret < 0) {
      goto exit;
   }

#if __APPLE__
   NOT_IMPLEMENTED();
#else
   ret = Id_SetEUid(pwd->pw_uid);
   if (ret < 0) {
      goto exit;
   }
#endif

   /* Setup the user's environment */
   Posix_Setenv("USER", pwd->pw_name, 1);
   Posix_Setenv("HOME", pwd->pw_dir, 1);
   Posix_Setenv("SHELL", pwd->pw_shell, 1);

   imp->impersonatedUser = strdup(pwd->pw_name);
   VERIFY(imp->impersonatedUser);

exit:
   imp->refCount = 1;
   VERIFY(ret == 0);
unlock:
#if !defined(VMX86_TOOLS)
   pthread_mutex_unlock(&mut);
#endif
   return (ret ? FALSE : TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * ImpersonateDo --
 *
 *      Impersonate as user. Can be nested if impersonated as that same user 
 *      each time. Can switch back to root temporarily regardless of nesting
 *      level via Impersonate_ForceRoot. Calling Impersonate_UnforceRoot will
 *      return to original impersonation at the same nesting level.
 *
 * Results:  
 *      TRUE if impersonation succeeds, FALSE otherwise.
  *
 * Side effects:
 *      imp.impersonatedUser may be updated.
 * 
 *----------------------------------------------------------------------------
 */

Bool 
ImpersonateDo(const char *user,       // IN
              AuthToken token)        // IN
{
   char buffer[BUFSIZ];
   struct passwd pw;
   struct passwd *ppw = &pw;
   int error;

   if ((error = Posix_Getpwnam_r(user, &pw, buffer, BUFSIZ, &ppw)) != 0 || !ppw) {
      if (error == 0) {
         error = ENOENT;
      }
      Warning("Failed to get password entry for : %s. Reason: %s\n", user,
              Err_Errno2String(error));
      return FALSE;
   }

   return ImpersonateDoPosix(ppw);
}


/*
 *----------------------------------------------------------------------------
 *
 * ImpersonateForceRoot --
 *
 *      Go back to base impersonate level (LocalSystem/root) for a brief
 *      period of time.
 *      Should only be used when already impersonated. This call is not nestable.     
 *      No other impersonation is permitted before calling Impersonate_UnforceRoot.
 *
 * Results:  
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      imp.forceRoot is set to TRUE on success.
 * 
 *----------------------------------------------------------------------------
 */

Bool
ImpersonateForceRoot(void)
{
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * ImpersonateUnforceRoot --
 *
 *      Unforce from root to original impersonation context
 *
 * Results:  
 *      TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *      imp.forceRoot is set to FALSE on success
 * 
 *----------------------------------------------------------------------------
 */

Bool 
ImpersonateUnforceRoot(void)
{ 
   return TRUE;
}
