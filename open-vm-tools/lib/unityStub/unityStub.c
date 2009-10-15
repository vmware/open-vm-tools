/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * UnityStub.c --
 *
 *    Unity functions.
 */

#include "rpcin.h"
#include "guestApp.h"
#include "unity.h"

/*
 *-----------------------------------------------------------------------------
 *
 * Unity_Init  --
 *
 *     One time initialization stuff.
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
Unity_Init(GuestApp_Dict *conf,
           int *blockingWndList,
           DesktopSwitchCallbackManager *desktopSwitchCallbackMgr)
{
}

void
Unity_InitBackdoor(struct RpcIn *rpcIn)
{
}

Bool
Unity_IsSupported(void)
{
   return FALSE;
}

void
Unity_SetActiveDnDDetWnd(UnityDnD *state)
{
}

void
Unity_Exit(void)
{
}

void
Unity_Cleanup(void)
{
}

void
Unity_RegisterCaps(void)
{
}

void
Unity_UnregisterCaps(void)
{
}
