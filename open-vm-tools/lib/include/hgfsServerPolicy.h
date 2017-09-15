/*********************************************************
 * Copyright (C) 1998-2017 VMware, Inc. All rights reserved.
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

#ifndef _HGFS_SERVER_POLICY_H_
#define _HGFS_SERVER_POLICY_H_

#include "vm_basic_types.h"
#include "hgfs.h"
#include "dbllnklst.h"
#include "cpName.h"
#include "hgfsServer.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Name of share that corresponds to the root of the server's
 * filesystem.
 */
#define HGFS_SERVER_POLICY_ROOT_SHARE_NAME "root"


typedef uint32 HgfsShareOptions;

/*
 * Structure representing one shared folder. We maintain a list of
 * these to check accesses against.
 */
typedef struct HgfsSharedFolder {
   DblLnkLst_Links links;
   const char *name;     /* Name of share */
   const char *path;     /*
                          * Path of share in server's filesystem. Should
                          * not include final path separator.
                          */
   const char *shareTags;/* Tags associated with this share (comma delimited). */
   size_t shareTagsLen;  /* Length of shareTag string */
   size_t nameLen;       /* Length of name string */
   size_t pathLen;       /* Length of path string */
   Bool readAccess;      /* Read permission for this share */
   Bool writeAccess;     /* Write permission for this share */
   HgfsShareOptions configOptions; /* User-config options. */
   HgfsSharedFolderHandle handle;  /* Handle assigned by HGFS server
                                    * when the folder was registered with it.
                                    * Policy package keeps the context and returns
                                    * it along with other shared folder properties.
                                    * Keeping it here ensures consistent lookup all
                                    * properties of the shared folder which takes into
                                    * account such details like case sensitive/case
                                    * insensitive name lookup.
                                    */
} HgfsSharedFolder;

/* Per share user configurable options. */
#define HGFS_SHARE_HOST_DEFAULT_CASE  (1 << 0)
#define HGFS_SHARE_FOLLOW_SYMLINKS    (1 << 1)

typedef struct HgfsServerPolicy_ShareList {
   size_t count;
   char **shareNames;
} HgfsServerPolicy_ShareList;

Bool
HgfsServerPolicy_Init(HgfsInvalidateObjectsFunc invalidateObjects,
                      HgfsServerResEnumCallbacks *enumResources);

Bool
HgfsServerPolicy_Cleanup(void);

HgfsNameStatus
HgfsServerPolicy_GetSharePath(char const *nameIn,         // IN:
                              size_t nameInLen,           // IN:
                              HgfsOpenMode mode,          // IN:
                              size_t *sharePathLen,       // OUT:
                              char const **sharePath);    // OUT:
HgfsNameStatus
HgfsServerPolicy_GetShareMode(char const *nameIn,        // IN: Share name to retrieve
                              size_t nameInLen,          // IN: Length of Share name
                              HgfsOpenMode *mode);       // OUT: Share's access mode
HgfsNameStatus
HgfsServerPolicy_GetShareOptions(char const *nameIn,        // IN: Share name
                          size_t nameInLen,                 // IN: Share name length
                          HgfsShareOptions *configOptions); // OUT: Share config options

Bool
HgfsServerPolicy_IsShareOptionSet(HgfsShareOptions shareOptions,  // IN: Config options
                                  uint32 option);                 // IN: Option to check
HgfsNameStatus
HgfsServerPolicy_ProcessCPName(char const *nameIn,            // IN: name in CPName form
                               size_t nameInLen,              // IN: length of the name
                               Bool *readAccess,              // OUT: Read permissions
                               Bool *writeAccess,             // OUT: Write permissions
                               HgfsSharedFolderHandle *handle,// OUT: folder handle
                               char const **shareBaseDir);    // OUT: Shared directory

void
HgfsServerPolicy_FreeShareList(HgfsServerPolicy_ShareList *shareList); // IN: list to free

HgfsServerPolicy_ShareList *
HgfsServerPolicy_GetSharesWithTag(const char *tag); // IN: tag to search for

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // _HGFS_SERVER_POLICY_H_
