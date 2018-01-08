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

/*
 * hgfsDirNotifyStub.c --
 *
 *	Stubs for directory notification support, used to build guest components.
 */

#include <stdio.h>

#include "vmware.h"
#include "vm_basic_types.h"

#include "hgfsProto.h"
#include "hgfsServer.h"
#include "hgfsUtil.h"
#include "hgfsDirNotify.h"


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsNotify_Init --
 *
 *    Initialization for the notification component.
 *
 * Results:
 *    Invalid value error.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsNotify_Init(const HgfsServerNotifyCallbacks *serverCbData) // IN: serverCbData unused
{
   return HGFS_ERROR_NOT_SUPPORTED;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsNotify_Exit --
 *
 *    Exit for the notification component.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsNotify_Exit(void)
{
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsNotify_Activate --
 *
 *    Activates generating file system change notifications.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsNotify_Activate(HgfsNotifyActivateReason reason, // IN: reason
                    struct HgfsSessionInfo *session) // IN: session
{
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsNotify_Deactivate --
 *
 *    Deactivates generating file system change notifications.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsNotify_Deactivate(HgfsNotifyActivateReason reason, // IN: reason
                      struct HgfsSessionInfo *session) // IN: session
{
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsNotify_AddSharedFolder --
 *
 *    Allocates memory and initializes new shared folder structure.
 *
 * Results:
 *    Opaque subscriber handle for the new subscriber or HGFS_INVALID_FOLDER_HANDLE
 *    if adding shared folder fails.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsSharedFolderHandle
HgfsNotify_AddSharedFolder(const char *path,       // IN: path in the host
                           const char *shareName)  // IN: name of the shared folder
{
   return HGFS_INVALID_FOLDER_HANDLE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsNotify_AddSubscriber --
 *
 *    Allocates memory and initializes new subscriber structure.
 *    Inserts allocated subscriber into corrspondent array.
 *
 * Results:
 *    Opaque subscriber handle for the new subscriber or HGFS_INVALID_SUBSCRIBER_HANDLE
 *    if adding subscriber fails.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsSubscriberHandle
HgfsNotify_AddSubscriber(HgfsSharedFolderHandle sharedFolder, // IN: shared folder handle
                         const char *path,                    // IN: relative path
                         uint32 eventFilter,                  // IN: event filter
                         uint32 recursive,                    // IN: look in subfolders
                         struct HgfsSessionInfo *session)     // IN: server context
{
   return HGFS_INVALID_SUBSCRIBER_HANDLE;
}

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsNotify_RemoveSharedFolder --
 *
 *    Deallcates memory used by shared folder and performs necessary cleanup.
 *    Also deletes all subscribers that are defined for the shared folder.
 *
 * Results:
 *    FALSE.
 *
 * Side effects:
 *    Removes all subscribers that correspond to the shared folder and invalidates
 *    thier handles.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsNotify_RemoveSharedFolder(HgfsSharedFolderHandle sharedFolder) // IN
{
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsNotify_RemoveSubscriber --
 *
 *    Deallcates memory used by NotificationSubscriber and performs necessary cleanup.
 *
 * Results:
 *    FALSE.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsNotify_RemoveSubscriber(HgfsSubscriberHandle subscriber) // IN
{
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsNotify_RemoveSessionSubscribers --
 *
 *    Removes all entries that are related to a particular session.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsNotify_RemoveSessionSubscribers(struct HgfsSessionInfo *session) // IN
{
}
