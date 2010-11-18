/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 * channel.h --
 */

#ifndef _HGFS_CHANNEL_H_
#define _HGFS_CHANNEL__H_

#include "hgfs_kernel.h"
#include "requestInt.h"

/*
 * There are the operations a channel should implement.
 */
struct HgfsTransportChannel;
typedef struct HgfsTransportChannelOps {
   Bool (*open)(struct HgfsTransportChannel *);
   void (*close)(struct HgfsTransportChannel *);
   HgfsKReqObject* (*allocate)(size_t payloadSize, int flags);
   int (*send)(struct HgfsTransportChannel *, HgfsKReqObject *);
   void (*free)(HgfsKReqObject *, size_t payloadSize);
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
} HgfsTransportChannel;

HgfsTransportChannel *HgfsGetBdChannel(void);
HgfsTransportChannel *HgfsGetVmciChannel(void);
Bool HgfsSetupNewChannel(void);
extern HgfsTransportChannel *gHgfsChannel;

#endif // _HGFS_CHANNEL_H_
