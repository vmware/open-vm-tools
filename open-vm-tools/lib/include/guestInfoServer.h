/*********************************************************
 * Copyright (C) 2003 VMware, Inc. All rights reserved.
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
 * guestInfoServer.h --
 *
 *	GuestInfo server
 */

#ifndef _GUEST_INFO_SERVER_H_
#define _GUEST_INFO_SERVER_H_

#include "vm_basic_types.h"
#include "dbllnklst.h"

#ifdef _WIN32
void GuestInfoServer_Main(void *data);
#endif
Bool GuestInfoServer_Init(DblLnkLst_Links * eventQueue);
void GuestInfoServer_Cleanup(void);
void GuestInfoServer_VMResumedNotify(void);
uint64 GuestInfo_GetAvailableDiskSpace(char *pathName);
void GuestInfoServer_DisableDiskInfoQuery(Bool disable);
Bool GuestInfoServer_SendUptime(void);

#endif // _GUEST_INFO_SERVER_H_
