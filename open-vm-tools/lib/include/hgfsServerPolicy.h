/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
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

/*
 * Name of share that corresponds to the root of the server's
 * filesystem.
 */
#define HGFS_SERVER_POLICY_ROOT_SHARE_NAME "root"

Bool
HgfsServerPolicy_Init(HgfsInvalidateObjectsFunc *invalidateObjects);

Bool
HgfsServerPolicy_Cleanup(void);

/*
 * Structure representing one shared folder. We maintain a list of
 * these to check accesses against.
 */
typedef struct HgfsSharedFolder {
   DblLnkLst_Links links;
   char *name; /* Name of share */
   char *path; /*
                * Path of share in server's filesystem. Should
                * not include final path separator.
                */
   size_t nameLen;   /* Length of name string */
   size_t pathLen;   /* Length of path string */
   Bool readAccess;  /* Read permission for this share */
   Bool writeAccess; /* Write permission for this share */
} HgfsSharedFolder;

/* Defined in hgfsServerInt.h */
HgfsGetNameFunc HgfsServerPolicy_GetShares;
HgfsInitFunc HgfsServerPolicy_GetSharesInit;
HgfsCleanupFunc HgfsServerPolicy_GetSharesCleanup;

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

#endif // _HGFS_SERVER_POLICY_H_
