/*********************************************************
 * Copyright (C) 2011-2017 VMware, Inc. All rights reserved.
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
 * @file impersonateLinux.c
 *
 * Linux impersonation APIs
 */

/*
 * Pull in setresuid()/setresgid() if possible.  Do this first, to
 * be sure we don't get unistd.h w/o _GNU_SOURCE defined.
 */
#define  _GNU_SOURCE
#include <unistd.h>

#if !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
#include <asm/param.h>
#include <locale.h>
#include <sys/stat.h>
#endif
#include <sys/types.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>

#include "VGAuthInt.h"


#if (__GLIBC__ == 2) && (__GLIBC_MINOR__ < 3)
#include <sys/syscall.h>
/*
 * Implements the setresuid and setresgid system calls (they are not wrapped
 * by glibc until 2.3.2).
 */

static inline int
setresuid(uid_t ruid,
          uid_t euid,
          uid_t suid)
{
   return syscall(SYS_setresuid, ruid, euid, suid);
}


static inline int
setresgid(gid_t ruid,
          gid_t euid,
          gid_t suid)
{
   return syscall(SYS_setresgid, ruid, euid, suid);
}
#endif


/*
 ******************************************************************************
 * VGAuthImpersonateImpl --                                              */ /**
 *
 * Does the real work to start impersonating the user represented by handle.
 *
 * Note that this will change the entire process on Linux to the
 * user represented by the VGAuthUserHandle (so it must be called by root).
 *
 * The effective uid/gid, $HOME, $USER and $SHELL are changed;
 * however, no $SHELL startup files are run, so you cannot assume that
 * other environment variables have been changed.
 *
 * @param[in]  ctx              The VGAuthContext.
 * @param[in]  handle           The handle representing the user to be
 *                              impersonated.
 * @param[in]  loadUserProfile  Unused parameter.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuthImpersonateImpl(VGAuthContext *ctx,
                      VGAuthUserHandle *handle,
                      UNUSED_PARAM(gboolean loadUserProfile))
{
   char buffer[BUFSIZ];
   struct passwd pw;
   struct passwd *ppw = &pw;
   gid_t root_gid;
   int error;
   int ret;

   if ((error = getpwuid_r(0, &pw, buffer, sizeof buffer, &ppw)) != 0 ||
       !ppw) {
      /*
       * getpwuid_r() and getpwnam_r() can return a 0 (success) but not
       * set the return pointer (ppw) if there's no entry for the user,
       * according to POSIX 1003.1-2003.
       */
      Warning("Failed to lookup root (%d)\n", error);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   root_gid = ppw->pw_gid;

   if ((error = getpwnam_r(handle->userName, &pw, buffer, sizeof buffer, &ppw)) != 0 ||
       !ppw) {
      Warning("Failed to lookup user '%s' (%d)\n", handle->userName, error);
      // XXX add VGAUTH_E_INVALIDUSER ???
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   // first change group
   ret = setresgid(ppw->pw_gid, ppw->pw_gid, root_gid);
   if (ret < 0) {
      Warning("Failed to setresgid() for user %s (%d)\n", handle->userName, errno);
      return VGAUTH_E_FAIL;
   }
   ret = initgroups(ppw->pw_name, ppw->pw_gid);
   if (ret < 0) {
      Warning("Failed to initgroups() for user %s (%d)\n", handle->userName, errno);
      goto failure;
   }
   // now user
   ret = setresuid(ppw->pw_uid, ppw->pw_uid, 0);
   if (ret < 0) {
      Warning("Failed to setresuid() for user %s (%d)\n", handle->userName, errno);
      goto failure;
   }

   // set env
   setenv("USER", ppw->pw_name, 1);
   setenv("HOME", ppw->pw_dir, 1);
   setenv("SHELL", ppw->pw_shell, 1);

   return VGAUTH_E_OK;

failure:
   // try to restore on error
   VGAuth_EndImpersonation(ctx);

   return VGAUTH_E_FAIL;
}


/*
 ******************************************************************************
 * VGAuthEndImpersonationImpl --                                         */ /**
 *
 * Ends the current impersonation, restoring the process to superUser,
 * and resetting $USER, $HOME and $SHELL.
 *
 * @param[in]  ctx        The VGAuthContext.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuthEndImpersonationImpl(VGAuthContext *ctx)
{
   char buffer[BUFSIZ];
   struct passwd pw;
   struct passwd *ppw = &pw;
   int error;
   int ret;

   if ((error = getpwuid_r(0, &pw, buffer, sizeof buffer, &ppw)) != 0 ||
       !ppw) {
      Warning("Failed to lookup root (%d)\n", error);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   // first change back user
   ret = setresuid(ppw->pw_uid, ppw->pw_uid, 0);
   if (ret < 0) {
      Warning("Failed to setresuid() for root (%d)\n", errno);
      return VGAUTH_E_FAIL;
   }

   // now group
   ret = setresgid(ppw->pw_gid, ppw->pw_gid, ppw->pw_gid);
   if (ret < 0) {
      Warning("Failed to setresgid() for root (%d)\n", errno);
      return VGAUTH_E_FAIL;
   }
   ret = initgroups(ppw->pw_name, ppw->pw_gid);
   if (ret < 0) {
      Warning("Failed to initgroups() for root (%d)\n", errno);
      return VGAUTH_E_FAIL;
   }

   // set env
   setenv("USER", ppw->pw_name, 1);
   setenv("HOME", ppw->pw_dir, 1);
   setenv("SHELL", ppw->pw_shell, 1);

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * VGAuth_GetCurrentUsername --                                          */ /**
 *
 * Returns the name of the current effective user.  Must be g_free()d.
 *
 * @return The name of the current user, NULL on failure.
 *
 ******************************************************************************
 */

gchar *
VGAuth_GetCurrentUsername(void)
{
   uid_t uid = geteuid();
   char buffer[BUFSIZ];
   struct passwd pw;
   struct passwd *ppw = &pw;
   int error;
   gchar *userName = NULL;

   if ((error = getpwuid_r(uid, &pw, buffer, sizeof buffer, &ppw)) != 0 ||
       !ppw) {
      Warning("Failed to look up username for current uid (%d)\n", error);
      return userName;
   }

   userName = g_strdup(ppw->pw_name);

   return userName;
}
