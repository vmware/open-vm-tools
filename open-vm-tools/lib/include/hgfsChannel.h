/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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
 * hgfsChannel.h --
 *
 *    Channel abstraction for the HGFS server.
 */

#ifndef _HGFSCHANNEL_H_
#define _HGFSCHANNEL_H_

#include "vm_basic_types.h"
#include "dbllnklst.h"
#include "hgfsServer.h"

/*
 * Handle used by the server to identify files and searches. Used
 * by the driver to match server replies with pending requests.
 */

typedef uint32 HgfsChannelId;

typedef struct HgfsChannelCBTable {
   Bool (*init)(HgfsChannelId, HgfsServerSessionCallbacks *, void **);
   void (*exit)(void *);
   void (*invalidateObjects)(DblLnkLst_Links *, void *);
} HgfsChannelCBTable;


/* For use by HgfsServerManager. */
Bool HgfsChannel_Init(void *data);  /* Optional data, used in guest. */
void HgfsChannel_Exit(void *data);  /* Optional data, used in guest. */
void HgfsChannel_InvalidateObjects(DblLnkLst_Links *shares);
#endif
