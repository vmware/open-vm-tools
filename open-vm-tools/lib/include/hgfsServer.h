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

#ifndef _HGFS_SERVER_H_
#define _HGFS_SERVER_H_

#include "hgfs.h"             /* for HGFS_PACKET_MAX */
#include "dbllnklst.h"

Bool
HgfsServer_InitState(void);

void
HgfsServer_DispatchPacket(char const *packetIn,
                          char *packetOut,
                          size_t *packetSize);

void
HgfsServer_ExitState(void);

/*
 * Function pointers used for getting names in HgfsServerGetDents
 *
 * Functions of this type are expected to return a NUL terminated
 * string and the length of that string.
 */
typedef Bool
HgfsGetNameFunc(void *data,        // IN
                char const **name, // OUT
                size_t *len,       // OUT
                Bool *done);       // OUT

/*
 * Associated setup and cleanup function types, which should be called
 * before and after (respectively) HgfsGetNameFunc.
 */
typedef void *
HgfsInitFunc(void);

typedef Bool
HgfsCleanupFunc(void *);  // IN

/*
 * Function used for invalidating nodes and searches that fall outside of a
 * share when the list of shares changes.
 */
typedef void
HgfsInvalidateObjectsFunc(DblLnkLst_Links *shares); // IN

HgfsInvalidateObjectsFunc HgfsServer_InvalidateObjects;

#endif // _HGFS_SERVER_H_
