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

#ifndef _HGFS_DIRNOTIFY_H
#define _HGFS_DIRNOTIFY_H

/*
 * hgfsDirNotify.h --
 *
 *	Function definitions for directory change notification.
 */

struct HgfsSessionInfo;
/* This is a callback that is implemented in hgfsServer.c */
typedef void HgfsNotificationCallbackFunc(HgfsSharedFolderHandle sharedFolder,
                                          HgfsSubscriberHandle subscriber,
                                          char *name,
                                          uint32 mask,
                                          struct HgfsSessionInfo *session);
HgfsInternalStatus HgfsNotify_Init(void);
void HgfsNotify_Shutdown(void);
void HgfsNotify_Suspend(void);
void HgfsNotify_Resume(void);

HgfsSharedFolderHandle HgfsNotify_AddSharedFolder(const char *path,
                                                  const char *shareName);
HgfsSubscriberHandle HgfsNotify_AddSubscriber(HgfsSharedFolderHandle sharedFolder,
                                              const char *path,
                                              uint32 eventFilter,
                                              uint32 recursive,
                                              HgfsNotificationCallbackFunc notify,
                                              struct HgfsSessionInfo *session);

Bool HgfsNotify_RemoveSharedFolder(HgfsSharedFolderHandle sharedFolder);
Bool HgfsNotify_RemoveSubscriber(HgfsSubscriberHandle subscriber);
void HgfsNotify_CleanupSession(struct HgfsSessionInfo *session);
Bool HgfsNotify_GetShareName(HgfsSharedFolderHandle sharedFolder,
                             size_t *shareNameLen,
                             char **shareName);

#endif // _HGFS_DIRNOTIFY_H
