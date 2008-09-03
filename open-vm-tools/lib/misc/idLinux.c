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
 * idLinux.c --
 *
 *   uid/gid helpers.
 */

#include <errno.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <string.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/socket.h>
#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>
#endif

#include "vmware.h"
#include "su.h"
#include "vm_atomic.h"

#if defined(__linux__)
#ifndef GLIBC_VERSION_21
/*
 * SYS_ constants for glibc 2.0, some of which may already be defined on some of those
 * older systems.
 */
#ifndef SYS_setresuid
#define SYS_setresuid          164
#endif
#ifndef SYS_setresgid
#define SYS_setresgid          170
#endif
#define SYS_setreuid32         203
#define SYS_setregid32         204
#define SYS_setresuid32        208
#define SYS_setresgid32        210
#define SYS_setuid32           213
#define SYS_setgid32           214
#endif // ifndef GLIBC_VERSION_21

/*
 * 64bit linux has no 16 bit versions and
 * the 32bit versions have the un-suffixed names.
 * And obviously, we're not using glibc 2.0
 * for our 64bit builds!
 */
#ifdef VM_X86_64
#define SYS_setreuid32         (abort(), 0)
#define SYS_setregid32         (abort(), 0)
#define SYS_setresuid32        (abort(), 0)
#define SYS_setresgid32        (abort(), 0)
#define SYS_setuid32           (abort(), 0)
#define SYS_setgid32           (abort(), 0)
#endif

/*
 * On 32bit systems:
 * Set to 1 when system supports 32bit uids, cleared to 0 when system
 * supports 16bit calls only.  By default we assume that 32bit uids
 * are supported and clear this flag on first failure.
 *
 * On 64bit systems:
 * Only the 32bit uid syscalls exist, but they do not have the names
 * with the '32' suffix, so we get the behaviour we want by forcing
 * the code to use the unsuffixed syscalls.
 */
#ifdef VM_X86_64
static int uid32 = 0;
#else
static int uid32 = 1;
#endif
#endif // __linux__

#if defined(__APPLE__)
#include <sys/kauth.h>

static AuthorizationRef IdAuthCreate(void);
static AuthorizationRef IdAuthCreateWithFork(void);

#endif


#if !defined(__APPLE__)
/*
 *----------------------------------------------------------------------------
 *
 * Id_SetUid --
 *
 *	If calling thread has euid = 0, it sets real, effective and saved uid
 *	to the specified value.
 *	If calling thread has euid != 0, then only effective uid is set.
 *
 * Results:
 *      0 on success, -1 on failure, errno set
 *
 * Side effects:
 *      errno may be modified on success
 *
 *----------------------------------------------------------------------------
 */

int
Id_SetUid(uid_t euid)		// IN: new euid
{
#if defined(__FreeBSD__) || defined(sun)
   return setuid(euid);
#elif defined(linux)
   if (uid32) {
      int r = syscall(SYS_setuid32, euid);
      if (r != -1 || errno != ENOSYS) {
         return r;
      }
      uid32 = 0;
   }
   return syscall(SYS_setuid, euid);
#else
#   error "Id_SetUid is not implemented for this platform"
#endif
}
#endif


/*
 *----------------------------------------------------------------------------
 *
 * Id_SetGid --
 *
 *      If calling thread has euid = 0, it sets real, effective and saved gid
 *	to the specified value.
 *	If calling thread has euid != 0, then only effective gid is set.
 *
 * Results:
 *      0 on success, -1 on failure, errno set
 *
 * Side effects:
 *      errno may be modified on success
 *
 *----------------------------------------------------------------------------
 */

int
Id_SetGid(gid_t egid)		// IN: new egid
{
#if defined(__APPLE__)
   Warning("XXXMACOS: implement %s\n", __func__);
   return -1;
#elif defined(sun) || defined(__FreeBSD__)
   return setgid(egid);
#else
   if (uid32) {
      int r = syscall(SYS_setgid32, egid);
      if (r != -1 || errno != ENOSYS) {
         return r;
      }
      uid32 = 0;
   }
   return syscall(SYS_setgid, egid);
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * Id_SetRESUid --
 *
 *      Sets uid, euid and saved uid to the specified values.  You can use -1
 *	for values which should not change.
 *
 * Results:
 *      0 on success, -1 on failure, errno set
 *
 * Side effects:
 *      errno may be modified on success
 *
 *----------------------------------------------------------------------------
 */

int
Id_SetRESUid(uid_t uid,		// IN: new uid
	     uid_t euid,	// IN: new effective uid
	     uid_t suid)	// IN: new saved uid
{
#if (defined(__FreeBSD__) && __FreeBSD_version >= 500043)
   return setresuid(uid, euid, suid);
#elif defined(linux)
   if (uid32) {
      int r = syscall(SYS_setresuid32, uid, euid, suid);
      if (r != -1 || errno != ENOSYS) {
         return r;
      }
      uid32 = 0;
   }
   return syscall(SYS_setresuid, uid, euid, suid);
#else
   Warning("XXX: implement %s\n", __func__);
   return -1;
#endif
}


#if !defined(__APPLE__)
/*
 *----------------------------------------------------------------------------
 *
 * Id_SetRESGid --
 *
 *      Sets gid, egid and saved gid to the specified values.  You can use -1
 *      for values which should not change.
 *
 * Results:
 *      0 on success, -1 on failure, errno set
 *
 * Side effects:
 *      errno may be modified on success
 *
 *----------------------------------------------------------------------------
 */

int
Id_SetRESGid(gid_t gid,		// IN: new gid
	     gid_t egid,	// IN: new effective gid
	     gid_t sgid)	// IN: new saved gid
{
#if (defined(__FreeBSD__) && __FreeBSD_version >= 500043)
   return setresgid(gid, egid, sgid);
#elif defined(linux)
   if (uid32) {
      int r = syscall(SYS_setresgid32, gid, egid, sgid);
      if (r != -1 || errno != ENOSYS) {
         return r;
      }
      uid32 = 0;
   }
   return syscall(SYS_setresgid, gid, egid, sgid);
#else
   Warning("XXX: implement %s\n", __func__);
   return -1;
#endif
}
#endif


/*
 *----------------------------------------------------------------------------
 *
 * Id_SetREUid --
 *
 *      Sets uid and euid to the specified values.  You can use -1
 *      for values which should not change.  If you are changing uid,
 *      or if you are changing euid to value which differs from old uid,
 *      then saved uid is updated to new euid value.
 *
 * Results:
 *      0 on success, -1 on failure, errno set
 *
 * Side effects:
 *      errno may be modified on success
 *
 *----------------------------------------------------------------------------
 */

int
Id_SetREUid(uid_t uid,		// IN: new uid
	    uid_t euid)		// IN: new effective uid
{
#if defined(__APPLE__)
   Warning("XXXMACOS: implement %s\n", __func__);
   return -1;
#elif defined(sun) || defined(__FreeBSD__)
   return setreuid(uid, euid);
#else
   if (uid32) {
      int r = syscall(SYS_setreuid32, uid, euid);
      if (r != -1 || errno != ENOSYS) {
         return r;
      }
      uid32 = 0;
   }
   return syscall(SYS_setreuid, uid, euid);
#endif
}


#if !defined(__APPLE__)
/*
 *----------------------------------------------------------------------------
 *
 * Id_SetREGid --
 *
 *      Sets gid and egid to the specified values.  You can use -1
 *      for values which should not change.  If you are changing gid,
 *      or if you are changing egid to value which differs from old gid,
 *      then saved gid is updated to new egid value.
 *
 * Results:
 *      0 on success, -1 on failure, errno set
 *
 * Side effects:
 *      errno may be modified on success
 *
 *----------------------------------------------------------------------------
 */

int
Id_SetREGid(gid_t gid,		// IN: new gid
	    gid_t egid)		// IN: new effective gid
{
#if defined(sun) || defined(__FreeBSD__)
   return setregid(gid, egid);
#else
   if (uid32) {
      int r = syscall(SYS_setregid32, gid, egid);
      if (r != -1 || errno != ENOSYS) {
         return r;
      }
      uid32 = 0;
   }
   return syscall(SYS_setregid, gid, egid);
#endif
}
#endif


#if defined(__APPLE__)
/*
 *----------------------------------------------------------------------------
 *
 * Id_SetSuperUser --
 *
 *      If the calling process does not have euid root, do nothing.
 *      If the calling process has euid root, make the calling thread acquire
 *      or release euid root.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

void
Id_SetSuperUser(Bool yes) // IN: TRUE to acquire super user, FALSE to release
{
   if (!IsSuperUser() == !yes) {
      // settid(2) fails on spurious transitions.
      return;
   }

   if (yes) {
      syscall(SYS_settid, KAUTH_UID_NONE, KAUTH_GID_NONE /* Ignored. */);
   } else {
      if (syscall(SYS_settid, getuid(), getgid()) == -1) {
         Log("Failed to release super user privileges.\n");
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * IdAuthCreate --
 *
 *      Create an Authorization session.
 *
 *      An Authorization session remembers which process name and which
 *      credentials created it, and how much time has elapsed since it last
 *      prompted the user at the console to authenticate to grant the
 *      Authorization session a specific right.
 *
 * Results:
 *      On success: A ref to the Authorization session.
 *      On failure: NULL.
 *
 * Side effects:
 *      See IdAuthCreateWithFork.
 *
 *-----------------------------------------------------------------------------
 */

static AuthorizationRef
IdAuthCreate(void)
{
   /*
    * Bug 195868: If thread credentials are in use, we need to fork.
    * Otherwise, avoid forking, as it breaks Apple's detection of
    * whether the calling process is a GUI process.
    *
    * This is needed because AuthorizationRefs created by GUI
    * processes can be passed to non-GUI processes to allow them to
    * prompt for authentication with a dialog that automatically
    * steals focus from the current GUI app.  (This is how the Mac UI
    * and VMX work today.)
    *
    * TODO: How should we handle single-threaded apps where uid !=
    * euid or gid != egid?  Some callers may expect us to check
    * against euid, others may expect us to check against uid.
    */
   uid_t thread_uid;
   gid_t thread_gid;
   int ret;

   ret = syscall(SYS_gettid, &thread_uid, &thread_gid);

   if (ret != -1) {
      /*
       * We have per-thread UIDs in use, so Apple's authorization
       * APIs don't work.  Fork so we can use them.
       */
      return IdAuthCreateWithFork();
   } else {
      if (errno != ESRCH) {
         Warning("%s: gettid failed, error %d.\n", __FUNCTION__, errno);
         return NULL;
      } else {
          // Per-thread identities are not in use in this thread.
         AuthorizationRef auth;
         OSStatus ret;

         ret = AuthorizationCreate(NULL,
                                   kAuthorizationEmptyEnvironment,
                                   kAuthorizationFlagDefaults,
                                   &auth);

         if (ret == errAuthorizationSuccess) {
            return auth;
         } else {
            Warning("%s: AuthorizationCreate failed, error %ld.\n",
                    __FUNCTION__, ret);
            return NULL;
         }
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * IdAuthCreateWithFork --
 *
 *      Create an Authorization session.
 *
 *      An Authorization session remembers which process name and which
 *      credentials created it, and how much time has elapsed since it last
 *      prompted the user at the console to authenticate to grant the
 *      Authorization session a specific right.
 *
 * Results:
 *      On success: A ref to the Authorization session.
 *      On failure: NULL.
 *
 * Side effects:
 *      The current process is forked.
 *
 *-----------------------------------------------------------------------------
 */

static AuthorizationRef
IdAuthCreateWithFork(void)
{
   int fds[2] = { -1, -1, };
   pid_t child;
   AuthorizationRef auth = NULL;
   struct {
      Bool success;
      AuthorizationExternalForm ext;
   } data;
   uint8 buf;

   /*
    * XXX One more Apple bug related to thread credentials:
    *     AuthorizationCreate() incorrectly uses process instead of thread
    *     credentials. So for this code to properly work in the VMX for
    *     example, we must do this elaborate fork/handshake dance. Fortunately
    *     this function is only called once very early when a process starts.
    */

   if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
      Warning("%s: socketpair() failed.\n", __func__);
      goto out;
   }

   child = fork();
   if (child < 0) {
      Warning("%s: fork() failed.\n", __func__);
      goto out;
   }

   if (child) {
      size_t rcvd;
      int status;
      pid_t result;

      // Parent: use fds[0]

      // Wait until the child has created its process ref to the auth session.
      for (rcvd = 0; rcvd < sizeof data; ) {
         ssize_t actual;

         actual = read(fds[0], (void *)&data + rcvd, sizeof data - rcvd);
         ASSERT(actual <= sizeof data - rcvd);
         if (actual < 0) {
            ASSERT(errno == EPIPE);
            Warning("%s: parent read() failed because child died.\n",
                    __func__);
            data.success = FALSE;
            break;
         }

         rcvd += actual;
      }

      if (data.success) {
         if (AuthorizationCreateFromExternalForm(&data.ext, &auth)
             != errAuthorizationSuccess) {
            Warning("%s: parent AuthorizationCreateFromExternalForm() "
                    "failed.\n", __func__);
         }
      }

      // Tell the child it can now destroy its process ref to the auth session.
      write(fds[0], &buf, sizeof buf);

      // Reap the child, looping if we get interrupted by a signal.
      do {
         result = waitpid(child, &status, 0);
      } while (result == -1 && errno == EINTR);

      ASSERT_NOT_IMPLEMENTED(result == child);
   } else {
      // Child: use fds[1]

      data.success = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment,
                                         kAuthorizationFlagDefaults, &auth)
                     == errAuthorizationSuccess;
      if (data.success) {
         data.success = AuthorizationMakeExternalForm(auth, &data.ext)
                        == errAuthorizationSuccess;
         if (!data.success) {
            Warning("%s: child AuthorizationMakeExternalForm() failed.\n",
                    __func__);
         }
      } else {
         Warning("%s: child AuthorizationCreate() failed.\n", __func__);
      }

      // Tell the parent it can now create a process ref to the auth session.
      if (write(fds[1], &data, sizeof data) == sizeof data) {
         /*
          * Wait until the child can destroy its process ref to the auth
          * session.
          */
         for (;;) {
            ssize_t actual;

            actual = read(fds[1], &buf, sizeof buf);
            ASSERT(actual <= sizeof buf);
            if (actual) {
               break;
            }
         }
      }

      /*
       * This implicitly:
       * o Destroys the child process ref to the Authorization session.
       * o Closes fds[0] and fds[1]
       */
      exit(0);
   }

out:
   close(fds[0]);
   close(fds[1]);

   return auth;
}


static Atomic_Ptr procAuth = { 0 };


/*
 *-----------------------------------------------------------------------------
 *
 * IdAuthGet --
 *
 *      Get a ref to the process' Authorization session.
 *
 * Results:
 *      On success: The ref.
 *      On failure: NULL.
 *
 * Side effects:
 *      If the process' Authorization session does not exist yet, it is
 *      created.
 *
 *-----------------------------------------------------------------------------
 */

static AuthorizationRef
IdAuthGet(void)
{
   if (UNLIKELY(Atomic_ReadPtr(&procAuth) == NULL)) {
      AuthorizationRef newProcAuth = IdAuthCreate();

      if (Atomic_ReadIfEqualWritePtr(&procAuth,
                                     NULL,
                                     newProcAuth)) {
         // Someone else snuck in before we did.  Free the new authorization.
         AuthorizationFree(newProcAuth, kAuthorizationFlagDefaults);
      }
   }

   ASSERT(Atomic_ReadPtr(&procAuth) != NULL);

   return Atomic_ReadPtr(&procAuth);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Id_AuthGetLocal --
 *
 *      Get a local ref to the process' Authorization session.
 *
 * Results:
 *      On success: The ref.
 *      On failure: NULL.
 *
 * Side effects:
 *      If the process' Authorization session does not exist yet, it is
 *      created.
 *
 *-----------------------------------------------------------------------------
 */

void *
Id_AuthGetLocal(void)
{
   return (void *)IdAuthGet();
}


/*
 *-----------------------------------------------------------------------------
 *
 * Id_AuthGetExternal --
 *
 *      Get a cross-process ref to the process' Authorization session.
 *
 * Results:
 *      On success: An allocated cross-process ref.
 *      On failure: NULL.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void *
Id_AuthGetExternal(size_t *size) // OUT
{
   AuthorizationRef auth;
   AuthorizationExternalForm *ext;

   auth = IdAuthGet();
   if (!auth) {
      return NULL;
   }

   ext = malloc(sizeof *ext);
   if (!ext) {
      Warning("Unable to allocate an AuthorizationExternalForm.\n");
      return NULL;
   }

   if (AuthorizationMakeExternalForm(auth, ext) != errAuthorizationSuccess) {
      Warning("AuthorizationMakeExternalForm() failed.\n");
      free(ext);
      return NULL;
   }

   *size = sizeof *ext;
   return ext;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Id_AuthSet --
 *
 *      Set the process' Authorization session to the Authorization session
 *      referred to by a cross-process ref.
 *
 * Results:
 *      On success: TRUE.
 *      On failure: FALSE.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Id_AuthSet(void const *buf, // IN
           size_t size)     // IN
{
   AuthorizationRef newProcAuth;

   AuthorizationExternalForm const *ext =
      (AuthorizationExternalForm const *)buf;

   if (!buf || size != sizeof *ext) {
      Warning("%s: Invalid argument.\n", __func__);
      return FALSE;
   }

   ASSERT(!Atomic_ReadPtr(&procAuth));
   if (AuthorizationCreateFromExternalForm(ext, &newProcAuth)
       != errAuthorizationSuccess) {
      Warning("AuthorizationCreateFromExternalForm failed.\n");
      return FALSE;
   }

   if (Atomic_ReadIfEqualWritePtr(&procAuth,
                                  NULL,
                                  newProcAuth)) {
      /*
       * This is meant to be called very early on in the life of the
       * process.  If someone else has snuck in an authorization,
       * we're toast.
       */
      NOT_IMPLEMENTED();
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Id_AuthCheck --
 *
 *      Check if 'right' is granted to the process' Authorization
 *      session, using the specified localized UTF-8 description as
 *      the prompt if it's non-NULL.
 *
 * Results:
 *      TRUE if the right was granted, FALSE if the user cancelled,
 *      entered the wrong password three times in a row, or if an
 *      error was encountered.
 *
 * Side effects:
 *      Displays a dialog to the user.  The dialog grabs keyboard
 *      focus if Id_AuthSet() was previously called with a
 *      cross-process ref to a GUI process.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Id_AuthCheck(char const *right,                // IN
             char const *localizedDescription) // IN: UTF-8
{
   AuthorizationRef auth;
   AuthorizationItem rightsItems[1] = { { 0 } };
   AuthorizationRights rights;
   AuthorizationItem environmentItems[1] = { { 0 } };
   AuthorizationEnvironment environmentWithDescription = { 0 };
   const AuthorizationEnvironment *environment =
      kAuthorizationEmptyEnvironment;

   auth = IdAuthGet();
   if (!auth) {
      return FALSE;
   }

   rightsItems[0].name = right;
   rights.items = rightsItems;
   rights.count = ARRAYSIZE(rightsItems);

   /*
    * By default, the API displays a dialog saying "APPLICATIONNAME
    * requires that you type your password" (if you're an admin; the
    * message is different if you're not).
    *
    * If the localized description is present, the API uses that
    * description and appends a space followed by the above string.
    */
   if (localizedDescription) {
      environmentItems[0].name = kAuthorizationEnvironmentPrompt;
      environmentItems[0].valueLength = strlen(localizedDescription);
      environmentItems[0].value = (void *)localizedDescription;
      environmentWithDescription.items = environmentItems;
      environmentWithDescription.count = ARRAYSIZE(environmentItems);
      environment = &environmentWithDescription;
   }

   /*
    * TODO: Is this actually thread-safe when multiple threads act on
    * the same AuthorizationRef?  Apple's documentation doesn't
    * actually say whether it is or is not.
    */
   return AuthorizationCopyRights(auth, &rights,
             environment,
             kAuthorizationFlagDefaults |
             kAuthorizationFlagInteractionAllowed |
             kAuthorizationFlagExtendRights,
             NULL) == errAuthorizationSuccess;
}

#endif
