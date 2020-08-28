/*********************************************************
 * Copyright (C) 2009-2019 VMware, Inc. All rights reserved.
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

#ifndef _HGFS_DIRNOTIFY_H
#define _HGFS_DIRNOTIFY_H

/*
 * hgfsDirNotify.h --
 *
 *	Function definitions for directory change notification.
 */

#include "hgfsServer.h" // for HgfsSharedFolderHandle
#include "hgfsProto.h"  // for HgfsSubscriberHandle
#include "hgfsUtil.h"   // for HgfsInternalStatus

struct HgfsSessionInfo;
/*
 * Activate and deactivate reason.
 * Currently, there are two scenarios:
 * 1) HGFS server is check point synchronizing: the file system event
 * generation is deactivated at the start and activated at the end.
 * 2) The client has added the first subscriber or removed the last
 * subscriber. The file system event generation is activated on the
 * addition of the first subscriber and deactivated on removal of
 * the last one.
 *
 * Note, in case 1 above, if there are no subscribers even at the end
 * of the HGFS server check point syncing, the activation will not
 * activate the file system events.
 */
typedef enum {
   HGFS_NOTIFY_REASON_SERVER_SYNC,
   HGFS_NOTIFY_REASON_SUBSCRIBERS,
} HgfsNotifyActivateReason;

/* These are the callbacks that are implemented in hgfsServer.c */
typedef void (*HgfsNotifyEventReceiveCb)(HgfsSharedFolderHandle sharedFolder,
                                         HgfsSubscriberHandle subscriber,
                                         char *name,
                                         uint32 mask,
                                         struct HgfsSessionInfo *session);

typedef struct HgfsServerNotifyCallbacks {
   HgfsNotifyEventReceiveCb       eventReceive;
} HgfsServerNotifyCallbacks;

HgfsInternalStatus HgfsNotify_Init(const HgfsServerNotifyCallbacks *serverCbData);
void HgfsNotify_Exit(void);
void HgfsNotify_Deactivate(HgfsNotifyActivateReason mode,
                           struct HgfsSessionInfo *session);
void HgfsNotify_Activate(HgfsNotifyActivateReason mode,
                         struct HgfsSessionInfo *session);

HgfsSharedFolderHandle HgfsNotify_AddSharedFolder(const char *path,
                                                  const char *shareName);
HgfsSubscriberHandle HgfsNotify_AddSubscriber(HgfsSharedFolderHandle sharedFolder,
                                              const char *path,
                                              uint32 eventFilter,
                                              uint32 recursive,
                                              struct HgfsSessionInfo *session);

Bool HgfsNotify_RemoveSharedFolder(HgfsSharedFolderHandle sharedFolder);
Bool HgfsNotify_RemoveSubscriber(HgfsSubscriberHandle subscriber);
void HgfsNotify_RemoveSessionSubscribers(struct HgfsSessionInfo *session);

#endif // _HGFS_DIRNOTIFY_H
