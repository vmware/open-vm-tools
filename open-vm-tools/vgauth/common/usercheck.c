/*********************************************************
 * Copyright (C) 2011-2016,2019 VMware, Inc. All rights reserved.
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

/*
 * A single retry works for the LDAP case, but try more often in case NIS
 * or something else has a related issue.  Note that a bad username/uid won't
 * give the EBADF so we won't retry 'expected' failures.
 */
#define MAX_RETRIES  5

#ifndef _WIN32
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
#ifndef sun
   int ret;
   int retryCount = 0;

   /*
    * XXX Retry kludge -- see above.
    */
retry:
   if ((ret = getpwnam_r(userName, &pw, buffer, sizeof buffer, &ppw)) != 0 ||
                         !ppw) {
      if ((EBADF == errno) && (++retryCount < MAX_RETRIES)) {
         g_debug("%s: getpwnam_r(%s) failed %d (%d), try #%d\n",
                 __FUNCTION__, userName, ret, errno, retryCount);
         goto retry;
      }
      return VGAUTH_E_NO_SUCH_USER;
   }
#else
   if ((ppw = getpwnam_r(userName, &pw, buffer, sizeof buffer)) == NULL) {
      return VGAUTH_E_NO_SUCH_USER;
   }
#endif

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
#ifndef sun
   int error;
   int retryCount = 0;

   /*
    * XXX Retry kludge -- see above.
    */
retry:
   if ((error = getpwuid_r(uid, &pw, buffer, sizeof buffer, &ppw)) != 0 ||
       !ppw) {
      /*
       * getpwuid_r() and getpwnam_r() can return a 0 (success) but not
       * set the return pointer (ppw) if there's no entry for the user,
       * according to POSIX 1003.1-2003.
       */
      if ((EBADF == errno) && (++retryCount < MAX_RETRIES)) {
         g_debug("%s: getpwuid_r(%d) failed %d (%d), try #%d\n",
                 __FUNCTION__, uid, error, errno, retryCount);
         goto retry;
      }
      return VGAUTH_E_NO_SUCH_USER;
   }
#else
   if ((ppw = getpwuid_r(uid, &pw, buffer, sizeof buffer)) == NULL) {
      return VGAUTH_E_NO_SUCH_USER;
   }
#endif

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
    *          Note that '\' is valid with a domain username; this is
    *          the restricted list for local usernames.
    * Ubuntu       /\[]:;|=,*<>"!(){}?$@%^&'
    * Rhel         /\[]:;|=,*<>"!(){}?$@%^&'+
    *
    */
   size_t len;
#ifdef _WIN32
   // allow '\' in for Windows domain usernames
   char *illegalChars = "<>/";
#else
   char *illegalChars = "\\<>/";
#endif

   len = strlen(userName);
   if (strcspn(userName, illegalChars) != len) {
      return FALSE;
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
