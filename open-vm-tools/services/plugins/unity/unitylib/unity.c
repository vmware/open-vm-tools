/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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

/**
 * @file unity.c
 *
 *    Unity: Guest window manager integration service.
 *
 * This file implements the guest-side Unity agent generally used as part of the
 * Tools Core Services Unity plugin. It contains the platform-agnostic entry points
 * for Unity window operations and establishes the context for the platform specific
 * window enumeration process that exports data from the guest window tracker to
 * the host.
 *
 * UnityWindowTracker updates are sent to the MKS in two ways:
 *    @li @ref UNITY_RPC_GET_UPDATE GuestRpc (host-to-guest).
 *    @li @ref UNITY_RPC_PUSH_UPDATE_CMD GuestRpc (guest-to-host).
 *
 * @note Looking for the old "unity.get.update" return syntax?  See @ref
 * UNITY_RPC_GET_UPDATE and @ref UnityGetUpdateReturn instead.
 *
 * @sa
 *    @li UnityRpcHG
 *    @li UnityRpcGH
 */

#include <glib.h>
#include <glib-object.h>
#include "vmware.h"
#include "debug.h"
#include "unityDebug.h"
#include "unityInt.h"
#include "unityPlatform.h"
#include "vmware/tools/unityevents.h"

/*
 * Singleton object for tracking the state of the service.
 */
UnityState unity;

/*
 * Helper Functions
 */
static void UnityUpdateCallbackFn(void *param, UnityUpdate *update);
static void UnitySetAddHiddenWindows(Bool enabled);
static void UnitySetInterlockMinimizeOperation(Bool enabled);
static void UnitySetSendWindowContents(Bool enabled);
static void FireEnterUnitySignal(gpointer serviceObj, gboolean entered);
static void UnitySetDisableCompositing(Bool disabled);

/*
 * Dispatch table for Unity window commands. All commands performing actions on
 * guest unity windows go here.
 */
typedef struct {
   const char *name;
   Bool (*exec)(UnityPlatform *up, UnityWindowId window);
} UnityCommandElem;

static UnityCommandElem unityCommandTable[] = {
   { UNITY_RPC_WINDOW_CLOSE, UnityPlatformCloseWindow },
   { UNITY_RPC_WINDOW_SHOW, UnityPlatformShowWindow },
   { UNITY_RPC_WINDOW_HIDE, UnityPlatformHideWindow },
   { UNITY_RPC_WINDOW_MINIMIZE, UnityPlatformMinimizeWindow },
   { UNITY_RPC_WINDOW_UNMINIMIZE, UnityPlatformUnminimizeWindow },
   { UNITY_RPC_WINDOW_MAXIMIZE, UnityPlatformMaximizeWindow },
   { UNITY_RPC_WINDOW_UNMAXIMIZE, UnityPlatformUnmaximizeWindow },
   { UNITY_RPC_WINDOW_STICK, UnityPlatformStickWindow },
   { UNITY_RPC_WINDOW_UNSTICK, UnityPlatformUnstickWindow },
   /* Add more commands and handlers above this. */
   { NULL, NULL }
};

/*
 * A list of the commands implemented in this library - this list should
 * match the command dispatch table.
 */
static char* unityCommandList[] = {
   UNITY_RPC_WINDOW_CLOSE,
   UNITY_RPC_WINDOW_SHOW,
   UNITY_RPC_WINDOW_HIDE,
   UNITY_RPC_WINDOW_MINIMIZE,
   UNITY_RPC_WINDOW_UNMINIMIZE,
   UNITY_RPC_WINDOW_MAXIMIZE,
   UNITY_RPC_WINDOW_UNMAXIMIZE,
   UNITY_RPC_WINDOW_STICK,
   UNITY_RPC_WINDOW_UNSTICK,
   /* Add more commands above this. */
   NULL
};

typedef struct {
   uint32 featureBit;
   void (*setter)(Bool enabled);
} UnityFeatureSetter;

/*
 * Dispatch table for each unity option and a specific function to handle enabling
 * or disabling the option. The function is called with an enable (TRUE) bool value.
 */
static UnityFeatureSetter unityFeatureTable[] = {
   { UNITY_ADD_HIDDEN_WINDOWS_TO_TRACKER, UnitySetAddHiddenWindows },
   { UNITY_INTERLOCK_MINIMIZE_OPERATION, UnitySetInterlockMinimizeOperation },
   { UNITY_SEND_WINDOW_CONTENTS, UnitySetSendWindowContents },
   { UNITY_DISABLE_COMPOSITING_IN_GUEST, UnitySetDisableCompositing },
   /* Add more Unity Feature Setters above this. */
   {0, NULL}
};

/*
 *----------------------------------------------------------------------------
 *
 * Unity_IsSupported --
 *
 *      Determine whether this guest supports unity.
 *
 * Results:
 *      TRUE if the guest supports Unity (i.e. if the guest is WinXP) or
 *      if the option to always enable unity was specified in the tools
 *      configuration file
 *      FALSE otherwise
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
Unity_IsSupported(void)
{
   return UnityPlatformIsSupported() || unity.forceEnable;
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_IsActive --
 *
 *      Determine whether we are in Unity mode at this moment.
 *
 * Results:
 *      TRUE if Unity is active.
 *      FALSE is Unity is not active.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
Unity_IsActive(void)
{
   return unity.isEnabled;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unity_Init  --
 *
 *     One time initialization stuff.
 *
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May register with the tools poll loop.
 *
 *-----------------------------------------------------------------------------
 */

void
Unity_Init(UnityHostCallbacks hostCallbacks,                       // IN
           gpointer serviceObj)                                    // IN
{
   Debug("Unity_Init\n");

   ASSERT(hostCallbacks.updateCB);
   ASSERT(hostCallbacks.buildUpdateCB);
   ASSERT(hostCallbacks.sendWindowContents);
   ASSERT(hostCallbacks.sendRequestMinimizeOperation);
   ASSERT(hostCallbacks.shouldShowTaskbar);

   unity.hostCallbacks = hostCallbacks;

   /*
    * Initialize the UnityWindowTracker object.  The uwt does all the actual work
    * of computing differences between two states of the windowing system.  The
    * callbacks we register here will fire when we request an update via
    * UnityWindowTracker_RequestUpdates.  See bora/lib/unityWindowTracker for more
    * information.
    */
   UnityWindowTracker_Init(&unity.tracker, UnityUpdateCallbackFn);


   /*
    * Initialize the platform-specific portion of the unity service.
    */
   unity.up = UnityPlatformInit(&unity.tracker,
                                unity.hostCallbacks);

   unity.virtDesktopArray.desktopCount = 0;

   /*
    * Cache the service object and use it to create the enter/exit Unity signal.
    */
   unity.serviceObj = serviceObj;
   g_signal_new(UNITY_SIG_ENTER_LEAVE_UNITY,
                G_OBJECT_TYPE(serviceObj),
                (GSignalFlags) 0,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__BOOLEAN,
                G_TYPE_NONE,
                1,
                G_TYPE_BOOLEAN);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unity_Cleanup  --
 *
 *    Exit Unity and do final cleanup.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
Unity_Cleanup()
{
   UnityPlatform *up;

   Debug("%s\n", __FUNCTION__);


   /*
    * Exit Unity.
    */
   Unity_Exit();

   unity.serviceObj = NULL;

   /*
    * Do one-time final platform-specific cleanup.
    */
   up = unity.up;
   unity.up = NULL;
   if (NULL != up) {
      UnityPlatformCleanup(up);
   }
   UnityWindowTracker_Cleanup(&unity.tracker);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unity_SetActiveDnDDetWnd  --
 *
 *    Right now we have 2 Unity DnD full screen detection window, one for version
 *    2 or older, another for version 3 or newer. This function is to set active
 *    one according to host DnD version.
 *
 *    XXX Both full-screent window is still bottom-most and is showed all time
 *    during Unity mode. Another change is needed to change it to only show the
 *    window during guest->host DnD. Also the window should not be bottom-most,
 *    but dynamicly change z-order during DnD.
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
Unity_SetActiveDnDDetWnd(UnityDnD *state)
{
   if (unity.up != NULL) {
      UnityPlatformSetActiveDnDDetWnd(unity.up, state);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unity_Exit  --
 *
 *    Called everytime we exit Unity. This function can be called when we are
 *    not in Unity mode. Right now it is called every time a 'reset' TCLO command
 *    is sent to the guest. Therefore, there's no guarantee that we were in the
 *    Unity mode when this function is called.
 *
 *    Try to do the following:
 *    Restore system settings if needed.
 *    Hide the unity dnd detection window.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Restores system settings since we are exiting Unity.
 *    Hides the unity dnd detection window if needed.
 *    Sets unity.isEnabled to FALSE
 *
 *-----------------------------------------------------------------------------
 */

void
Unity_Exit(void)
{
   int featureIndex = 0;

   if (unity.isEnabled) {
      /*
       * Reset any Unity Options - they'll be re-enabled as required before the
       * next UnityTcloEnter.
       */
      while (unityFeatureTable[featureIndex].featureBit != 0) {
         if (unity.currentOptions & unityFeatureTable[featureIndex].featureBit) {
            unityFeatureTable[featureIndex].setter(FALSE);
         }
         featureIndex++;
      }
      unity.currentOptions = 0;

      /* Hide full-screen detection window for Unity DnD. */
      UnityPlatformUpdateDnDDetWnd(unity.up, FALSE);

      UnityPlatformExitUnity(unity.up);

      /* Restore previously saved user settings. */
      UnityPlatformRestoreSystemSettings(unity.up);

      unity.isEnabled = FALSE;
      FireEnterUnitySignal(unity.serviceObj, FALSE);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unity_Enter  --
 *
 *    Called everytime we enter Unity.
 *
 *    Try to do the following:
 *    Save the system settings.
 *    Show the unity dnd detection window.
 *
 * Results:
 *    TRUE if Unity was entered.
 *
 * Side effects:
 *    Sets unity.isEnabled to TRUE.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Unity_Enter(void)
{
   if (!unity.isEnabled) {
      /* Save and disable certain user settings here. */
      UnityPlatformSaveSystemSettings(unity.up);

      if (!UnityPlatformEnterUnity(unity.up)) {
         UnityPlatformExitUnity(unity.up);
         UnityPlatformRestoreSystemSettings(unity.up);
         return FALSE;
      }

      /*
       * Show full-screen detection window for Unity DnD. It is a bottom-most (but
       * still in front of desktop) transparent detection window for guest->host DnD
       * as drop target. We need this window because:
       * 1. All active windows except desktop will be shown on host desktop and can
       *    accept DnD signal. This full-screen detection window will block any DnD signal
       *    (even mouse signal) to the desktop, which will fix bug 164880.
       * 2. With this full-screen but bottommost detection window, every time when user
       *    drag something out from active window, the dragEnter will always be immediately
       *    catched for Unity DnD.
       */
      UnityPlatformUpdateDnDDetWnd(unity.up, TRUE);
      FireEnterUnitySignal(unity.serviceObj, TRUE);
      unity.isEnabled = TRUE;
   }
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_GetWindowCommandList --
 *
 *     Retrieve the list of command strings supported by this library. The
 *     commands are a list of strings which each operate on a specified Unity
 *     window ID to perform operations like unmiminimize or restore.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

void
Unity_GetWindowCommandList(char ***commandList)     // OUT
{
   ASSERT(commandList != NULL);

   *commandList = unityCommandList;
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_GetWindowPath --
 *
 *      Get the information needed to re-launch a window and retrieve further information
 *      on it. windowPathUtf8 and execPathUtf8 allow a platform to specify different
 *      null terminated strings for the 'path' to the window vs. the path to the
 *      executable that launched the window. The exact meaning of the buffer contents
 *      is platform-specific.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
Unity_GetWindowPath(UnityWindowId window,     // IN: window handle
                    DynBuf *windowPathUtf8,   // IN/OUT: full path for the window
                    DynBuf *execPathUtf8)     // IN/OUT: full path for the executable
{
   return UnityPlatformGetWindowPath(unity.up, window, windowPathUtf8, execPathUtf8);
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_WindowCommand --
 *
 *      Execute the specified command for the given window ID.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

Bool
Unity_WindowCommand(UnityWindowId window,    // IN: window handle
                    const char *command)     // IN: Command name
{
   unsigned int i;
   ASSERT(command);

   for (i = 0; unityCommandTable[i].name != NULL; i++) {
      if (strcmp(unityCommandTable[i].name, command) == 0) {
         if (!unityCommandTable[i].exec(unity.up, window)) {
            Debug("%s: Unity window command %s failed.\n", __FUNCTION__, command);
            return FALSE;
         } else {
            return TRUE;
         }
      }
   }

   Debug("%s: Invalid command %s\n", __FUNCTION__, command);
   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_SetDesktopWorkAreas --
 *
 *     Sets the work areas for all screens. These are the areas
 *     to which windows will maximize.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

Bool
Unity_SetDesktopWorkAreas(UnityRect workAreas[], // IN
                          uint32 numWorkAreas)   // IN
{
   uint32 i;

   for (i = 0; i < numWorkAreas; i++) {
      if (workAreas[i].x < 0 || workAreas[i].y < 0 ||
          workAreas[i].width <= 0 || workAreas[i].height <= 0) {
         Debug("%s: Invalid work area\n", __FUNCTION__);
         return FALSE;
      }
   }

   return UnityPlatformSetDesktopWorkAreas(unity.up, workAreas, numWorkAreas);
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_SetTopWindowGroup --
 *
 *      Set the group of windows on top of all others.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

Bool
Unity_SetTopWindowGroup(UnityWindowId windows[],   // IN: array of window ids
                        unsigned int windowCount) // IN: # of windows in the array
{
   return UnityPlatformSetTopWindowGroup(unity.up, windows, windowCount);
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_GetUpdate --
 *
 *      This function is used to asynchronously collect Unity window updates
 *      and send them to the host via the guest->host channel.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
Unity_GetUpdate(Bool incremental)         // IN: Incremental vs. full update
{
   UnityPlatformDoUpdate(unity.up, incremental);
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_ConfirmOperation --
 *
 *     Confirmation from the host that an operation requiring interlock has been
 *     completed by the host.
 *
 * Results:
 *     TRUE if the confirmation could be handled sucessfully.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

Bool
Unity_ConfirmOperation(unsigned int operation,   // IN
                       UnityWindowId windowId,   // IN
                       uint32 sequence,          // IN
                       Bool allow)               // IN
{
   Bool retVal = FALSE;

   if (MINIMIZE == operation) {
      retVal = UnityPlatformConfirmMinimizeOperation(unity.up,
                                                     windowId,
                                                     sequence,
                                                     allow);
   } else {
      Debug("%s: Confirmation for unknown operation ID = %d\n", __FUNCTION__, operation);
   }
   return retVal;
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_SendMouseWheel --
 *
 *     Sends the given mouse wheel event to the window at the given location.
 *
 * Results:
 *     TRUE on success, FALSE on failure.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

Bool
Unity_SendMouseWheel(int32 deltaX,         // IN
                     int32 deltaY,         // IN
                     int32 deltaZ,         // IN
                     uint32 modifierFlags) // IN
{
   return UnityPlatformSendMouseWheel(unity.up, deltaX, deltaY, deltaZ, modifierFlags);
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_GetUpdates --
 *
 *     Get the unity window update and append it to the specified output buffer.
 *     This function can be called from two different threads: either from
 *     the main thread that is trying to execute a TCLO command (unity.get.update)
 *     or from the unity update thread that is gathering periodic updates and
 *     pushes them to the VMX as needed (by calling 'tools.unity.push.update RPC).
 *     Since this function can be called from two different threads, protect
 *     the global unity singleton with locks.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

void
Unity_GetUpdates(int flags)            //  IN: unity update flags
{
   UnityPlatformLock(unity.up);

   /*
    * Generate the update stream. This will cause our UnityUpdateCallbackFn to be
    * triggered, which will in turn lead to the callback registered with the
    * 'consumer' of this library which will do the actual update serialization.
    */
   UnityWindowTracker_RequestUpdates(&unity.tracker, flags, unity.up);

   UnityPlatformUnlock(unity.up);

   return;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityUpdateCallbackFn --
 *
 *     Callback from the unity window tracker indicating something has
 *     changed.
 *
 *     Perform any internal functions we need called as a consequence of tracker
 *     window state changing and then call the provided callback to serialize the
 *     update.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Clearly.
 *
 *----------------------------------------------------------------------------
 */

void
UnityUpdateCallbackFn(void *param,          // IN: UnityPlatform
                      UnityUpdate *update)  // IN
{
   unity.hostCallbacks.updateCB(unity.hostCallbacks.updateCbCtx, update);
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_GetWindowContents --
 *
 *     Read the correct bits off the window regardless of whether it's minimized
 *     or obscured.   Return the result as a PNG in the imageData DynBuf.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *     imageData contains PNG formatted window contents.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

Bool
Unity_GetWindowContents(UnityWindowId window,  // IN
                        DynBuf *imageData,     // IN/OUT
                        uint32 *width,         // OUT
                        uint32 *height)        // OUT
{
   return UnityPlatformGetWindowContents(unity.up, window, imageData, width, height);
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_GetIconData --
 *
 *     Read part or all of a particular icon on a window.  Return the result as a PNG in
 *     the imageData DynBuf, and also return the full length of the PNG in fullLength.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

Bool
Unity_GetIconData(UnityWindowId window,    // IN
                  UnityIconType iconType,  // IN
                  UnityIconSize iconSize,  // IN
                  uint32 dataOffset,       // IN
                  uint32 dataLength,       // IN
                  DynBuf *imageData,       // OUT
                  uint32 *fullLength)      // OUT
{
   return UnityPlatformGetIconData(unity.up, window, iconType, iconSize,
                                   dataOffset, dataLength, imageData, fullLength);
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_ShowTaskbar  --
 *
 *     Show/hide the taskbar while in Unity mode.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

void
Unity_ShowTaskbar(Bool showTaskbar)    // IN
{
   UnityPlatformShowTaskbar(unity.up, showTaskbar);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unity_ShowDesktop --
 *
 *      Shows or hides the entire VM desktop while in unity. This is useful for
 *      situations where the user must interact with a window that we cannot
 *      control programmatically, such as UAC prompts on Vista and Win7.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
Unity_ShowDesktop(Bool showDesktop) // IN
{
   UnityPlatformShowDesktop(unity.up, showDesktop);
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_MoveResizeWindow --
 *
 *      Moves and/or resizes the given window to the specified location. Does not
 *      attempt to move and/or resize window if (a) the destination rectangle does not
 *      intersect with the virtual screen rectangle, or (b) window is minimized.
 *
 *      If the input width & height match the current width & height, then this
 *      function will end up just moving the window. Similarly if the input
 *      x & y coordinates match the current coordinates, then it will end up just
 *      resizing the window.
 *
 * Results:
 *      Even if the move/resize operation is not executed or it fails, window's
 *      current coordinates are always sent back.
 *
 *      Function does not return FALSE if the attempt to move and/or resize fails.
 *      This is because the caller will be comparing input and output parameters to
 *      decide whether the window really moved and/or resized.
 *
 *      In a very rare case, when attempt to get window's current coordinates fail,
 *      returns FALSE.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
Unity_MoveResizeWindow(UnityWindowId window,      // IN: Window handle
                       UnityRect *moveResizeRect) // IN/OUT: Desired coordinates,
                                                  // before and after the operation.
{
   return UnityPlatformMoveResizeWindow(unity.up, window, moveResizeRect);
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_SetDesktopConfig --
 *
 *     Set the virtual desktop configuration as specified by the host.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

Bool
Unity_SetDesktopConfig(const UnityVirtualDesktopArray *desktopConfig) // IN
{
   if (UnityPlatformSetDesktopConfig(unity.up, desktopConfig)) {
      unity.virtDesktopArray = *desktopConfig;
      return TRUE;
   }
   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_SetDesktopActive --
 *
 *     Switch to the specified virtual desktop.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     Might change the active virtual desktop in the guest.
 *
 *----------------------------------------------------------------------------
 */

Bool
Unity_SetDesktopActive(UnityDesktopId desktopId)  // IN: Index into desktop config. array
{
   if (desktopId >= unity.virtDesktopArray.desktopCount) {
      Debug("%s: Desktop (%d) does not exist in the guest", __FUNCTION__, desktopId);
      return FALSE;
   }

   return UnityPlatformSetDesktopActive(unity.up, desktopId);
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_SetWindowDesktop --
 *
 *     Move the window to the specified desktop. The desktopId is an index
 *     into the desktop configuration array.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     Might change the active virtual desktop in the guest.
 *
 *----------------------------------------------------------------------------
 */

Bool
Unity_SetWindowDesktop(UnityWindowId windowId,    // IN
                       UnityDesktopId desktopId)  // IN
{
   if (desktopId >= unity.virtDesktopArray.desktopCount) {
      Debug("%s: The desktop (%d) does not exist in the guest", __FUNCTION__, desktopId);
      return FALSE;
   }

   /*
    * Set the desktop id for this window in the tracker.
    * We need to do this before moving the window since on MS Windows platforms
    * moving the window will hide it and there's a danger that we may enumerate the
    * hidden window before changing it's desktop ID. The Window tracker will ignore
    * hidden windows on the current desktop - which ultimately can lead to this window
    * being reaped from the tracker.
    */
   UnityWindowTracker_ChangeWindowDesktop(&unity.tracker, windowId, desktopId);

   return UnityPlatformSetWindowDesktop(unity.up, windowId, desktopId);
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_SetUnityOptions --
 *
 *     Set the Unity options - must be be called before entering Unity mode.
 *     UnityFeatures is a bitmask of features to be enabled (see unityCommon.h)
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

void
Unity_SetUnityOptions(uint32 newFeaturesMask)   // IN: UnityFeatures
{
   int featureIndex = 0;
   uint32 featuresChanged;

   if (unity.isEnabled) {
      Debug("%s: Attempting to set unity options whilst unity is enabled\n",
            __FUNCTION__);
   }

   /*
    * For each potential feature bit XOR the current mask with the newly
    * specified set, then if the bit has changed call the specific setter
    * function with TRUE/FALSE according to the new state of the bit.
    */
   featuresChanged = newFeaturesMask ^ unity.currentOptions;
   while (unityFeatureTable[featureIndex].featureBit != 0) {
      if (featuresChanged & unityFeatureTable[featureIndex].featureBit) {
         unityFeatureTable[featureIndex].setter(
            (newFeaturesMask & unityFeatureTable[featureIndex].featureBit) != 0);
      }
      featureIndex++;
   }

   unity.currentOptions = newFeaturesMask;
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_RequestWindowContents --
 *
 *     Add the requeste window IDs to a list of windows whose contents should
 *     be sent to the host. See also hostcallbacks.sendWindowContents().
 *
 * Results:
 *     TRUE if all the window IDs are valid.
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

Bool
Unity_RequestWindowContents(UnityWindowId windowIds[],   // IN
                            uint32 numWindowIds)         // IN)
{
   return UnityPlatformRequestWindowContents(unity.up, windowIds, numWindowIds);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnitySetAddHiddenWindows --
 *
 *     Set (or unset) whether hidden windows should be added to the tracker.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

void
UnitySetAddHiddenWindows(Bool enabled)
{
   /*
    * Should we add hidden windows to the tracker (the host will use the trackers
    * attribute field to display hidden windows in the appropriate manner.)
    */
   if (enabled) {
      Debug("%s: Adding hidden windows to tracker\n", __FUNCTION__);
   } else {
      Debug("%s: Do not add hidden windows to tracker\n", __FUNCTION__);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * UnitySetInterlockMinimizeOperation --
 *
 *     Set (or unset) whether window operations should be denied/delayed and
 *     relayed to the host for later confirmation.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

void
UnitySetInterlockMinimizeOperation(Bool enabled)
{
   /*
    * Should we interlock operations through the host. For example: instead of
    * allowing minimize to occur immediately in the guest should we prevent the
    * minimize of a window in the guest, then relay the minimize to the host and wait
    * for the hosts confirmation before actually minimizing the window in the guest.
    */
   if (enabled) {
      Debug("%s: Interlocking minimize operations through the host\n",
            __FUNCTION__);
   } else {
      Debug("%s: Do not interlock minimize operations through the host\n",
            __FUNCTION__);
   }
   UnityPlatformSetInterlockMinimizeOperation(unity.up, enabled);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnitySetSendWindowContents --
 *
 *     Set (or unset) whether window contents should be sent to the host.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

void
UnitySetSendWindowContents(Bool enabled)
{
   /*
    * Is the host prepared to receive scraped window contents at any time - even though
    * it may not have previously requested the window contents. Explicit requests from
    * the host will always be honored - this flag determines whether the guest will send
    * the window contents directly after a qualifying operation (like changes in the
    * z-order of a window).
    */
   if (enabled) {
      Debug("%s: Sending window contents to the host on appropriate events\n",
            __FUNCTION__);
   } else {
      Debug("%s: Do not send window contents to the host on appropriate events\n",
            __FUNCTION__);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * UnitySetDisableCompositing --
 *
 *     Set (or unset) whether the compositing features of the guest window
 *     manager should be disabled.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

void
UnitySetDisableCompositing(Bool disabled)
{
   /*
    * Does the host wish us to disable the compositing features of the guest window
    * manager. The flag only takes effect on subsequent 'enter unity' calls where
    * it is checked and used to disable compositing in the platform layer.
    */
   if (disabled) {
      Debug("%s: Window compositing will be disabled in the guest window manager.\n",
            __FUNCTION__);
   } else {
      Debug("%s: Window compositing will be enabled in the guest window manager.\n",
            __FUNCTION__);
   }
   UnityPlatformSetDisableCompositing(unity.up, disabled);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unity_SetConfigDesktopColor --
 *
 *      Set the preferred desktop background color for use when in Unity Mode. Only
 *      takes effect the next time unity mode is entered.
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
Unity_SetConfigDesktopColor(int desktopColor)   // IN
{
   UnityPlatformSetConfigDesktopColor(unity.up, desktopColor);
}


/*
 *------------------------------------------------------------------------------
 *
 * Unity_SetInitialDesktop --
 *
 *     Set a desktop specified by the desktop id as the initial state.
 *
 * Results:
 *     Returns TRUE if successful, and FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

Bool
Unity_SetInitialDesktop(UnityDesktopId desktopId)  // IN
{
   return UnityPlatformSetInitialDesktop(unity.up, desktopId);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unity_SetForceEnable --
 *
 *      Set's the flag to indicate that Unity should be forced to be enabled, rather
 *      than relying on runtime determination of the state of other dependancies.
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
Unity_SetForceEnable(Bool forceEnable)   // IN
{
   unity.forceEnable = forceEnable;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unity_InitializeDebugger --
 *
 *      Initialize the Unity Debugger. This is a graphical display inside the guest
 *      used to visualize the current state of the unity window tracker.
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
Unity_InitializeDebugger(void)
{
   UnityDebug_Init(&unity.tracker);
}


/**
 * Fire signal to broadcast when unity is entered and exited.
 *
 * @param[in] ctx tools application context
 * @param[in] enter if TRUE, unity was entered. If FALSE, unity has exited.
 */

static void
FireEnterUnitySignal(gpointer serviceObj,
                     gboolean enter)
{
   Debug("%s: enter. enter argument is set to %s\n", __FUNCTION__, enter ? "true" : "false");
   g_signal_emit_by_name(serviceObj,
                         UNITY_SIG_ENTER_LEAVE_UNITY,
                         enter);
}
