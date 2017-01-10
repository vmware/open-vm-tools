/*********************************************************
 * Copyright (C) 2013 VMware, Inc. All rights reserved.
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
 * transport.h --
 */

#ifndef _HGFS_DRIVER_TRANSPORT_H_
#define _HGFS_DRIVER_TRANSPORT_H_

#include "request.h"
#include <pthread.h>

typedef enum {
   HGFS_CHANNEL_UNINITIALIZED,
   HGFS_CHANNEL_NOTCONNECTED,
   HGFS_CHANNEL_CONNECTED,
} HgfsChannelStatus;

/*
 * There are the operations a channel should implement.
 */
struct HgfsTransportChannel;
typedef struct HgfsTransportChannelOps {
   HgfsChannelStatus (*open)(struct HgfsTransportChannel *);
   void (*close)(struct HgfsTransportChannel *);
   int (*send)(struct HgfsTransportChannel *, HgfsReq *);
   int (*recv)(struct HgfsTransportChannel *, char **, size_t *);
   void (*exit)(struct HgfsTransportChannel *);
} HgfsTransportChannelOps;

typedef struct HgfsTransportChannel {
   const char *name;               /* Channel name. */
   HgfsTransportChannelOps ops;    /* Channel ops. */
   HgfsChannelStatus status;       /* Connection status. */
   void *priv;                     /* Channel private data. */
   pthread_mutex_t connLock;       /* Protect _this_ struct. */
} HgfsTransportChannel;

/* Public functions (with respect to the entire module). */
int HgfsTransportInit(void);
void HgfsTransportExit(void);
int HgfsTransportSendRequest(HgfsReq *req);
void HgfsTransportProcessPacket(char *receivedPacket,
                                size_t receivedSize);
void HgfsTransportBeforeExitingRecvThread(void);

#endif // _HGFS_DRIVER_TRANSPORT_H_
