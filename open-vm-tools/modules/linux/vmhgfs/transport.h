/*********************************************************
 * Copyright (C) 2009-2016 VMware, Inc. All rights reserved.
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
 */

#ifndef _HGFS_DRIVER_TRANSPORT_H_
#define _HGFS_DRIVER_TRANSPORT_H_

#include "request.h"
#include "compat_mutex.h"
#include "hgfsProto.h"

/*
 * There are the operations a channel should implement.
 */
struct HgfsTransportChannel;
typedef struct HgfsTransportChannelOps {
   Bool (*open)(struct HgfsTransportChannel *);
   void (*close)(struct HgfsTransportChannel *);
   HgfsReq* (*allocate)(size_t payloadSize);
   int (*send)(struct HgfsTransportChannel *, HgfsReq *);
   void (*free)(HgfsReq *);
} HgfsTransportChannelOps;

typedef enum {
   HGFS_CHANNEL_UNINITIALIZED,
   HGFS_CHANNEL_NOTCONNECTED,
   HGFS_CHANNEL_CONNECTED,
   HGFS_CHANNEL_DEAD,   /* Error has been detected, need to shut it down. */
} HgfsChannelStatus;

typedef struct HgfsTransportChannel {
   const char *name;               /* Channel name. */
   HgfsTransportChannelOps ops;    /* Channel ops. */
   HgfsChannelStatus status;       /* Connection status. */
   void *priv;                     /* Channel private data. */
   compat_mutex_t connLock;        /* Protect _this_ struct. */
} HgfsTransportChannel;

/* Public functions (with respect to the entire module). */
void HgfsTransportInit(void);
void HgfsTransportExit(void);
HgfsReq *HgfsTransportAllocateRequest(size_t payloadSize);
void HgfsTransportFreeRequest(HgfsReq *req);
int HgfsTransportSendRequest(HgfsReq *req);
HgfsReq *HgfsTransportGetPendingRequest(HgfsHandle id);
void HgfsTransportRemovePendingRequest(HgfsReq *req);
void HgfsTransportFinishRequest(HgfsReq *req, Bool success, Bool do_put);
void HgfsTransportFlushRequests(void);
void HgfsTransportMarkDead(void);

HgfsTransportChannel *HgfsGetBdChannel(void);

#endif // _HGFS_DRIVER_TRANSPORT_H_
