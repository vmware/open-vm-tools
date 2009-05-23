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
 * hgfsDirNotify.h --
 *
 *	Function definitions for directory change notification.
 */

/*
 *  XXX:
 *  Following constants comes from HGFS protocol definition.
 *  When hgfsProto.h is updated with new definitions for V4
 *  these constants should be removed and definitions from
 *  hgfsProto.h should be used instead.
 */
#define HGFS_FILE_NOTIFY_ADD_FILE                 (1 << 0)
#define HGFS_FILE_NOTIFY_ADD_DIR                  (1 << 1)
#define HGFS_FILE_NOTIFY_DELETE_FILE              (1 << 2)
#define HGFS_FILE_NOTIFY_DELETE_DIR               (1 << 3)
#define HGFS_FILE_NOTIFY_RENAME_FILE              (1 << 4)
#define HGFS_FILE_NOTIFY_RENAME_DIR               (1 << 5)
#define HGFS_FILE_NOTIFY_CHANGE_SIZE              (1 << 6)
#define HGFS_FILE_NOTIFY_CHANGE_LAST_WRITE        (1 << 7)
#define HGFS_FILE_NOTIFY_CHANGE_LAST_ACCESS       (1 << 8)
#define HGFS_FILE_NOTIFY_CHANGE_CREATION          (1 << 9)
#define HGFS_FILE_NOTIFY_CHANGE_EA                (1 << 10)
#define HGFS_FILE_NOTIFY_CHANGE_SECURITY          (1 << 11)
#define HGFS_FILE_NOTIFY_ADD_STREAM               (1 << 12)
#define HGFS_FILE_NOTIFY_DELETE_STREAM            (1 << 13)
#define HGFS_FILE_NOTIFY_CHANGE_STREAM_SIZE       (1 << 14)
#define HGFS_FILE_NOTIFY_CHANGE_STREAM_LAST_WRITE (1 << 15)
#define HGFS_FILE_NOTIFY_WATCH_DELETED            (1 << 16)
#define HGFS_FILE_NOTIFY_EVENTS_DROPPED           (1 << 17)

#define INVALID_OBJECT_HANDLE  0xffffffff

typedef uint32 SharedFolderHandle;
typedef uint64 SubscriberHandle;

uint32 HgfsNotify_Init(void);
void HgfsNotify_Shutdown(void);

SharedFolderHandle HgfsNotify_AddSharedFolder(const char* path);
SubscriberHandle HgfsNotify_AddSubscriber(SharedFolderHandle sharedFolder,
                                          const char *path,
                                          uint32 eventFilter,
                                          uint32 recursive);

uint32 HgfsNotify_RemoveSharedFolder(SharedFolderHandle sharedFolder);
uint32 HgfsNotify_RemoveSubscriber(SubscriberHandle subscriber);

/* This is a callback that is implemented in hgfsServer.c */
void Hgfs_NotificationCallback(SharedFolderHandle sharedFolder,
                               SubscriberHandle subscriber,
                               char* name,
                               char* newName,
                               uint32 mask);
