/*********************************************************
 * Copyright (c) 2011-2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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
 * @file usercheck.c --
 *
 *    Helper functions to check user existence.
 */

#include "usercheck.h"
#include "VGAuthLog.h"

#ifndef _WIN32
#include <stdio.h>
#include <errno.h>
#endif

#ifdef _WIN32
#include <Lm.h>
#pragma comment(lib, "netapi32.lib")
#endif

#include <string.h>

/*
 * XXX
 *
 * Lost connection issue with LDAP.
 *
 * On my dev Linux box (Redhat 5.6), the underlying username lookup
 * code will get an LDAP connection and hold it.  But the LDAP server
 * supports an 'idletimeout' feature, where it kicks off clients
 * that stop talking to it.  For me, that looks to be 3-4 minutes.
 * On the next username resolution attempt, the client side discovers
 * the TCP connection is gone when a send() fails.  But instead of
 * reconnecting and retrying, the underlying code just returns the EBADF
 * it got from the failed send().  But the next call works fine, since the
 * client code knows its connection is gone and reestablishes it.
 *
 *
 * The end result of this is that a user can do a VGAuth operation,
 * wait 4 minutes, try again, and be told the user doesn't exist.
 *
 *
 * Timeouts are controlled on the LDAP server side, and while they
 * aren't on by default for OpenLDAP, they're probably going to be on in
 * many cases, since otherwise the poor LDAP server can have huge numbers of
 * idle connections sucking resources.  So we can't expect customers
 * to not have timeouts.
 *
 * Another posible fix would be to essentially send our own keep-alives,
 * but this puts that stress back on the LDAP server.
 *
 * Another solution is to add "nss_connect_policy oneshot" to
 * /etc/ldap.conf.  This tells the client code to not keep its
 * connection open.  But we can't expect a customer to fix things
 * by changing their LDAP configuration.
 *
 * So the safe fix is to do the retry at our layer.
 *
 *
 * XXX Right now this is just on for Linux.  We may need it for
 * Solaris as well, but that path is untested.
 */

#define MAX_USER_NAME_LEN 256

/*
 * A single retry works for the LDAP case, but try more often in case NIS
 * or something else has a related issue.  Note that a bad username/uid won't
 * give the EBADF so we won't retry 'expected' failures.
 */
#define MAX_RETRIES  5

#ifndef _WIN32
/*
 * ******************************************************************************
 * UsercheckRetryGetpwuid_r --                                             */ /**
 *
 * Wrapper for calls to getpwuid_r().
 *
 * Handle retryable errors from calls to getpwuid_r.
 *
 * @param[in]     uid, the user id to find.
 * @param[in/out] struct passwd to use.
 * @param[in/out] buf, buffer for the entry data.
 * @param[in]     bufLen, buffer size in bytes.
 * @param[out]    result, where to store the struct passwd address if found.
 * @param[in]     retry, the maximum number of retries:
 *                   < 0: use default (MAX_RETRIES),
 *                     0: no retries (do once),
 *                   > 0: use value for max retries
 *
 * @return On success (found), error is 0 and *result == ppw;
 *         On not found, error is 0 or ENOENT and *result == NULL;
 *         On error, error != 0 and *result == NULL.
 *
 *         In case retries occurred, the last attempt error and result values
 *         are returned.
 *
 * Note: retries not implemented for Sun OS.
 */

int
UsercheckRetryGetpwuid_r(const uid_t uid,
                         struct passwd *ppw,
                         char *buf,
                         const size_t bufLen,
                         struct passwd **result,
                         const int retry) {
#ifdef sun /* sun */
   /*
    * Retry loop for EBADF is not implemented for Sun OS.
    * Adapt the getpwuid_r call's outcome to match the expectation.
    * Use errno as the returned error value.
    */
   errno = 0;
   if ((ppw = getpwuid_r(uid, ppw, buf, bufLen)) == NULL) {
      /* Fall through, set *result to ppw (NULL), and return errno. */
   }
   *result = ppw;
   return errno;

#else /* !sun */
   int error;
   int maxRetries = retry < 0 ? MAX_RETRIES : retry;
   int retryCount = 0;
   int saveErrno;

   /*
    * Retry loop for EBADF.
    */
retry:
   saveErrno = errno;
   errno = 0; /* PR3105769 - reset errno before the call. Per man page */
   if ((error = getpwuid_r(uid, ppw, buf, bufLen, result)) != 0 ||
       !result) {
      /*
       * According to POSIX 1003.1-2003
       *   - On not found: error == 0 or ENOENT && result == NULL
       *   - On error:     error != 0 && result == NULL
       */
      if (EBADF == errno) {
         retryCount++;
         if (retryCount < maxRetries) {
            g_debug("%s: getpwuid_r(%lu) failed %d (%d) (was: %d), try #%d\n",
                  __FUNCTION__, (unsigned long int)uid, error, errno, saveErrno,
                  retryCount);
            g_thread_yield(); /* XXX: if adds too much delay, use g_usleep(X) */
            goto retry;
         }
         /* Else: fall through and return the last attempt error. */
         g_warning("%s: getpwuid_r(%lu) failed %d (%d) (was: %d), try #%d\n",
                 __FUNCTION__, (unsigned long int)uid, error, errno, saveErrno,
                 retryCount);
      }
      /* Else: fall through and return the current error. */
   }
   return error;
#endif /* !sun */
}


/*
 * ******************************************************************************
 * UsercheckRetryGetpwnam_r --                                             */ /**
 *
 * Wrapper for calls to getpwnam_r().
 *
 * Handle retryable errors from calls to getpwnam_r.
 *
 * @param[in]     name, the user name to find.
 * @param[in/out] struct passwd to use.
 * @param[in/out] buf, buffer for the entry data.
 * @param[in]     bufLen, buffer size in bytes.
 * @param[out]    result, where to store the struct passwd address if found.
 * @param[in]     retry, the maximum number of retries:
 *                   < 0: use default (MAX_RETRIES),
 *                     0: no retries (do once),
 *                   > 0: use value for max retries
 *
 * @return On success (found), error is 0 and *result == ppw;
 *         On not found, error is 0 or ENOENT and *result == NULL;
 *         On error, error != 0 and *result == NULL.
 *
 *         In case retries occurred, the last attempt error and result values
 *         are returned.
 *
 * Note: retries not implemented for Sun OS.
 */
int UsercheckRetryGetpwnam_r(const char *name,
                             struct passwd *ppw,
                             char *buf,
                             const size_t bufLen,
                             struct passwd **result,
                             const int retry) {
#ifdef sun /* sun */
   /*
    * Retry loop for EBADF is not implemented for Sun OS.
    * Adapt the getpwnam_r call's outcome to match the expectation.
    * Use errno as the returned error value.
    */
   errno = 0;
   if ((ppw = getpwnam_r(name, ppw, buf, bufLen)) == NULL) {
      /* Fall through, set *result to ppw (NULL), and return errno. */
   }
   *result = ppw;
   return errno;

#else /* !sun */
   int error;
   int maxRetries = retry < 0 ? MAX_RETRIES : retry;
   int retryCount = 0;
   int saveErrno;

   /*
    * Retry loop for EBADF.
    */
retry:
   saveErrno = errno;
   errno = 0; /* PR3105769 - reset errno before the call. Per man page */
   if ((error = getpwnam_r(name, ppw, buf, bufLen, result)) != 0 ||
       !result) {
      /*
       * According to POSIX 1003.1-2003
       *   - On not found: error == 0 or ENOENT && result == NULL
       *   - On error:     error != 0 && result == NULL
       */
      if (EBADF == errno) {
         retryCount++;
         if (retryCount < maxRetries) {
            g_debug("%s: getpwnam_r(%s) failed %d (%d) (was: %d), try #%d\n",
                  __FUNCTION__, name, error, errno, saveErrno, retryCount);
            g_thread_yield(); /* XXX: if adds too much delay, use g_usleep(X) */
            goto retry;
         }
         /* Else: fall through and return the last attempt error. */
         g_warning("%s: getpwnam_r(%s) failed %d (%d) (was: %d), try #%d\n",
                 __FUNCTION__, name, error, errno, saveErrno, retryCount);
      }
      /* Else: fall through and return the current error. */
   }
   return error;
#endif /* !sun */
}


/*
 ******************************************************************************
 * UsercheckLookupUser --                                                */ /**
 *
 * Returns the uid/gid of userName.
 * XXX locale issue lurking here.
 *
 * @param[in]   userName        The userName to look up.
 * @param[out]  uid             The uid of userName.
 * @param[out]  gid             The gid of userName.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
UsercheckLookupUser(const gchar *userName,
                    uid_t *uid,
                    gid_t *gid)
{
   struct passwd pw;
   struct passwd *ppw = &pw;
   char buffer[BUFSIZ];
   int error;

   error = UsercheckRetryGetpwnam_r(userName, &pw, buffer, sizeof buffer, &ppw,
                                    -1);
   if (error != 0 || !ppw) {
      return VGAUTH_E_NO_SUCH_USER;
   }

   *uid = ppw->pw_uid;
   *gid = ppw->pw_gid;

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * UsercheckLookupUid --                                                 */ /**
 *
 * Returns the username matching uid.
 *
 * @param[in]  uid             The uid to look up.
 * @param[out] userName        The userName of uid.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
UsercheckLookupUid(uid_t uid,
                   gchar **userName)
{
   struct passwd pw;
   struct passwd *ppw = &pw;
   char buffer[BUFSIZ];
   int error;

   error = UsercheckRetryGetpwuid_r(uid, &pw, buffer, sizeof buffer, &ppw, -1);
   if (error != 0 || !ppw) {
      return VGAUTH_E_NO_SUCH_USER;
   }

   // XXX locale issue lurking here
   *userName = g_strdup(ppw->pw_name);

   return VGAUTH_E_OK;
}
#endif   // !_WIN32


/*
 ******************************************************************************
 * UsercheckIDCheckUserExists --                                         */ /**
 *
 * Checks to see if user exists in OS.
 *
 * @param[in]   userName        The userName to look up.
 *
 * @return TRUE if exists, FALSE if not.
 *
 ******************************************************************************
 */

gboolean
UsercheckUserExists(const gchar *userName)
{
   gboolean result = TRUE;
#ifdef _WIN32
   PSID pSidUser = WinUtil_LookupSid(userName);

   if (!pSidUser) {
      result = FALSE;
   } else {
      g_free(pSidUser);
   }

#else
   uid_t uid;
   gid_t gid;
   VGAuthError err;

   err = UsercheckLookupUser(userName, &uid, &gid);
   if (VGAUTH_E_OK != err) {
      result = FALSE;
   }
#endif
   return result;
}


/*
 ******************************************************************************
 * Usercheck_CompareByName --                                            */ /**
 *
 * Determines whether two usernames refer to the same logical account.
 *
 * @param[in]  u1    A username in UTF-8.
 * @param[in]  u2    A username in UTF-8 to compare with u1.
 *
 ******************************************************************************
 */

gboolean
Usercheck_CompareByName(const char *u1,
                        const char *u2)
{
   gboolean res = FALSE;

#ifdef _WIN32
   PSID sid1;
   PSID sid2 = NULL;

   /*
    * Usernames in Windows are case-insensitive. However, doing a UTF-8
    * friendly case-insensitive comparison is complex and expensive (see
    * g_utf8_casefold()), so just look-up the SIDs for each name and compare
    * those.
    * TODO: Does this cause any issues in cases where the network is down?
    */

   sid1 = WinUtil_LookupSid(u1);
   if (NULL == sid1) {
      goto done;
   }

   sid2 = WinUtil_LookupSid(u2);
   if (NULL == sid2) {
      goto done;
   }

   res = EqualSid(sid1, sid2) != 0;

done:
   g_free(sid2);
   g_free(sid1);
#else
   if (g_strcmp0(u1, u2) == 0) {
      res = TRUE;
   } else {
      uid_t uid1, uid2;
      gid_t dummy;

      /*
       * On Linux, it is possible to have more than one username refer to
       * the same UID, and thus the same user. So, the right way to check
       * is to look up the UIDs for each name and compare those.
       */

      if (UsercheckLookupUser(u1, &uid1, &dummy) != VGAUTH_E_OK) {
         goto done;
      }

      if (UsercheckLookupUser(u2, &uid2, &dummy) != VGAUTH_E_OK) {
         goto done;
      }

      res = uid1 == uid2;

   done:
      ;
   }
#endif

   return res;
}


/*
 ******************************************************************************
 * Usercheck_UsernameIsLegal --                                          */ /**
 *
 * Checks to see if the userName contains any illegal characters.
 *
 * @param[in]   userName        The userName to check.
 *
 * @return TRUE if its legal.
 *
 ******************************************************************************
 */

gboolean
Usercheck_UsernameIsLegal(const gchar *userName)
{
   /*
    * This catches the stuff that will upset the fileSystem when the
    * username is used as part of the aliasStore filename.  Note
    * that this is not a complete list.
    *
    * Different Linux distros seem to add additonal restrictions. QE has found
    * the following are legal chars in usernames:
    *
    * - Windows:        _!(){}$%^&'
    * - Ubuntu 12.04:   _.+-
    * - Rhel 6.1:       _.-
    *
    * Note that Rhel has restricions beyond Ubuntu.
    *
    * The illegal character list appears to be:
    *
    * Windows      /\@[]:;|=,+*?<>"
    * Ubuntu       /\[]:;|=,*<>"!(){}?$@%^&'
    * Rhel         /\[]:;|=,*<>"!(){}?$@%^&'+
    *
    * Note that '\' is valid with a domain username; this is the
    * restricted list for local usernames.
    */
   size_t len;
   size_t i = 0;
   int backSlashCnt = 0;
   /*
    * As user names are used to generate its alias store file name/path, it
    * should not contain path traversal characters ('/' and '\').
    */
   char *illegalChars = "<>/\\";

   len = strlen(userName);
   if (len > MAX_USER_NAME_LEN) {
      return FALSE;
   }

   while ((i += strcspn(userName + i, illegalChars)) < len) {
      /*
       * One backward slash is allowed for domain\username separator.
       */
      if (userName[i] != '\\' || ++backSlashCnt > 1) {
         return FALSE;
      }
      ++i;
   }

   return TRUE;
}


#ifdef _WIN32
/*
 ******************************************************************************
 * Usercheck_IsAdminMember --                                          */ /**
 *
 * Checks to see if the userName is a member of the Admninistrator group.
 *
 * This is currently written to support only the local Administrator
 * group.
 *
 * @param[in]   userName        The userName to check.
 *
 * @return TRUE if its legal.
 *
 ******************************************************************************
 */

gboolean
Usercheck_IsAdminMember(const gchar *userName)
{
   NET_API_STATUS status;
   gunichar2 *userNameW = NULL;
   LOCALGROUP_MEMBERS_INFO_1 *groupList;
   DWORD entriesRead;
   DWORD totalEntries;
   DWORD i;
   gboolean bRetVal = FALSE;
   BOOL ok;
   DWORD lastError;
   WCHAR *accountW = NULL;
   WCHAR *domainW = NULL;
   DWORD accountChar2Needed = 0;
   DWORD domainChar2Needed = 0;
   SID_NAME_USE eUse;
   PSID pSid;

   /*
    * XXX Should this cache some (all?) of the returned data for a perf boost?
    * Or does that open up bugs (security or other) where it might
    * change while the service is running?  The name of the group changing
    * seems unlikely; member changing less so.
    */

   /*
    * To avoid localization issues, start with the Administrators group's SID,
    * and find the name to pass to NetLocalGroupGetMembers to get
    * the group members.
    */
   pSid = WinUtil_GroupAdminSid();

   ok = LookupAccountSidW(NULL, pSid, NULL, &accountChar2Needed,
                          NULL, &domainChar2Needed, &eUse);
   ASSERT(!ok);
   lastError = GetLastError();
   if (lastError != ERROR_INSUFFICIENT_BUFFER) {
      VGAUTH_LOG_ERR_WIN_CODE(lastError, "LookupAccountSidW() failed");
      goto done;
   }

   ASSERT(accountChar2Needed > 0);
   ASSERT(domainChar2Needed > 0);

   accountW = (WCHAR *) g_malloc0(accountChar2Needed * sizeof(WCHAR));
   domainW = (WCHAR *) g_malloc0(domainChar2Needed * sizeof(WCHAR));

   ok = LookupAccountSidW(NULL, pSid, accountW, &accountChar2Needed,
                          domainW, &domainChar2Needed, &eUse);
   if (!ok) {
      VGAUTH_LOG_ERR_WIN("LookupAccountSidW failed");
      goto done;
   }

   /*
    * Since the query is being done on the local server, the domain
    * return value shouldn't matter (and should be 'BUILTIN').
    */

   // get everything in one shot
   status = NetLocalGroupGetMembers(NULL,       // server name
                                    accountW,    // group name
                                    1,          // level
                                    (LPBYTE *) &groupList, // return buffer
                                    MAX_PREFERRED_LENGTH,   // get it all
                                    &entriesRead,
                                    &totalEntries,
                                    NULL);         // resume handle

   if (status != NERR_Success) {
      VGAUTH_LOG_ERR_WIN_CODE(status, "NetLocalGroupGetMembers() failed");
      goto done;
   }

   CHK_UTF8_TO_UTF16(userNameW, userName, goto done);
   for (i = 0; i < entriesRead; i++) {
#ifdef VMX86_DEBUG
      g_debug("%s: checking input %S against group member #%d %S\n",
              __FUNCTION__, userNameW, i, groupList[i].lgrmi1_name);
#endif
      if (_wcsicmp(userNameW, groupList[i].lgrmi1_name) == 0) {
         bRetVal = TRUE;
         goto done;
      }
   }

done:
   g_free(pSid);
   if (groupList) {
      NetApiBufferFree(groupList);
   }
   g_free(userNameW);
   g_free(accountW);
   g_free(domainW);

   return bRetVal;
}
#endif   // _WIN32
