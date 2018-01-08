/*********************************************************
 * Copyright (C) 2009-2017 VMware, Inc. All rights reserved.
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

#ifndef _HGFSCHANNELGUESTINT_H_
#define _HGFSCHANNELGUESTINT_H_

#if defined(VMTOOLS_USE_GLIB)
#define G_LOG_DOMAIN          "hgfsd"
#define Debug                 g_debug
#define Warning               g_warning
#else
#include "debug.h"
#endif
#include "hgfsServer.h"
#include "hgfsServerManager.h"

/**
 * @file hgfsChannelGuestInt.h
 *
 * Prototypes of Hgfs channel packet process handler found in
 * hgfsChannelGuest.c
 */

/*
 * Opaque structure owned by the guest channel to hold the connection
 * data to the HGFS server. Only held by the channel manager to pass
 * back to the guest channel for requests and teardown.
 * (Or it would be used with any registered internal callback.)
 */
 struct HgfsGuestConn;

/*
 * Guest channel table of callbacks.
 */
typedef struct HgfsGuestChannelCBTable {
   Bool (*init)(const HgfsServerSessionCallbacks *, void *, void *, struct HgfsGuestConn **);
   void (*exit)(struct HgfsGuestConn *);
   Bool (*receive)(struct HgfsGuestConn *, char const *, size_t, char *, size_t *);
   uint32 (*invalidateInactiveSessions)(struct HgfsGuestConn *);
} HgfsGuestChannelCBTable;

/* The guest channels callback tables. */
extern const HgfsGuestChannelCBTable gGuestBackdoorOps;

/* For use by HgfsServerManager. */
Bool HgfsChannelGuest_Init(HgfsServerMgrData *data, HgfsServerMgrCallbacks *cb);
void HgfsChannelGuest_Exit(HgfsServerMgrData *data);
Bool HgfsChannelGuest_Receive(HgfsServerMgrData *data,
                              char const *packetIn,
                              size_t packetInSize,
                              char *packetOut,
                              size_t *packetOutSize);
uint32 HgfsChannelGuest_InvalidateInactiveSessions(HgfsServerMgrData *data);

#endif /* _HGFSCHANNELGUESTINT_H_ */

