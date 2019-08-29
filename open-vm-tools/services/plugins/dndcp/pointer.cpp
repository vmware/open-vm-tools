/*********************************************************
 * Copyright (C) 2006-2019 VMware, Inc. All rights reserved.
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
 * pointer.cpp --
 *
 *    Pointer functions.
 */

#define G_LOG_DOMAIN "dndcp"

#if defined(WIN32)
#include "dndPluginInt.h"
#elif defined(__APPLE__)
extern void PointerGetMacCursorPos(int *rootX, int *rootY);
extern void PointerSetMacCursorPos(int x, int y);
#else
#include "dndPluginIntX11.h"
#endif

extern "C" {
#include "copyPasteCompat.h"
}

#include "copyPasteDnDWrapper.h"
#include "pointer.h"
#include "vmware/tools/utils.h"
#include "vm_assert.h"
#include "backdoor_def.h"

extern "C" {
   #include "backdoor.h"
   #include "rpcvmx.h"
}


typedef enum {
   ABSMOUSE_UNAVAILABLE,
   ABSMOUSE_AVAILABLE,
   ABSMOUSE_UNKNOWN
} AbsoluteMouseState;


static Bool mouseIsGrabbed;
static AbsoluteMouseState absoluteMouseState = ABSMOUSE_UNKNOWN;
static uint8 gHostClipboardTries = 0;

#if defined(WIN32)
extern BOOL vmx86WantsSelection;
#endif

static void PointerGrabbed(void);
static void PointerUngrabbed(void);
static gboolean PointerUpdatePointerLoop(gpointer clientData);

#define POINTER_UPDATE_TIMEOUT 100


/*
 *----------------------------------------------------------------------------
 *
 * PointerGetAbsoluteMouseState
 *
 *    Are the host/guest capable of using absolute mouse mode?
 *
 * Results:
 *    TRUE if host is in absolute mouse mode, FALSE otherwise.
 *
 * Side effects:
 *    Issues Tools RPC.
 *
 *----------------------------------------------------------------------------
 */

static AbsoluteMouseState
PointerGetAbsoluteMouseState(void)
{
   Backdoor_proto bp;
   AbsoluteMouseState state = ABSMOUSE_UNKNOWN;

   bp.in.cx.halfs.low = BDOOR_CMD_ISMOUSEABSOLUTE;
   Backdoor(&bp);
   if (bp.out.ax.word == 0) {
      state = ABSMOUSE_UNAVAILABLE;
   } else if (bp.out.ax.word == 1) {
      state = ABSMOUSE_AVAILABLE;
   }

   return state;
}


#if !defined(WIN32) && !defined(__APPLE__)
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

#endif


/*
 *-----------------------------------------------------------------------------
 *
 * PointerGetPos --
 *
 *      Retrieve the host notion of the guest pointer location.
 *
 * Results:
 *      '*x' and '*y' are the coordinates (top left corner is 0, 0) of the
 *      host notion of the guest pointer location. (-100, -100) means that the
 *      mouse is not grabbed on the host.
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

static void
PointerGetPos(int16 *x, // OUT
              int16 *y) // OUT
{
   Backdoor_proto bp;

   bp.in.cx.halfs.low = BDOOR_CMD_GETPTRLOCATION;
   Backdoor(&bp);
   *x = bp.out.ax.word >> 16;
   *y = bp.out.ax.word;
}


/*
 *-----------------------------------------------------------------------------
 *
 * PointerSetPos --
 *
 *      Update the host notion of the guest pointer location. 'x' and 'y' are
 *      the coordinates (top left corner is 0, 0).
 *
 * Results:
 *      None
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

static void
PointerSetPos(uint16 x, // IN
              uint16 y) // IN
{
   Backdoor_proto bp;

   bp.in.cx.halfs.low = BDOOR_CMD_SETPTRLOCATION;
   bp.in.size = (x << 16) | y;
   Backdoor(&bp);
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
 * Result:
 *      None..
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
PointerGrabbed(void)
{
   short hostPosX;
   short hostPosY;

   PointerGetPos(&hostPosX, &hostPosY);
#if defined(WIN32)
   SetCursorPos(hostPosX, hostPosY);
#elif defined(__APPLE__)
   if (!CopyPaste_IsRpcCPSupported()) {
      if(absoluteMouseState != ABSMOUSE_AVAILABLE) {
        PointerSetMacCursorPos(hostPosX, hostPosY);
      }
   }
#else
   PointerSetXCursorPos(hostPosX, hostPosY);
#endif
   gHostClipboardTries = 9;
}


/*
 *-----------------------------------------------------------------------------
 *
 * PointerUngrabbed --
 *
 *    Called by the background thread when the pointer's state switches
 *    from grabbed to ungrabbed
 *
 * Result:
 *      None..
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
PointerUngrabbed()
{
#if defined(WIN32)
   if (vmx86WantsSelection) {
      /*
       * vmx agrees to exchange selections. This is a little
       * optimization to avoid an unnecessary backdoor call if vmx
       * disagrees.
       */
      CopyPaste_RequestSelection();
   }
#else
   CopyPaste_RequestSelection();
#endif
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
 *      This function is queued in Event Manager only when vmx doesn't support
 *      RPC copy/paste because newer vmx initiates copy/paste from UI through
 *      RPC, and doesn't need cursor grab/ungrab state to start copy/paste.
 *
 * Results:
 *      FALSE.
 *
 * Side effects:
 *      Lots. The vmx's notion of guest cursor position could change, the
 *      vmx's clipboard could change, and the guest's clipboard could change.
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
PointerUpdatePointerLoop(gpointer clientData) // IN: unused
{
   int16 hostPosX, hostPosY;
#if defined(WIN32)
   POINT guestPos;
#else
   int guestX, guestY;
#endif

   PointerGetPos(&hostPosX, &hostPosY);
   if (mouseIsGrabbed) {
      if (hostPosX == UNGRABBED_POS) {
         /* We transitioned from grabbed to ungrabbed */
         mouseIsGrabbed = FALSE;
         g_debug("PointerUpdatePointerLoop: ungrabbed\n");
         PointerUngrabbed();
      } else {
#if defined(WIN32)
         /*
          * We used to return early if GetCursorPos() failed, but we at least want to
          * continue and do the selection work. Also, I'm not sure we need this code anymore
          * since all new tools have absolute pointing device
          */
         if (!GetCursorPos(&guestPos)) {
            g_debug("PointerIsGrabbed: GetCursorPos() failed!\n");
         } else {
            if ( hostPosX != guestPos.x || hostPosY != guestPos.y) {
               /*
                * Report the new guest pointer location (It is used to teach VMware
                * where to position the outside pointer if the user releases the guest
                * pointer via the key combination).
                */
               PointerSetPos(guestPos.x, guestPos.y);
            }
         }
#elif defined(__APPLE__)
         if (!CopyPaste_IsRpcCPSupported()) {
            if(absoluteMouseState != ABSMOUSE_AVAILABLE) {
                PointerGetMacCursorPos(&guestX, &guestY);
                if ( hostPosX != guestX || hostPosY != guestY) {
                    PointerSetPos(guestX, guestY);
                }
            }
         }
#else
         PointerGetXCursorPos(&guestX, &guestY);
         if ( hostPosX != guestX || hostPosY != guestY) {
            PointerSetPos(guestX, guestY);
         }
#endif
         CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();
         ASSERT(wrapper);
         if (gHostClipboardTries > 0) {
            gHostClipboardTries--;
            if (wrapper->IsCPEnabled() && gHostClipboardTries < 6 &&
               CopyPaste_GetBackdoorSelections()) {
               gHostClipboardTries = 0;
            }
         }
      }
   } else {
      if (hostPosX != UNGRABBED_POS) {
         mouseIsGrabbed = TRUE;
         g_debug("PointerUpdatePointerLoop: grabbed\n");
         PointerGrabbed();
      }
   }

   if (!CopyPaste_IsRpcCPSupported() ||
       (absoluteMouseState == ABSMOUSE_UNAVAILABLE)) {

      GSource *src;

      CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();
      ToolsAppCtx *ctx = wrapper->GetToolsAppCtx();
      if (ctx) {
         src = VMTools_CreateTimer(POINTER_UPDATE_TIMEOUT);
         VMTOOLSAPP_ATTACH_SOURCE(ctx, src, PointerUpdatePointerLoop, NULL, NULL);
         g_source_unref(src);
      }
   }

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Pointer_Init  --
 *
 *     One time pointer initialization stuff. Enter the pointer update
 *     loop which will check mouse position and put pointer in grabbed
 *     and ungrabbed state, accordingly (see PointerUpdatePointerLoop()
 *     for details.)
 *
 * Results:
 *      None..
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
Pointer_Init(ToolsAppCtx *ctx)
{
   absoluteMouseState = PointerGetAbsoluteMouseState();
   g_debug("%s:absoluteMouseState:%s\n", __FUNCTION__,
           ((absoluteMouseState == ABSMOUSE_UNAVAILABLE)?
           "ABSMOUSE_UNAVAILABLE":((absoluteMouseState == ABSMOUSE_AVAILABLE)?
           "ABSMOUSE_AVAILABLE":"ABSMOUSE_UNKNOWN")));
   PointerUpdatePointerLoop(NULL);
   mouseIsGrabbed = FALSE;
}
