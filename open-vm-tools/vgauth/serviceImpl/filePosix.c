/*********************************************************
 * Copyright (C) 2011-2016 VMware, Inc. All rights reserved.
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

/**
* @file filePosix.c --
*
*    Posix file functions.  These are glib calls in most cases,
*    but we also need file perms and ownership, as well as Posix
*    errnos.
*/

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "VGAuthLog.h"

#include "serviceInt.h"


#if defined(sun) && defined(__i386__)
#define FMTUID "l"
#define FMTGID "l"
#define FMTMODE "l"
#else
#define FMTUID ""
#define FMTGID ""
#define FMTMODE ""
#endif


/*
 ******************************************************************************
 * ServiceFilePosixMakeTempfile --                                       */ /**
 *
 * Wrapper on g_mkstemp() that logs errno details.
 *
 * @param[in]   fileName       The file name.
 * @param[in]   mode           The file open modes/permissions.
 *
 * @return A new fd on success, -1 on error
 *
 ******************************************************************************
 */

int
ServiceFilePosixMakeTempfile(gchar *fileName,
                             int mode)
{
   int fd;

   /* TODO: Once all OSes use GLIB 2.22+, just use g_mkstemp_full(). */

   fd = g_mkstemp(fileName);
   if (fd < 0) {
      VGAUTH_LOG_ERR_POSIX("g_mkstemp(%s) failed", fileName);
      goto done;
   }

  /*
   * Make sure that g_mkstemp() doesn't create a file writeable by our group
   * or by others, since that could create a window for an attacker to modify
   * the file.
   */
#ifdef VMX86_DEVEL
   {
      struct stat st;

      if (fstat(fd, &st) == 0) {
         ASSERT(!(st.st_mode & S_IWGRP) && !(st.st_mode & S_IWOTH));
      } else {
         VGAUTH_LOG_ERR_POSIX("Failed to stat temp file %s!", fileName);
      }
   }
#endif

   if (fchmod(fd, mode) != 0) {
      VGAUTH_LOG_ERR_POSIX("Failed to change ownership of %s", fileName);
      close(fd);
      fd = -1;
      goto done;
   }

done:
   return fd;
}


/*
 ******************************************************************************
 * ServiceFileSetOwner --                                                */ /**
 *
 * Changes the file to be owned by userName.
 *
 * @param[in]   fileName       The file name.
 * @param[in]   userName       The user to become owner of the file.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceFileSetOwner(const gchar *fileName,
                   const gchar *userName)
{
   VGAuthError err;
   uid_t uid;
   gid_t gid;

   err = UsercheckLookupUser(userName, &uid, &gid);
   if (err != VGAUTH_E_OK) {
      Warning("%s: Unable to look up userinfo to change ownership of '%s' to '%s'\n",
              __FUNCTION__, fileName, userName);
      return err;
   }

   if (chown(fileName, uid, gid) < 0) {
      Warning("%s: chown() failed, %d\n", __FUNCTION__, errno);
      return VGAUTH_E_PERMISSION_DENIED;
   }

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * ServiceFileCopyOwnership --                                           */ /**
 *
 * Changes dstFilename to have the same ownership as srcFilename.
 *
 * @param[in]   srcFilename       The source file name.
 * @param[in]   dstFilename       The name of the file to change.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceFileCopyOwnership(const gchar *srcFilename,
                         const gchar *dstFilename)
{
   VGAuthError err = VGAUTH_E_OK;
   uid_t uid;
   gid_t gid;
   int ret;
   struct stat stbuf;  // XXX docs say GStatBuf, but what we have
                       // in the toolchain uses this older format.

   ret = g_lstat(srcFilename, &stbuf);
   if (ret < 0) {
      VGAUTH_ERROR_SET_SYSTEM_ERRNO(err, errno);
      Warning("%s: g_lstat() failed on '%s', %d\n",
              __FUNCTION__, srcFilename, errno);
      goto done;
   } else {
      uid = stbuf.st_uid;
      gid = stbuf.st_gid;
   }

   if (chown(dstFilename, uid, gid) < 0) {
      Warning("%s: chown() failed, %d\n", __FUNCTION__, errno);
      err = VGAUTH_E_PERMISSION_DENIED;
   }

done:
   return err;
}


/*
 ******************************************************************************
 * ServiceFileMakeDirTree --                                             */ /**
 *
 * Wrapper on g_mkdir_with_parents() that logs errno details.
 *
 * @param[in]   fileName       The file name.
 * @param[in]   mode           The file open modes/permissions.
 *
 * @return 0 on success, -1 on error
 *
 ******************************************************************************
 */

int
ServiceFileMakeDirTree(const gchar *dirName,
                      int mode)
{
   int ret;

   ret = g_mkdir_with_parents(dirName, mode);
   if (ret < 0) {
      Warning("%s: g_mkdir_with_parents(%s, 0%o) failed (%d)\n",
              __FUNCTION__, dirName, mode, errno);
   }

   /*
    * Potential security issue here.  If the directory tree already exists
    * and doesn't have the desired perms, we could leave the certstore
    * wide open.  Any caller may want to use ServiceFileSetPermissions()
    * if the path is sensitive.
    */
   return ret;
}


/*
 ******************************************************************************
 * ServiceFileSetPermissions --                                          */ /**
 *
 * Sets the permissions on a file.
 *
 * @param[in]   fileName       The file name.
 * @param[in]   mode           The file permissions.
 *
 * @return 0 on success, -1 on error
 *
 ******************************************************************************
 */

int
ServiceFileSetPermissions(const gchar *fileName,
                         int mode)
{
   int ret;

   ret = chmod(fileName, mode);
   if (ret < 0) {
      Warning("%s: chmod() failed, %d\n", __FUNCTION__, errno);
   }

   return ret;
}


/*
 ******************************************************************************
 * ServiceFileGetPermissions --                                          */ /**
 *
 * Gets the permissions on a file.
 *
 * @param[in]   fileName       The file name.
 * @param[out]  mode           The file permissions.
 *
 * @return 0 on success, -1 on error
 *
 ******************************************************************************
 */

int
ServiceFileGetPermissions(const gchar *fileName,
                         int *mode)
{
   int ret;
   struct stat stbuf;  // XXX docs say GStatBuf, but what we have
                       // in the toolchain uses this older format.

   ret = g_lstat(fileName, &stbuf);
   if (ret < 0) {
      Warning("%s: g_lstat() failed on '%s', %d\n", __FUNCTION__, fileName, errno);
   } else {
      *mode = stbuf.st_mode;
   }

   return ret;
}


/*
 ******************************************************************************
 * ServiceFileVerifyFileOwnerAndPerms --                                 */ /**
 *
 * Validates the owner and permissions of the given file.
 *
 * If the user cannot be looked up, and the uid of the file can also not
 * be looked up, assume the user has been removed or we have a network
 * user issue, and ignore the ownership check.
 *
 * Returns the uids found for subsequent sanity checks.
 *
 * @param[in]   fileName       The file name.
 * @param[in]   userName       The expected owner.
 * @param[in]   mode           The expected file permissions.
 * @param[out]  uid            The actual uid of the file.
 * @param[out]  gid            The actual gid of the file.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceFileVerifyFileOwnerAndPerms(const char *fileName,
                                   const char *userName,
                                   int mode,
                                   uid_t *uidRet,
                                   gid_t *gidRet)
{
   int ret;
   struct stat stbuf;  // XXX docs say GStatBuf, but what we have
                      // in the toolchain uses this older format.
   VGAuthError err;
   uid_t uid;
   gid_t gid;

   ret = g_lstat(fileName, &stbuf);
   if (ret < 0) {
      Warning("%s: g_lstat() failed on '%s', %d\n", __FUNCTION__, fileName, errno);
      return VGAUTH_E_FAIL;
   }

   err = UsercheckLookupUser(userName, &uid, &gid);
   if (err != VGAUTH_E_OK) {
      gchar *uidUserName = NULL;

      Warning("%s: Unable to look up userinfo of '%s' to check ownership of '%s'\n",
          __FUNCTION__, userName, fileName);

      /*
       * We can't find the user.  But we don't know if its because they no
       * longer exist, or if they are just unavailable (eg NIS can't be
       * reached).  So check the uid on the file.  If that has no
       * match, we can be pretty sure the file is OK but the user is not
       * accessible.
       *
       * Note that NIS may come back between the two calls.  If it does,
       * we trust the uid->name comparison.
       */

      err = UsercheckLookupUid(stbuf.st_uid, &uidUserName);
      if (VGAUTH_E_OK == err) {
         /*
          * We got a match, check the name -- maybe NIS came back.
          */
         if (g_strcmp0(uidUserName, userName) != 0) {
            /*
             * Name mis-match.  Suspected hack attempt.
             */
            Warning("%s: Unable to look up userinfo of '%s' to check ownership "
                    "of '%s', but found valid entry for uid %"FMTUID"d\n",
                    __FUNCTION__, userName, fileName, stbuf.st_uid);
            g_free(uidUserName);
            return VGAUTH_E_SECURITY_VIOLATION;
         } else {
            Warning("%s: username '%s' lookup failed, but found uid %"FMTUID"d -- "
                    "temp NIS outage?\n", __FUNCTION__, userName, stbuf.st_uid);
         }
      } else {
         Warning("%s: failed to look up uid %"FMTUID"d; assuming user is deleted or "
                 "NIS is inaccessible\n", __FUNCTION__, stbuf.st_uid);
      }
      g_free(uidUserName);

      /*
       * If we can't lookup by name, assume deleted or unavailable user and
       * continue.
       * Set the uid/gid so they'll match and it will then do the
       * perm check.
       */
      uid = stbuf.st_uid;
      gid = stbuf.st_gid;
   }

   if (uid != stbuf.st_uid) {
      Warning("%s: uid mismatch for %s (want %"FMTUID"d, found %"FMTUID"d)\n",
              __FUNCTION__, fileName, uid, stbuf.st_uid);
      return VGAUTH_E_SECURITY_VIOLATION;
   }

   if (gid != stbuf.st_gid) {
      Warning("%s: gid mismatch for %s (want %"FMTGID"d, found %"FMTGID"d)\n",
              __FUNCTION__, fileName, gid, stbuf.st_gid);
      return VGAUTH_E_SECURITY_VIOLATION;
   }

   if (mode != (stbuf.st_mode & 0777)) {
      Warning("%s: file permission mismatch for %s (want 0%o, found "
              "0%"FMTMODE"o)\n", __FUNCTION__, fileName, mode, stbuf.st_mode);
      return VGAUTH_E_SECURITY_VIOLATION;
   }

   if (uidRet) {
      *uidRet = uid;
   }
   if (gidRet) {
      *gidRet = gid;
   }

   return VGAUTH_E_OK;
}
