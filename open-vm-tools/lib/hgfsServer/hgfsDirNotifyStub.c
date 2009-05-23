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
 * hgfsDirNotifyStub.c --
 *
 *	Stubs for directory notification support, used to build guest components.
 */

#include <stdio.h>
#include <errno.h>

#include "vmware.h"
#include "vm_basic_types.h"

#include "hgfsDirNotify.h"


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsNotify_Init --
 *
 *    One time initialization of the library.
 *
 * Results:
 *    0 if success, error code otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

uint32
HgfsNotify_Init(void)
{
   return EINVAL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsNotify_Shutdown --
 *
 *    Performs nesessary cleanup.
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
HgfsNotify_Shutdown(void)
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
 *    Opaque subscriber handle for the new subscriber or INVALID_OBJECT_HANDLE
 *    if adding shared folder fails.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

SharedFolderHandle
HgfsNotify_AddSharedFolder(const char* path) // IN
{
   return INVALID_OBJECT_HANDLE;
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
 *    Opaque subscriber handle for the new subscriber or INVALID_OBJECT_HANDLE
 *    if adding subscriber fails.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

SubscriberHandle
HgfsNotify_AddSubscriber(SharedFolderHandle sharedFolder, // IN
              const char *path,                // IN path relative to shared folder
              uint32 eventFilter,              // IN
              uint32 recursive)                // IN TRUE if look in subdirectories
{
   return INVALID_OBJECT_HANDLE;
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
 *    0 if success, error code otherwise.
 *
 * Side effects:
 *    Removes all subscribers that correspond to the shared folder and invalidates
 *    thier handles.
 *
 *-----------------------------------------------------------------------------
 */

uint32
HgfsNotify_RemoveSharedFolder(SharedFolderHandle sharedFolder) // IN
{
   return EINVAL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsNotify_RemoveSubscriber --
 *
 *    Deallcates memory used by NotificationSubscriber and performs necessary cleanup.
 *
 * Results:
 *    0 if success, error code otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

uint32
HgfsNotify_RemoveSubscriber(SubscriberHandle subscriber) // IN
{
   return EINVAL;
}
