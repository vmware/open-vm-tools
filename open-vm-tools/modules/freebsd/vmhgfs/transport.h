/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * transport.h --
 *
 * Communication with HGFS server.
 */

#ifndef _HGFS_TRANSPORT_COMMON_H_
#define _HGFS_TRANSPORT_COMMON_H_

#include "hgfs_kernel.h"

/* Internal transport functions used by both FreeBSD and Mac OS */
int HgfsSendOpenRequest(HgfsSuperInfo *sip, int openMode, int openFlags,
                        int permissions, char *fullPath,
                        uint32 fullPathLen, HgfsHandle *handle);
int HgfsSendOpenDirRequest(HgfsSuperInfo *sip, char *fullPath,
                           uint32 fullPathLen, HgfsHandle *handle);
int HgfsCloseServerDirHandle(HgfsSuperInfo *sip, HgfsHandle handleToClose);
int HgfsCloseServerFileHandle(HgfsSuperInfo *sip, HgfsHandle handleToClose);

#endif // _HGFS_TRANSPORT_COMMON_H_
