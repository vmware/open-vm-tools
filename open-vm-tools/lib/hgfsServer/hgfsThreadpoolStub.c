/*********************************************************
 * Copyright (C) 2020 VMware, Inc. All rights reserved.
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
 * hgfsThreadPoolStub.c --
 *
 *	Stubs for threadpool support, used to build guest components.
 */

#include "vmware.h"
#include "vm_basic_types.h"
#include "util.h"

#include "hgfsProto.h"
#include "hgfsServer.h"
#include "hgfsThreadpool.h"

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsThreadpool_Init --
 *
 *    Initialization of the threadpool component.
 *
 * Results:
 *    0 if success, error code otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsThreadpool_Init(void)
{
   return HGFS_ERROR_NOT_SUPPORTED;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsThreadpool_Activate --
 *
 *    Activate the threadpool.
 *
 * Results:
 *    Always return FALSE.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsThreadpool_Activate(void)
{
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsThreadpool_Deactivate --
 *
 *    Deactivate the threadpool.
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
HgfsThreadpool_Deactivate(void)
{
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsThreadpool_Exit --
 *
 *    Exit for the threadpool component.
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
HgfsThreadpool_Exit(void)
{
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsThreadpool_QueueWorkItem --
 *
 *    Execute a work item.
 *
 * Results:
 *    TRUE if the work item is queued successfully,
 *    FALSE if the work item is not queued.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsThreadpool_QueueWorkItem(HgfsThreadpoolWorkItem workItem, // IN
                             void *data)                      // IN
{
   return FALSE;
}

