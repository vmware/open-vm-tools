/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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
 * pointer.c --
 *
 *     Set of functions for pointer (mouse) grab/ungrab.
 */

#include "vmwareuserInt.h"
#include <stdlib.h>
#include <string.h>

#include "vm_assert.h"
#include "debug.h"
#include "str.h"
#include "strutil.h"
#include "vm_app.h"
#include "guestApp.h"
#include "eventManager.h"

static Bool mouseIsGrabbed;
static uint8 gHostClipboardTries = 0;

/*
 * Forward Declarations
 */
void PointerGrabbed(void);
void PointerUngrabbed(void);
void PointerGetXCursorPos(int *x, int *y);
static Bool PointerUpdatePointerLoop(void* clientData);

/*
 *-----------------------------------------------------------------------------
 *
 * PointerGetXCursorPos --
 *
 *      Return the position in pixels of the X (mouse) pointer in the root
 *      window.
 *
 * Results:
 *      x and y coordinates.
 *
 * Side effects:
 *      None.
 *-----------------------------------------------------------------------------
 */

void
PointerGetXCursorPos(int *rootX, int *rootY)
{
   Window rootWin;
   Window childWin;
   int x;
   int y;
   unsigned int mask;

   XQueryPointer(gXDisplay, gXRoot, &rootWin, &childWin, rootX, rootY, &x, &y, &mask);
}


/*
 *-----------------------------------------------------------------------------
 *
 * PointerSetXCursorPos
 *
 *      Set the position in pixels of the X (mouse) pointer in the root window
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
PointerSetXCursorPos(int x, int y)
{
   XWarpPointer(gXDisplay, None, gXRoot, 0, 0, 0, 0, x, y);
}


/*
 *-----------------------------------------------------------------------------
 *
 * PointerGrabbed --
 *
 *      Called when the pointer's state switches from released to grabbed.
 *      We warp the cursor to whatever position the vmx tells us, and then
 *      setup the loop which attempts to get the host clipboard.
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
PointerGrabbed()
{
   short hostPosX;
   short hostPosY;

   GuestApp_GetPos(&hostPosX, &hostPosY);

   PointerSetXCursorPos(hostPosX, hostPosY);
   gHostClipboardTries = 9;
}


/*
 *-----------------------------------------------------------------------------
 *
 * PointerUngrabbed --
 *
 *      Called when the pointer's state switches from grabbed to release.
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
PointerUngrabbed(void)
{
   CopyPaste_RequestSelection();
}


/*
 *-----------------------------------------------------------------------------
 *
 * PointerUpdatePointerLoop  --
 *
 *      Event Manager function for tracking the mouse/pointer/clipboard state.
 *      Manage grabbed/ungrab state based on x/y data from backdoor. On the
 *      transition to grabbed, call PointerHasBeenGrabbed(). While grabbed,
 *      send guest pointer coordinates thru the backdoor. Also, make several
 *      attempts to get the host clipboard from the backdoor. When changing
 *      to ungrabbed, call PointerHasBeenUngrabbed, which will push our
 *      clipboard thru the backdoor. While ungrabbed, don't do a thing.
 *
 * Results:
 *      TRUE.
 *
 * Side effects:
 *      Lots. The vmx's notion of guest cursor position could change, the
 *      vmx's clipboard could change, and the guest's clipboard could change.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
PointerUpdatePointerLoop(void* clientData) // IN: unused
{
   int16 hostX, hostY;
   int guestX, guestY;

   GuestApp_GetPos(&hostX, &hostY);
   if (mouseIsGrabbed) {
      if (hostX == UNGRABBED_POS) {
         /* We transitioned from grabbed to ungrabbed */
         mouseIsGrabbed = FALSE;
         PointerUngrabbed();
      } else {
         PointerGetXCursorPos(&guestX, &guestY);
         if (hostX != guestX || hostY != guestY) {
            GuestApp_SetPos(guestX,guestY);
         }
         if (gHostClipboardTries-- > 0 ) {
            if (gHostClipboardTries < 6 && 
                CopyPaste_GetBackdoorSelections()) {
               gHostClipboardTries = 0;
            }
         }
      }
   } else {
      if (hostX != UNGRABBED_POS) {
         mouseIsGrabbed = TRUE;
         PointerGrabbed();
      }

   }

   EventManager_Add(gEventQueue, POINTER_POLL_TIME, PointerUpdatePointerLoop,
                    clientData);
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Pointer_Register --
 *
 *      Initialize Pointer.
 *
 * Results:
 *      Always TRUE.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Pointer_Register(GtkWidget* mainWnd)
{
   PointerUpdatePointerLoop(mainWnd);
   mouseIsGrabbed = FALSE;
   return TRUE;
}
