/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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
 * hgfsServerPolicyGuest.c --
 *
 *     Implementation of access policy for hgfs server running in a
 *     VM. All access is allowed.
 */

#ifdef sun
#   include <stdlib.h>
#   include <strings.h>
#elif defined(__FreeBSD__)
#   include <stdlib.h>
#endif

#undef LOG

#define LGLEVEL         (10)
#define LGPFX_FMT       "%s:%s:"
#define LGPFX           "hgfsd"

#if defined VMTOOLS_USE_GLIB
#define G_LOG_DOMAIN          LGPFX
#define Debug                 g_debug
#define Warning               g_warning
#else
#include "debug.h"
#endif

#define DOLOG(_min)     ((_min) <= LGLEVEL)

#define LOG(_level, args)                                  \
   do {                                                    \
      if (DOLOG(_level)) {                                 \
         Debug(LGPFX_FMT, LGPFX, __FUNCTION__);            \
         Debug args;                                       \
      }                                                    \
   } while (0)


#include "vmware.h"
#include "hgfsServerPolicy.h"


typedef struct HgfsServerPolicyState {
   /*
    * An empty list means that the policy server enforces the "deny all access
    * requests" policy --hpreg
    */
   DblLnkLst_Links shares;
} HgfsServerPolicyState;


static HgfsServerPolicyState myState;

static void *
HgfsServerPolicyEnumSharesInit(void);
static Bool
HgfsServerPolicyEnumSharesGet(void *data,
                              char const **name,
                              size_t *len,
                              Bool *done);
static Bool
HgfsServerPolicyEnumSharesExit(void *data);


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerPolicyDestroyShare --
 *
 *    Destroy the internal representation of a share --hpreg
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerPolicyDestroyShare(HgfsSharedFolder *share) // IN
{
   ASSERT(share);

   free(share);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerPolicyDestroyShares --
 *
 *    Destroy the internal representation of all shares. The function is
 *    idempotent --hpreg
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerPolicyDestroyShares(DblLnkLst_Links *head) // IN
{
   ASSERT(head);

   while (head->next != head) {
      HgfsSharedFolder *share;

      share = DblLnkLst_Container(head->next, HgfsSharedFolder, links);
      ASSERT(share);
      DblLnkLst_Unlink1(&share->links);
      HgfsServerPolicyDestroyShare(share);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerPolicy_Init --
 *
 *    Initialize the HGFS security server state.
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsServerPolicy_Init(HgfsInvalidateObjectsFunc invalidateObjects,  // Unused
                      HgfsServerResEnumCallbacks *enumResources)    // OUT enum callbacks
{
   HgfsSharedFolder *rootShare;

   /*
    * Currently these callbacks are not used, so make sure our caller doesn't pass
    * it in.
    */
   ASSERT(invalidateObjects == NULL);

   LOG(8, ("HgfsServerPolicy_Init: enter\n"));

   DblLnkLst_Init(&myState.shares);

   /* For the guest, we hard code a "root" share */
   rootShare = (HgfsSharedFolder *)malloc(sizeof *rootShare);
   if (!rootShare) {
      LOG(4, ("HgfsServerPolicy_Init: memory allocation failed\n"));
      return FALSE;
   }

   DblLnkLst_Init(&rootShare->links);

   /*
    * A path = "" has special meaning; it indicates that access is
    * granted to the root of the server filesystem, and in Win32
    * causes everything after the share name in the request to be
    * interpreted as either a drive letter or UNC name. [bac]
    */
   rootShare->path = "";
   rootShare->name = HGFS_SERVER_POLICY_ROOT_SHARE_NAME;
   rootShare->readAccess = TRUE;
   rootShare->writeAccess = TRUE;
   /* These are strictly optimizations to save work later */
   rootShare->pathLen = strlen(rootShare->path);
   rootShare->nameLen = strlen(rootShare->name);
   rootShare->handle = HGFS_INVALID_FOLDER_HANDLE;

   /* Add the root node to the end of the list */
   DblLnkLst_LinkLast(&myState.shares, &rootShare->links);

   /*
    * Fill the share enumeration callback table.
    */
   enumResources->init = HgfsServerPolicyEnumSharesInit;
   enumResources->get = HgfsServerPolicyEnumSharesGet;
   enumResources->exit = HgfsServerPolicyEnumSharesExit;

   LOG(8, ("HgfsServerPolicy_Init: exit\n"));
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerPolicy_Cleanup --
 *
 *    Cleanup the HGFS security server state.
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsServerPolicy_Cleanup(void)
{
   LOG(8, ("HgfsServerPolicy_Cleanup: enter\n"));
   HgfsServerPolicyDestroyShares(&myState.shares);

   LOG(8, ("HgfsServerPolicy_Cleanup: exit\n"));
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerPolicyGetShare --
 *
 *    Get the share whose name matches the given name (if any).
 *
 * Results:
 *    The share, if a match is found.
 *    NULL otherwise
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsSharedFolder *
HgfsServerPolicyGetShare(HgfsServerPolicyState *state, // IN
                         char const *nameIn,           // IN: Name to check
                         size_t nameInLen)             // IN: Length of nameIn
{
   DblLnkLst_Links *l;

   ASSERT(state);
   ASSERT(nameIn);

   /*
    * First try to find a share that matches the given name exactly.
    * This is to handle the case where 2 share names differ in case only.
    */

   for (l = state->shares.next; l != &state->shares; l = l->next) {
      HgfsSharedFolder *share;

      share = DblLnkLst_Container(l, HgfsSharedFolder, links);
      ASSERT(share);
      if (nameInLen == share->nameLen &&
          !memcmp(nameIn, share->name, nameInLen)) {
         return share;
      }
   }

   /*
    * There was no match. As a fall back try a case insensitive match.
    * This is because some Windows applications uppercase or lowercase the
    * entire path before sending the request.
    */

   for (l = state->shares.next; l != &state->shares; l = l->next) {
      HgfsSharedFolder *share;
      char *tempName;

      /*
       * Null terminate the input name before a case insensitive comparison.
       * This is just to protect against bad implementations of strnicmp.
       */

      if (!(tempName = (char *)malloc(nameInLen + 1))) {
         LOG(4, ("HgfsServerPolicyGetShare: couldn't allocate tempName\n"));
         return NULL;
      }

      memcpy(tempName, nameIn, nameInLen);
      tempName[nameInLen] = 0;

      share = DblLnkLst_Container(l, HgfsSharedFolder, links);
      ASSERT(share);
      if (nameInLen == share->nameLen &&
#ifdef _WIN32
          !strnicmp(tempName, share->name, nameInLen)) {
#else
          !strncasecmp(tempName, share->name, nameInLen)) {
#endif
         free(tempName);
         return share;
      }

      free(tempName);
   }

   return NULL;
}


/* State used by HgfsServerPolicyEnumSharesGet and friends */
typedef struct State {
   DblLnkLst_Links *next;
} GetSharesState;


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerPolicyEnumSharesInit --
 *
 *    Setup state for HgfsServerPolicyEnumSharesGet
 *
 * Results:
 *    Pointer to state on success.
 *    NULL on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void *
HgfsServerPolicyEnumSharesInit(void)
{
   GetSharesState *that;

   that = malloc(sizeof *that);
   if (!that) {
      LOG(4, ("HgfsServerPolicyEnumSharesInit: couldn't allocate state\n"));
      return NULL;
   }

   that->next = myState.shares.next;
   return that;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerPolicyEnumSharesGet --
 *
 *    Enumerate share names one at a time.
 *
 *    When finished, sets "done" to TRUE.
 *
 *    Should be called with the results obtained by calling
 *    HgfsServerPolicyEnumSharesInit.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure (never happens).
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsServerPolicyEnumSharesGet(void *data,        // IN:  Callback data
                              char const **name, // OUT: Share name
                              size_t *len,       // OUT: Name length
                              Bool *done)        // OUT: Completion status
{
   GetSharesState *that;
   HgfsSharedFolder *share;

   that = (GetSharesState *)data;
   ASSERT(that);
   ASSERT(name);
   ASSERT(len);
   ASSERT(done);

   if (that->next == &myState.shares) {
      /* No more shares */
      *done = TRUE;
      return TRUE;
   }

   share = DblLnkLst_Container(that->next, HgfsSharedFolder, links);
   ASSERT(share);
   that->next = share->links.next;
   *name = share->name;
   *len = share->nameLen;
   LOG(4, ("HgfsServerPolicyEnumSharesGet: Share name is \"%s\"\n",
           *name));
   *done = FALSE;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerPolicyEnumSharesExit --
 *
 *    Cleanup state from HgfsServerPolicyEnumSharesGet
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure (never happens).
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsServerPolicyEnumSharesExit(void *data) // IN: Callback data
{
   GetSharesState *that;

   that = (GetSharesState *)data;
   ASSERT(that);
   free(that);
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerPolicy_GetSharePath --
 *
 *    Get the local path for a share name by looking at the requested
 *    name, finding the matching share (if any), checking access
 *    permissions, and returning the share's local path.
 *
 * Results:
 *    An HgfsNameStatus value indicating the result is returned.
 *
 *    The local path for the shareName is also returned if a match is found and
 *    access is permitted.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsNameStatus
HgfsServerPolicy_GetSharePath(char const *nameIn,        // IN: Name to check
                              size_t nameInLen,          // IN: Length of nameIn
                              HgfsOpenMode mode,         // IN: Requested access mode
                              size_t *sharePathLen,      // OUT: Length of share path
                              char const **sharePath)    // OUT: Share path
{
   HgfsSharedFolder *myShare;

   ASSERT(nameIn);
   ASSERT(sharePathLen);
   ASSERT(sharePath);

   myShare = HgfsServerPolicyGetShare(&myState, nameIn, nameInLen);
   if (!myShare) {
      LOG(4, ("HgfsServerPolicy_GetSharePath: No matching share name\n"));
      return HGFS_NAME_STATUS_DOES_NOT_EXIST;
   }

   /*
    * See if access is allowed in the requested mode.
    *
    * XXX Yeah, this is retarded. We should be using bits instead of
    * an enum for HgfsOpenMode. Add it to the todo list. [bac]
    */
   switch (HGFS_OPEN_MODE_ACCMODE(mode)) {
   case HGFS_OPEN_MODE_READ_ONLY:
      if (!myShare->readAccess) {
         LOG(4, ("HgfsServerPolicy_GetSharePath: Read access denied\n"));
         return HGFS_NAME_STATUS_ACCESS_DENIED;
      }
      break;

   case HGFS_OPEN_MODE_WRITE_ONLY:
      if (!myShare->writeAccess) {
         LOG(4, ("HgfsServerPolicy_GetSharePath: Write access denied\n"));
         return HGFS_NAME_STATUS_ACCESS_DENIED;
      }
      break;

   case HGFS_OPEN_MODE_READ_WRITE:
      if (!myShare->readAccess || !myShare->writeAccess) {
         LOG(4, ("HgfsServerPolicy_GetSharePath: Read/write access denied\n"));
         return HGFS_NAME_STATUS_ACCESS_DENIED;
      }
      break;

   default:
      LOG(0, ("HgfsServerPolicy_GetSharePath: Invalid mode\n"));
      return HGFS_NAME_STATUS_FAILURE;
      break;
   }

   *sharePathLen = myShare->pathLen;
   *sharePath = myShare->path;
   return HGFS_NAME_STATUS_COMPLETE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerPolicy_ProcessCPName --
 *
 *    Get the local path for a share name by looking at the requested
 *    name, finding the matching share (if any) and returning the share's
 *    local path local path and permissions.
 *
 * Results:
 *    An HgfsNameStatus value indicating the result is returned.
 *
 *    The local path for the shareName is also returned if a match is found.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsNameStatus
HgfsServerPolicy_ProcessCPName(char const *nameIn,            // IN: name in CPName form
                               size_t nameInLen,              // IN: length of the name
                               Bool *readAccess,              // OUT: Read permissions
                               Bool *writeAccess,             // OUT: Write permissions
                               HgfsSharedFolderHandle *handle,// OUT: folder handle
                               char const **shareBaseDir)     // OUT: Shared directory
{
   HgfsSharedFolder *myShare;

   ASSERT(nameIn);
   ASSERT(shareBaseDir);

   myShare = HgfsServerPolicyGetShare(&myState, nameIn, nameInLen);
   if (!myShare) {
      LOG(4, ("%s: No matching share name\n", __FUNCTION__));
      return HGFS_NAME_STATUS_DOES_NOT_EXIST;
   }

   *readAccess = myShare->readAccess;
   *writeAccess = myShare->writeAccess;
   *shareBaseDir = myShare->path;
   *handle = myShare->handle;
   return HGFS_NAME_STATUS_COMPLETE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerPolicy_GetShareOptions --
 *
 *    Get the HGFS share config options by looking at the requested name,
 *    finding the matching share (if any).
 *
 * Results:
 *    HGFS_NAME_STATUS_COMPLETE on success, and HGFS_NAME_STATUS_DOES_NOT_EXIST
 *    if no matching share.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsNameStatus
HgfsServerPolicy_GetShareOptions(char const *nameIn,             // IN: Share name
                                 size_t nameInLen,               // IN: Share name length
                                 HgfsShareOptions *configOptions)// OUT: Config options
{
   HgfsSharedFolder *share;
   char const *inEnd;
   char *next;
   int len;

   ASSERT(nameIn);
   ASSERT(configOptions);

   inEnd = nameIn + nameInLen;
   len = CPName_GetComponent(nameIn, inEnd, (char const **) &next);
   if (len < 0) {
      LOG(4, ("HgfsServerPolicy_GetShareOptions: get first component failed\n"));
      return HGFS_NAME_STATUS_FAILURE;
   }

   share = HgfsServerPolicyGetShare(&myState, nameIn, len);
   if (!share) {
      LOG(4, ("HgfsServerPolicy_GetShareOptions: No matching share name.\n"));
      return HGFS_NAME_STATUS_DOES_NOT_EXIST;
   }
   *configOptions = share->configOptions;
   return HGFS_NAME_STATUS_COMPLETE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerPolicy_IsShareOptionSet --
 *
 *    Check if the specified config option is set.
 *
 * Results:
 *    TRUE if set.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsServerPolicy_IsShareOptionSet(HgfsShareOptions configOptions, // IN: config options
                                  uint32 option)                  // IN: option to check
{
   return (configOptions & option) == option;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerPolicy_GetShareMode --
 *
 *    Get the access mode for a share by looking at the requested
 *    name, finding the matching share (if any), and returning
 *    the share's access mode.
 *
 * Results:
 *    An HgfsNameStatus value indicating the result is returned.
 *
 *    The access mode for the shareName is also returned if a match is found.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsNameStatus
HgfsServerPolicy_GetShareMode(char const *nameIn,        // IN: Share name to retrieve
                              size_t nameInLen,          // IN: Length of Share name
                              HgfsOpenMode *mode)        // OUT: Share's access mode
{
   HgfsSharedFolder *share;

   ASSERT(nameIn);
   ASSERT(mode);

   share = HgfsServerPolicyGetShare(&myState, nameIn, nameInLen);
   if (!share) {
      LOG(4, ("HgfsServerPolicy_GetShareMode: No matching share name\n"));
      return HGFS_NAME_STATUS_DOES_NOT_EXIST;
   }

   /*
    * Get the access mode.
    */
   if (share->readAccess && share->writeAccess) {
      *mode = HGFS_OPEN_MODE_READ_WRITE;
   } else if (share->readAccess) {
      *mode = HGFS_OPEN_MODE_READ_ONLY;
   } else if (share->writeAccess) {
      *mode = HGFS_OPEN_MODE_WRITE_ONLY;
   } else {
      /* Share should be at least read or write access. */
      ASSERT(FALSE);
      LOG(4, ("HgfsServerPolicy_GetShareMode: Invalid access mode\n"));
      return HGFS_NAME_STATUS_FAILURE;
   }

   return HGFS_NAME_STATUS_COMPLETE;
}
