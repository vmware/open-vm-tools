/*********************************************************
 * Copyright (C) 2013-2016 VMware, Inc. All rights reserved.
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
 * hgfsServerOplock.h --
 *
 *	Header file for public common data types used in the HGFS
 *	opportunistic lock routines.
 */

#ifndef _HGFS_SERVER_OPLOCK_H_
#define _HGFS_SERVER_OPLOCK_H_

#include "hgfsProto.h"     // for protocol types
#include "hgfsServerInt.h" // for common server types e.g. HgfsSessionInfo


/*
 * Data structures
 */



/*
 * Global variables
 */



/*
 * Global functions
 */

Bool HgfsServerOplockInit(void);
void HgfsServerOplockDestroy(void);
Bool HgfsHandle2ServerLock(HgfsHandle handle,
                           HgfsSessionInfo *session,
                           HgfsLockType *lock);
Bool HgfsFileHasServerLock(const char *utf8Name,
                           HgfsSessionInfo *session,
                           HgfsLockType *serverLock,
                           fileDesc   *fileDesc);

Bool HgfsAcquireServerLock(fileDesc fileDesc,
                           HgfsSessionInfo *session,
                           HgfsLockType *serverLock);


#endif // ifndef _HGFS_SERVER_OPLOCK_H_
