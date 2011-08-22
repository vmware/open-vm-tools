/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 * debug.c --
 *
 *    Debugging functions for unity window manager intergration.
 *
 */

#include "vmware.h"
#include "debug.h"
#include "util.h"
#include "region.h"
#include "unityPlatform.h"
#include "unityWindowTracker.h"

/*
 *-----------------------------------------------------------------------------
 *
 * UnityDebug_Init  --
 *
 *     One time initialization stuff.  Create our Window GDI objects.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityDebug_Init(UnityWindowTracker *tracker)       // IN
{
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityDebug_OnUpdate  --
 *
 *      Called everytime we get an update request from the tools.  Just
 *      invalidate the debug window to trigger a repaint.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityDebug_OnUpdate(void)
{
}
