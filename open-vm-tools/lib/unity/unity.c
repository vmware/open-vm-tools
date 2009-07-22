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
 * This file implements the guest-side Unity agent as part of the VMware Tools.
 * It contains entry points for embedding within the VMware Tools User Agent and
 * handles the GuestRpc (TCLO, RPCI) interface.
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

#include "vmware.h"
#include "rpcin.h"
#include "rpcout.h"
#include "debug.h"
#include "util.h"
#include "strutil.h"
#include "region.h"
#include "unityWindowTracker.h"
#include "unityCommon.h"
#include "unity.h"
#include "unityPlatform.h"
#include "unityDebug.h"
#include "dynxdr.h"
#include "guestrpc/unityActive.h"
#include "guestCaps.h"
#include "appUtil.h"
#include <stdio.h>


/*
 * Singleton object for tracking the state of the service.
 */
typedef struct UnityState {
   UnityWindowTracker tracker;
   Bool forceEnable;
   Bool isEnabled;
   UnityVirtualDesktopArray virtDesktopArray;   // Virtual desktop configuration
   UnityUpdateChannel updateChannel;            // Unity update transmission channel.
   UnityPlatform *up; // Platform-specific state
} UnityState;

static UnityState unity;

static GuestCapabilities unityCaps[] = {
   UNITY_CAP_STATUS_UNITY_ACTIVE
};


/*
 * Helper Functions
 */

static Bool UnityUpdateState(void);
static void UnityUpdateCallbackFn(void *param, UnityUpdate *update);
static Bool UnityTcloGetUpdate(char const **result, size_t *resultLen, const char *name,
                               const char *args, size_t argsSize, void *clientData);
static Bool UnityTcloEnter(char const **result, size_t *resultLen, const char *name,
                           const char *args, size_t argsSize, void *clientData);
static Bool UnityTcloExit(char const **result, size_t *resultLen, const char *name,
                          const char *args, size_t argsSize, void *clientData);
static Bool UnityTcloGetWindowPath(char const **result, size_t *resultLen,
                                   const char *name, const char *args,
                                   size_t argsSize, void *clientData);
static Bool UnityTcloWindowCommand(char const **result,
                                   size_t *resultLen,
                                   const char *name,
                                   const char *args,
                                   size_t argsSize,
                                   void *clientData);
static Bool UnityTcloGetWindowContents(char const **result,
                                       size_t *resultLen,
                                       const char *name,
                                       const char *args,
                                       size_t argsSize,
                                       void *clientData);
static Bool UnityTcloGetIconData(char const **result,
                                 size_t *resultLen,
                                 const char *name,
                                 const char *args,
                                 size_t argsSize,
                                 void *clientData);
static Bool UnityTcloSetDesktopWorkArea(char const **result,
                                       size_t *resultLen,
                                       const char *name,
                                       const char *args,
                                       size_t argsSize,
                                       void *clientData);
static Bool UnityTcloSetTopWindowGroup(char const **result,
                                       size_t *resultLen,
                                       const char *name,
                                       const char *args,
                                       size_t argsSize,
                                       void *clientData);
static Bool UnityTcloShowTaskbar(char const **result,
                                 size_t *resultLen,
                                 const char *name,
                                 const char *args,
                                 size_t argsSize,
                                 void *clientData);
static Bool UnityTcloMoveResizeWindow(char const **result,
                                      size_t *resultLen,
                                      const char *name,
                                      const char *args,
                                      size_t argsSize,
                                      void *clientData);
static Bool UnityTcloSetDesktopConfig(char const **result,
                                      size_t *resultLen,
                                      const char *name,
                                      const char *args,
                                      size_t argsSize,
                                      void *clientData);
static Bool UnityTcloSetDesktopActive(char const **result,
                                      size_t *resultLen,
                                      const char *name,
                                      const char *args,
                                      size_t argsSize,
                                      void *clientData);
static Bool UnityTcloSetWindowDesktop(char const **result,
                                      size_t *resultLen,
                                      const char *name,
                                      const char *args,
                                      size_t argsSize,
                                      void *clientData);


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
   /* Add more commands and handlers above this. */
   { NULL, NULL }
};

/*
 * XXX:
 * According to Adar:
 *    "UnityTcloGetUpdate cannot return the contents of a DynBuf. This will leak
 *     the DynBuf's memory, since nobody at a lower level will ever free it.  It's
 *     a crappy interface, but we make due by using a static buffer to hold the
 *     results."
 *
 * We ideally would not use a static buffer because the maximum size of the
 * update is unknown.  To work around this, make the DynBuf returned in
 * UnityTcloGetUpdate file-global and recycle it across update requests.
 */

static DynBuf gTcloUpdate;


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
Unity_Init(GuestApp_Dict *conf, // IN
           int* blockedWnd)     // IN
{
   Debug("Unity_Init\n");

   /*
    * Initialize the UnityWindowTracker object.  The uwt does all the actual work
    * of computing differences between two states of the windowing system.  The
    * callbacks we register here will fire when we request an update via
    * UnityWindowTracker_RequestUpdates.  See bora/lib/unityWindowTracker for more
    * information.
    */
   UnityWindowTracker_Init(&unity.tracker, UnityUpdateCallbackFn);

   /*
    * Initialize the update channel.
    */
   if (UnityUpdateChannelInit(&unity.updateChannel) == FALSE) {
      Warning("%s: Unable to initialize Unity update channel.\n", __FUNCTION__);
      return;
   }

   /*
    * Initialize the host-specific portion of the unity service.
    */
   unity.up = UnityPlatformInit(&unity.tracker, &unity.updateChannel, blockedWnd);

   /*
    * Init our global dynbuf used to send results back.
    */
   DynBuf_Init(&gTcloUpdate);

   /*
    * If debugging has been enabled, initialize the debug module.  On Windows,
    * this will pop up a small HUD window which shows an echo of the current
    * state of the windowing system.
    */
   if (GuestApp_GetDictEntryBool(conf, "unity.debug")) {
      UnityDebug_Init(&unity.tracker);
   }

   /*
    * Check if the user specified the option to always enable unity regardless
    * of the guest OS type.
    */

   unity.forceEnable = GuestApp_GetDictEntryBool(conf, "unity.forceEnable");
   unity.isEnabled = FALSE;

   unity.virtDesktopArray.desktopCount = 0;
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
Unity_Cleanup(void)
{
   UnityPlatform *up;

   Debug("%s\n", __FUNCTION__);


   /*
    * Exit Unity.
    */
   Unity_Exit();

   /*
    * Do one-time final platform-specific cleanup.
    */
   up = unity.up;
   unity.up = NULL;
   UnityPlatformCleanup(up);

   UnityUpdateChannelCleanup(&unity.updateChannel);
   UnityWindowTracker_Cleanup(&unity.tracker);
   DynBuf_Destroy(&gTcloUpdate);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unity_InitBackdoor  --
 *
 *    One time initialization stuff for the backdoor.
 *
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
Unity_InitBackdoor(struct RpcIn *rpcIn)   // IN
{
   /*
    * Only register the callback if the guest is capable of supporting Unity.
    * This way, if the VMX/UI sends us a Unity request on a non-supported platform
    * (for whatever reason), we will reply with 'command not supported'.
    */

   if (Unity_IsSupported()) {
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_ENTER, UnityTcloEnter, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_GET_UPDATE_FULL, UnityTcloGetUpdate, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_GET_UPDATE_INCREMENTAL,
                             UnityTcloGetUpdate, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_GET_WINDOW_PATH,
                             UnityTcloGetWindowPath, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_WINDOW_SETTOP,
                             UnityTcloSetTopWindowGroup, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_WINDOW_CLOSE,
                             UnityTcloWindowCommand, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_GET_WINDOW_CONTENTS,
                             UnityTcloGetWindowContents, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_GET_ICON_DATA,
                             UnityTcloGetIconData, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_DESKTOP_WORK_AREA_SET,
                             UnityTcloSetDesktopWorkArea, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_SHOW_TASKBAR, UnityTcloShowTaskbar, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_EXIT, UnityTcloExit, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_WINDOW_MOVE_RESIZE,
                             UnityTcloMoveResizeWindow, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_WINDOW_SHOW,
                             UnityTcloWindowCommand, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_WINDOW_HIDE,
                             UnityTcloWindowCommand, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_WINDOW_MINIMIZE,
                             UnityTcloWindowCommand, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_WINDOW_UNMINIMIZE,
                             UnityTcloWindowCommand, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_WINDOW_MAXIMIZE,
                             UnityTcloWindowCommand, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_WINDOW_UNMAXIMIZE,
                             UnityTcloWindowCommand, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_DESKTOP_CONFIG_SET,
                             UnityTcloSetDesktopConfig, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_DESKTOP_ACTIVE_SET,
                             UnityTcloSetDesktopActive, NULL);
      RpcIn_RegisterCallback(rpcIn, UNITY_RPC_WINDOW_DESKTOP_SET,
                             UnityTcloSetWindowDesktop, NULL);
   }
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
   UnityPlatformSetActiveDnDDetWnd(unity.up, state);
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
 *    Kills all unity helper threads if any are running.
 *    Hide the unity dnd detection window.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Restores system settings since we are exiting Unity.
 *    Kills all unity helper threads if any.
 *    Hides the unity dnd detection window if needed.
 *
 *-----------------------------------------------------------------------------
 */

void
Unity_Exit(void)
{
   if (unity.isEnabled) {
      /* Hide full-screen detection window for Unity DnD. */
      UnityPlatformUpdateDnDDetWnd(unity.up, FALSE);

      /* Kill Unity helper threads. */
      UnityPlatformKillHelperThreads(unity.up);

      /* Restore previously saved user settings. */
      UnityPlatformRestoreSystemSettings(unity.up);

      unity.isEnabled = FALSE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unity_RegisterCaps  --
 *
 *    Called by the application (VMwareUser) to allow the unity subsystem to
 *    register its capabilities.
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
Unity_RegisterCaps(void)
{
   /*
    * Send Unity capability.
    */

   if (!RpcOut_sendOne(NULL, NULL, UNITY_RPC_UNITY_CAP" %d",
                       Unity_IsSupported() ? 1 : 0)) {
      Debug("%s: could not set unity capability\n", __FUNCTION__);
   }

   /*
    * Register guest platform specific capabilities.
    */

   UnityPlatformRegisterCaps(unity.up);
   AppUtil_SendGuestCaps(unityCaps, ARRAYSIZE(unityCaps), TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unity_UnregisterCaps  --
 *
 *    Called by the application (VMwareUser) to allow the unity subsystem to
 *    unregister its capabilities.
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
Unity_UnregisterCaps(void)
{
   /*
    * Unregister guest platform specific capabilities.
    */

   UnityPlatformUnregisterCaps(unity.up);

   /*
    * Unregister the unity capability.
    */

   if (!RpcOut_sendOne(NULL, NULL, UNITY_RPC_UNITY_CAP" 0")) {
      Debug("Failed to unregister Unity capability\n");
   }
   AppUtil_SendGuestCaps(unityCaps, ARRAYSIZE(unityCaps), FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloEnter --
 *
 *     RPC handler for 'unity.enter'. Save and disable certain user
 *     settings. Start Unity updates thread and any other platform
 *     specific threads (like a thread that listens for
 *     the desktop switch event on Windows). Note that we first set
 *     the UI settings, and then start the threads. This way the UI
 *     settings take effect before we start sending Unity updates,
 *     so that we never send things like task bar (see bug 166085).
 *
 * Results:
 *     TRUE if helper threads were started.
 *     FALSE otherwise.
 *
 * Side effects:
 *     Certain UI system settings will be disabled.
 *     Unity update thread will be started.
 *     Any other platform specific helper threads will be started as well.
 *
 *----------------------------------------------------------------------------
 */

static Bool
UnityTcloEnter(char const **result,     // OUT
               size_t *resultLen,       // OUT
               const char *name,        // IN
               const char *args,        // IN
               size_t argsSize,         // ignored
               void *clientData)        // ignored
{
   Debug("%s\n", __FUNCTION__);

   if (!unity.isEnabled) {
      /* Save and disable certain user settings here. */
      UnityPlatformSaveSystemSettings(unity.up);

      /* Start Unity helper threads. */
      if (!UnityPlatformStartHelperThreads(unity.up)) {

         /*
          * If we couldn't start one or more helper threads,
          * we cannot enter Unity. Kill all running helper
          * threads and restore ui settings.
          */

         UnityPlatformKillHelperThreads(unity.up);
         UnityPlatformRestoreSystemSettings(unity.up);
         return RpcIn_SetRetVals(result, resultLen,
                                 "Could not start unity helper threads", FALSE);
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
      unity.isEnabled = TRUE;
   }

   UnityUpdateState();

   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloExit --
 *
 *     RPC handler for 'unity.exit'.
 *
 * Results:
 *     Always TRUE.
 *
 * Side effects:
 *     Same as side effects of Unity_Exit().
 *
 *----------------------------------------------------------------------------
 */

static Bool
UnityTcloExit(char const **result,     // OUT
              size_t *resultLen,       // OUT
              const char *name,        // IN
              const char *args,        // IN
              size_t argsSize,         // ignored
              void *clientData)        // ignored
{
   Debug("UnityTcloExit.\n");

   Unity_Exit();

   UnityUpdateState();
   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloGetWindowPath --
 *
 *      RPC handler for UNITY_RPC_GET_WINDOW_PATH.
 *
 *      Get the information needed to re-launch a window and retrieve further
 *      information on it.  Returns double-NUL-terminated buffer consisting of
 *      NUL-terminated strings "windowPath" and "execPath" strings, the first
 *      uniquely identifying the window and the second uniquely identifying the
 *      window's owning executable.
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

static Bool
UnityTcloGetWindowPath(char const **result,     // OUT
                       size_t *resultLen,       // OUT
                       const char *name,        // IN
                       const char *args,        // IN
                       size_t argsSize,         // ignored
                       void *clientData)        // ignored

{
   UnityWindowId window;
   DynBuf windowPathUtf8;
   DynBuf execPathUtf8;

   unsigned int index = 0;
   Bool ret = TRUE;

   Debug("UnityTcloGetWindowPath name:%s args:'%s'\n", name, args);

   /* Parse the command & window id.*/

   if (!StrUtil_GetNextIntToken(&window, &index, args, " ")) {
      Debug("UnityTcloGetWindowInfo: Invalid RPC arguments.\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "Invalid arguments. Expected \"windowId\"",
                              FALSE);
   }

   Debug("UnityTcloGetWindowInfo: window %d\n", window);

   /*
    * Please note that the UnityPlatformGetWindowPath implementations assume that the
    * dynbuf passed in does not contain any existing data that needs to be appended to,
    * so this code should continue to accomodate that assumption.
    */
   DynBuf_Destroy(&gTcloUpdate);
   DynBuf_Init(&gTcloUpdate);
   DynBuf_Init(&windowPathUtf8);
   DynBuf_Init(&execPathUtf8);
   if (!UnityPlatformGetWindowPath(unity.up, window, &windowPathUtf8, &execPathUtf8)) {
      Debug("UnityTcloGetWindowInfo: Could not get window path.\n");
      ret = RpcIn_SetRetVals(result, resultLen,
                             "Could not get window path",
                             FALSE);
      goto exit;
   }

   /*
    * Construct the buffer holding the result. Note that we need to use gTcloUpdate
    * here to avoid leaking during the RPC handler.
    */
   DynBuf_Copy(&windowPathUtf8, &gTcloUpdate);
   DynBuf_Append(&gTcloUpdate, DynBuf_Get(&execPathUtf8), DynBuf_GetSize(&execPathUtf8));

   /*
    * Write the final result into the result out parameters and return!
    */
   *result = (char *)DynBuf_Get(&gTcloUpdate);
   *resultLen = DynBuf_GetSize(&gTcloUpdate);

exit:
   DynBuf_Destroy(&windowPathUtf8);
   DynBuf_Destroy(&execPathUtf8);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloWindowCommand --
 *
 *     RPC handler for 'unity.window.*' (excluding 'unity.window.settop')
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

static Bool
UnityTcloWindowCommand(char const **result,     // OUT
                       size_t *resultLen,       // OUT
                       const char *name,        // IN
                       const char *args,        // IN
                       size_t argsSize,         // ignored
                       void *clientData)        // ignored
{
   UnityWindowId window;
   unsigned int index = 0;
   unsigned int i;

   Debug("UnityTcloWindowCommand: name:%s args:'%s'\n", name, args);

   /* Parse the command & window id.*/

   if (!StrUtil_GetNextIntToken(&window, &index, args, " ")) {
      Debug("UnityTcloWindowCommand: Invalid RPC arguments.\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "Invalid arguments. Expected \"windowId\"",
                              FALSE);

   }

   Debug("UnityTcloWindowCommand: %s window %d\n", name, window);

   for (i = 0; unityCommandTable[i].name != NULL; i++) {
      if (strcmp(unityCommandTable[i].name, name) == 0) {
         if (!unityCommandTable[i].exec(unity.up, window)) {
            Debug("Unity window command failed.\n");
            return RpcIn_SetRetVals(result, resultLen,
                                   "Could not execute window command",
                                   FALSE);
         } else {
            return RpcIn_SetRetVals(result, resultLen, "", TRUE);
         }
      }
   }

   return RpcIn_SetRetVals(result, resultLen, "Bad command", FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloSetDesktopWorkArea --
 *
 *     RPC handler for 'unity.desktop.work_area.set'.
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

static Bool
UnityTcloSetDesktopWorkArea(char const **result,  // IN
                            size_t *resultLen,    // IN
                            const char *name,     // IN
                            const char *args,     // IN
                            size_t argsSize,      // IN
                            void *clientData)     // IN
{
   Bool success = FALSE;
   unsigned int count;
   unsigned int i;
   UnityRect *workAreas = NULL;

   /*
    * The argument string will look something like:
    *   <count> [ , <x> <y> <w> <h> ] * count.
    *
    * e.g.
    *    3 , 0 0 640 480 , 640 0 800 600 , 0 480 640 480
    */

   if (sscanf(args, "%u", &count) != 1) {
      return RpcIn_SetRetVals(result, resultLen,
                              "Invalid arguments. Expected \"count\"",
                              FALSE);
   }

   workAreas = (UnityRect *)malloc(sizeof *workAreas * count);
   if (!workAreas) {
      RpcIn_SetRetVals(result, resultLen,
                       "Failed to alloc buffer for work areas",
                       FALSE);
      goto out;
   }

   for (i = 0; i < count; i++) {
      args = strchr(args, ',');
      if (!args) {
         RpcIn_SetRetVals(result, resultLen,
                          "Expected comma separated display list",
                          FALSE);
         goto out;
      }
      args++; /* Skip past the , */

      if (sscanf(args, " %d %d %d %d ",
                 &workAreas[i].x, &workAreas[i].y,
                 &workAreas[i].width, &workAreas[i].height) != 4) {
         RpcIn_SetRetVals(result, resultLen,
                          "Expected x, y, w, h in display entry",
                          FALSE);
         goto out;
      }
   }

   if (!UnityPlatformSetDesktopWorkAreas(unity.up, workAreas, count)) {
      RpcIn_SetRetVals(result, resultLen,
                       "UnityPlatformSetDesktopWorkAreas failed",
                       FALSE);
      goto out;
   }

   success = RpcIn_SetRetVals(result, resultLen, "", TRUE);

out:
   free(workAreas);
   return success;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloSetTopWindowGroup --
 *
 *     RPC handler for 'unity.window.settop'.
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

static Bool
UnityTcloSetTopWindowGroup(char const **result,     // OUT
                           size_t *resultLen,       // OUT
                           const char *name,        // IN
                           const char *args,        // IN
                           size_t argsSize,         // ignored
                           void *clientData)        // ignored
{
   UnityWindowId window;
   unsigned int index = 0;
   unsigned int windowCount = 0;
   UnityWindowId windows[UNITY_MAX_SETTOP_WINDOW_COUNT];

   Debug("%s: name:%s args:'%s'\n", __FUNCTION__, name, args);

   /* Parse the command & window ids.*/

   while (StrUtil_GetNextUintToken(&window, &index, args, " ")) {
      windows[windowCount] = window;
      windowCount++;
      if (windowCount == UNITY_MAX_SETTOP_WINDOW_COUNT) {
         Debug("%s: Too many windows.\n", __FUNCTION__);
         return RpcIn_SetRetVals(result, resultLen,
                                 "Invalid arguments. Too many windows",
                                 FALSE);
      }
   }

   if (windowCount == 0) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RpcIn_SetRetVals(result, resultLen,
                              "Invalid arguments. Expected at least one windowId",
                              FALSE);
   }

   if (!UnityPlatformSetTopWindowGroup(unity.up, windows, windowCount)) {
      return RpcIn_SetRetVals(result, resultLen,
                              "Could not execute window command",
                              FALSE);
   }

   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloGetUpdate --
 *
 *     RPC handler for 'unity.get.update'.  Ask the unity window tracker
 *     to give us an update (either incremental or non-incremental based
 *     on whether the 'incremental' arg is present) and send the result
 *     back to the VMX.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Clearly.
 *
 *----------------------------------------------------------------------------
 */

static Bool
UnityTcloGetUpdate(char const **result,     // OUT
                   size_t *resultLen,       // OUT
                   const char *name,        // IN
                   const char *args,        // IN
                   size_t argsSize,         // ignored
                   void *clientData)        // ignored
{
   Bool incremental = FALSE;

   Debug("UnityTcloGetUpdate name:%s args:'%s'", name, args);

   /*
    * Specify incremental or non-incremetal updates based on whether or
    * not the client set the "incremental" arg.
    */
   if (strstr(name, "incremental")) {
      incremental = TRUE;
   }

   /*
    * Call into platform-specific implementation to gather and send updates
    * back via RPCI.  (This is done to ensure all updates are sent to the
    * Unity server in sequence via the same channel.)
    */
   UnityPlatformDoUpdate(unity.up, incremental);

   /*
    * To maintain compatibility, we'll return a successful but empty response.
    */
   *result = "";
   *resultLen = 0;

   /*
    * Give the debugger a crack to do something interesting at this point
    *
    * XXX Not sure if this is worth keeping around since this routine no
    * longer returns updates directly.
    */
   UnityDebug_OnUpdate();

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityGetUpdateCommon --
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
UnityGetUpdateCommon(int flags,     //  IN: unity update flags
                     DynBuf *buf)   //  IN/OUT: unity update buffer
{

   ASSERT(buf);

   UnityPlatformLock(unity.up);

   /*
    * Ask the guest to crawl the windowing system and push updates
    * into the unity window tracker.  If the guest backend isn't able to get
    * notification of destroyed windows, UnityPlatformUpdateWindowState will
    * return TRUE, which is are signal to set the UNITY_UPDATE_REMOVE_UNTOUCHED
    * flag.  This make the unity window tracker generate remove events for
    * windows that it hasn't seen an update for since the last update
    * request.
    */
   if (UnityPlatformUpdateWindowState(unity.up, &unity.tracker)) {
      flags |= UNITY_UPDATE_REMOVE_UNTOUCHED;
   }

   /*
    * Generate the update string.  We'll accumulate updates in the DynBuf
    * buf via the callbacks registered in Unity_Init().  Each update will
    * append a null terminated string to buf.
    */
   UnityWindowTracker_RequestUpdates(&unity.tracker, flags, buf);

   UnityPlatformUnlock(unity.up);

   /*
    * Write the final '\0' to the DynBuf to signal that we're all out of
    * updates.
    */
   DynBuf_AppendString(buf, "");

   return;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityUpdateCallbackFn --
 *
 *     Callback from the unity window tracker indicating something's
 *     changed.
 *
 *     Write the update string into our dynbuf accumlating the update
 *     and return.
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
UnityUpdateCallbackFn(void *param,          // IN: dynbuf
                      UnityUpdate *update)  // IN
{
   DynBuf *buf = (DynBuf *)param;
   char data[1024];
   int i, n, count = 0;
   RegionPtr region;
   char *titleUtf8 = NULL;
   char *windowPathUtf8 = "";
   char *execPathUtf8 = "";

   switch (update->type) {

   case UNITY_UPDATE_ADD_WINDOW:
      if (DynBuf_GetSize(&update->u.addWindow.windowPathUtf8) > 0) {
         windowPathUtf8 = DynBuf_Get(&update->u.addWindow.windowPathUtf8);
      }
      if (DynBuf_GetSize(&update->u.addWindow.execPathUtf8) > 0) {
         execPathUtf8 = DynBuf_Get(&update->u.addWindow.execPathUtf8);
      }

      Str_Sprintf(data, sizeof data, "add %u windowPath=%s execPath=%s",
                  update->u.addWindow.id,
                  windowPathUtf8,
                  execPathUtf8);
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_MOVE_WINDOW:
      Str_Sprintf(data, sizeof data, "move %u %d %d %d %d",
                  update->u.moveWindow.id,
                  update->u.moveWindow.rect.x1,
                  update->u.moveWindow.rect.y1,
                  update->u.moveWindow.rect.x2,
                  update->u.moveWindow.rect.y2);
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_REMOVE_WINDOW:
      Str_Sprintf(data, sizeof data, "remove %u", update->u.removeWindow.id);
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_REGION:
      /*
       * A null region indicates that the region should be deleted.
       * Make sure we write "region <id> 0" for the reply.
       */
      region = update->u.changeWindowRegion.region;
      if (region) {
         count = REGION_NUM_RECTS(region);
      }
      Str_Sprintf(data, sizeof data, "region %u %d",
                  update->u.changeWindowRegion.id, count);
      DynBuf_AppendString(buf, data);

      for (i = 0; i < count; i++) {
         BoxPtr p = REGION_RECTS(region) + i;
         Str_Sprintf(data, sizeof data, "rect %d %d %d %d",
                     p->x1, p->y1, p->x2, p->y2);
         DynBuf_AppendString(buf, data);
      }
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_TITLE:
      titleUtf8 = DynBuf_Get(&update->u.changeWindowTitle.titleUtf8);

      if (titleUtf8 &&
          (DynBuf_GetSize(&update->u.changeWindowTitle.titleUtf8) ==
           strlen(titleUtf8) + 1)) {
           Str_Sprintf(data, sizeof data, "title %u ",
                       update->u.changeWindowTitle.id);
           Str_Strncat(data, sizeof data, titleUtf8, sizeof data - strlen(data) - 1);
           data[sizeof data - 1] = '\0';
      } else {
         Str_Sprintf(data, sizeof data, "title %u",
                     update->u.changeWindowTitle.id);
      }
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_CHANGE_ZORDER:
      n = Str_Snprintf(data, sizeof data, "zorder %d", update->u.zorder.count);
      DynBuf_Append(buf, data, n);
      for (i = 0; i < update->u.zorder.count; i++) {
         n = Str_Snprintf(data, sizeof data, " %d", update->u.zorder.ids[i]);
         DynBuf_Append(buf, data, n);
      }
      DynBuf_AppendString(buf, ""); // for appending NULL
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_STATE:
      Str_Sprintf(data, sizeof data, "state %u %u",
                  update->u.changeWindowState.id,
                  update->u.changeWindowState.state);
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_ATTRIBUTE:
      Str_Sprintf(data, sizeof data, "attr %u %u %u",
                  update->u.changeWindowAttribute.id,
                  update->u.changeWindowAttribute.attr,
                  update->u.changeWindowAttribute.value);
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_TYPE:
      Str_Sprintf(data, sizeof data, "type %u %d",
                  update->u.changeWindowType.id,
                  update->u.changeWindowType.winType);
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_ICON:
      Str_Sprintf(data, sizeof data, "icon %u %u",
                  update->u.changeWindowIcon.id,
                  update->u.changeWindowIcon.iconType);
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_DESKTOP:
      Str_Sprintf(data, sizeof data, "desktop %u %d",
                  update->u.changeWindowDesktop.id,
                  update->u.changeWindowDesktop.desktopId);
      DynBuf_AppendString(buf, data);
      break;

   case UNITY_UPDATE_CHANGE_ACTIVE_DESKTOP:
      Str_Sprintf(data, sizeof data, "activedesktop %d",
                  update->u.changeActiveDesktop.desktopId);
      DynBuf_AppendString(buf, data);
      break;

   default:
      NOT_IMPLEMENTED();
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityUpdateChannelInit --
 *
 *      Initialize the state for the update thread.
 *
 * Return value:
 *      TRUE if all needed data was initialized.
 *      FALSE otherwise
 *
 * Side effects:
 *      RpcOut channel might be open.
 *      Memory for the update buffer might be allocated.
 *
 *-----------------------------------------------------------------------------
 */

Bool
UnityUpdateChannelInit(UnityUpdateChannel *updateChannel) // IN
{
   ASSERT(updateChannel);

   updateChannel->rpcOut = NULL;
   updateChannel->cmdSize = 0;

   DynBuf_Init(&updateChannel->updates);
   DynBuf_AppendString(&updateChannel->updates, UNITY_RPC_PUSH_UPDATE_CMD " ");

   /* Exclude the null. */
   updateChannel->cmdSize = DynBuf_GetSize(&updateChannel->updates) - 1;

   updateChannel->rpcOut = RpcOut_Construct();
   if (updateChannel->rpcOut == NULL) {
      goto error;
   }

   if (!RpcOut_start(updateChannel->rpcOut)) {
      RpcOut_Destruct(updateChannel->rpcOut);
      goto error;
   }

   return TRUE;

error:
   DynBuf_Destroy(&updateChannel->updates);

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityUpdateChannelCleanup --
 *
 *      Cleanup the unity update thread state.
 *
 * Return value:
 *      None.
 *
 * Side effects:
 *      RpcOut channel will be closed.
 *      Memory will be freed.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityUpdateChannelCleanup(UnityUpdateChannel *updateChannel) // IN
{
   if (updateChannel && updateChannel->rpcOut) {
      RpcOut_stop(updateChannel->rpcOut);
      RpcOut_Destruct(updateChannel->rpcOut);
      updateChannel->rpcOut = NULL;

      DynBuf_Destroy(&updateChannel->updates); // Avoid double-free by guarding this as well
   }
}


#ifdef VMX86_DEVEL
/*
 *-----------------------------------------------------------------------------
 *
 * DumpUpdate --
 *
 *      Prints a Unity update via debug output.  NUL is represented as '!'.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
DumpUpdate(UnityUpdateChannel *updateChannel)   // IN
{
   int i, len;
   char *buf = NULL;

   len = updateChannel->updates.size;
   buf = Util_SafeMalloc(len + 1);
   memcpy(buf, updateChannel->updates.data, len);
   buf[len] = '\0';
   for (i = 0 ; i < len; i++) {
      if (buf[i] == '\0') {
         buf[i] = '!';
      }
   }

   Debug("%s: Sending update: %s\n", __FUNCTION__, buf);

   free(buf);
}
#endif // ifdef VMX86_DEVEL


/*
 *-----------------------------------------------------------------------------
 *
 * UnitySendUpdates --
 *
 *      Gather and send a round of unity updates. The caller is responsible
 *      for gathering updates into updateChannel->updates buffer prior to the
 *      function call. This function should only be called if there's data
 *      in the update buffer to avoid sending empty update string to the VMX.
 *
 * Return value:
 *      TRUE if the update was sent,
 *      FALSE if something went wrong (an invalid RPC channel, for example).
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
UnitySendUpdates(UnityUpdateChannel *updateChannel) // IN
{
   char const *myReply;
   size_t myRepLen;
   Bool retry = FALSE;

   ASSERT(updateChannel);
   ASSERT(updateChannel->rpcOut);

   /* Send 'tools.unity.push.update <updates>' to the VMX. */

#ifdef VMX86_DEVEL
   DumpUpdate(updateChannel);
#endif

retry_send:
   if (!RpcOut_send(updateChannel->rpcOut,
                    (char *)DynBuf_Get(&updateChannel->updates),
                    DynBuf_GetSize(&updateChannel->updates),
                    &myReply, &myRepLen)) {

      /*
       * We could not send the RPC. If we haven't tried to reopen
       * the channel, try to reopen and resend. If we already
       * tried to resend, then it's time to give up. I hope that
       * trying to resend once is enough.
       */

      if (!retry) {
         retry = TRUE;
         Debug("%s: could not send rpc. Reopening channel.\n", __FUNCTION__);
         RpcOut_stop(updateChannel->rpcOut);
         if (!RpcOut_start(updateChannel->rpcOut)) {
            Debug("%s: could not reopen rpc channel. Exiting...\n", __FUNCTION__);
            return FALSE;
         }
         goto retry_send;

      } else {
         Debug("%s: could not resend rpc. Giving up and exiting...\n", __FUNCTION__);
         return FALSE;
      }
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloGetWindowContents --
 *
 *     RPC handler for 'unity.get.window.contents'. Suck the bits off the
 *     window and return a .png image over the backdoor.
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

static Bool
UnityTcloGetWindowContents(char const **result,     // OUT
                           size_t *resultLen,       // OUT
                           const char *name,        // IN
                           const char *args,        // IN
                           size_t argsSize,         // ignored
                           void *clientData)        // ignored
{
   unsigned int window;
   unsigned int index = 0;
   DynBuf *imageData = &gTcloUpdate;

   Debug("UnityTcloGetWindowContents: name:%s args:'%s'\n", name, args);

   /*
    * Parse the command & window id.
    */
   if (!StrUtil_GetNextIntToken(&window, &index, args, " ")) {
      Debug("UnityTcloGetWindowContents: Invalid RPC arguments.\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "failed: arguments. Expected \"windowId\"",
                              FALSE);

   }
   Debug("UnityTcloGetWindowContents: window %d\n", window);

   /*
    * Read the contents of the window, compress it as a .png and
    * send the .png back to the vmx as the RPC result.
    */
   DynBuf_SetSize(imageData, 0);
   if (!UnityPlatformGetWindowContents(unity.up, window, imageData)) {
      return RpcIn_SetRetVals(result, resultLen,
                              "failed: Could not read window contents",
                              FALSE);
   }

   *result = (char *)DynBuf_Get(imageData);
   *resultLen = DynBuf_GetSize(imageData);

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloGetIconData --
 *
 *     RPC handler for 'unity.get.icon.data'. Suck the bits off the
 *     window and return a .png image over the backdoor.
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

static Bool
UnityTcloGetIconData(char const **result,     // OUT
                     size_t *resultLen,       // OUT
                     const char *name,        // IN
                     const char *args,        // IN
                     size_t argsSize,         // ignored
                     void *clientData)        // ignored
{
   UnityWindowId window;
   UnityIconType iconType;
   UnityIconSize iconSize;
   unsigned int dataOffset, dataLength;
   uint32 fullLength;
   size_t retLength;
   DynBuf *results = &gTcloUpdate, imageData;
   char data[1024];

   Debug("UnityTcloGetIconData: name:%s args:'%s'\n", name, args);

   /*
    * Parse the arguments.
    */
   if ((sscanf(args, "%u %u %u %u %u",
               &window,
               &iconType,
               &iconSize,
               &dataOffset,
               &dataLength) != 5)
       || (dataLength > UNITY_MAX_ICON_DATA_CHUNK)) {
      Debug("UnityTcloGetIconData: Invalid RPC arguments.\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "failed: arguments missing",
                              FALSE);
   }

   Debug("%s: window %u iconType %u" \
         " iconSize %u dataOffset %u dataLength %u\n",
         __FUNCTION__,
         window, iconType, iconSize, dataOffset, dataLength);

   /*
    * Retrieve part/all of the icon in PNG format.
    */
   DynBuf_Init(&imageData);
   if (!UnityPlatformGetIconData(unity.up, window, iconType, iconSize,
                                 dataOffset, dataLength, &imageData, &fullLength)) {
      return RpcIn_SetRetVals(result, resultLen,
                              "failed: Could not read icon data properly",
                              FALSE);
   }


   DynBuf_SetSize(results, 0);
   retLength = DynBuf_GetSize(&imageData);
   retLength = MIN(retLength, UNITY_MAX_ICON_DATA_CHUNK);
   DynBuf_Append(results, data, Str_Snprintf(data, sizeof data, "%u %" FMTSZ "u ",
                                             fullLength, retLength));
   DynBuf_Append(results, DynBuf_Get(&imageData), retLength);

   /*
    * Guarantee that the results have a trailing \0 in case anything does a strlen...
    */
   DynBuf_AppendString(results, "");
   *result = (char *)DynBuf_Get(results);
   *resultLen = DynBuf_GetSize(results);
   DynBuf_Destroy(&imageData);

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloShowTaskbar --
 *
 *     RPC handler for 'unity.show.taskbar'.
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

static Bool
UnityTcloShowTaskbar(char const **result,     // OUT
                     size_t *resultLen,       // OUT
                     const char *name,        // IN
                     const char *args,        // IN
                     size_t argsSize,         // IN: Size of args
                     void *clientData)        // ignored
{
   uint32 command = 0;
   unsigned int index = 0;

   Debug("%s: name:%s args:'%s'\n", __FUNCTION__, name, args);

   if (!StrUtil_GetNextUintToken(&command, &index, args, " ")) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RpcIn_SetRetVals(result, resultLen,
                              "Invalid arguments.",
                              FALSE);
   }

   Debug("%s: command %d\n", __FUNCTION__, command);

   UnityPlatformShowTaskbar(unity.up, (command == 0) ? FALSE : TRUE);

   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloMoveResizeWindow --
 *
 *     RPC handler for 'unity.window.move_resize'.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *     If successful adds null terminated strings for each output coordinates.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
UnityTcloMoveResizeWindow(char const **result,     // OUT
                          size_t *resultLen,       // OUT
                          const char *name,        // IN
                          const char *args,        // IN
                          size_t argsSize,         // IN: Size of args
                          void *clientData)        // ignored
{
   DynBuf *buf = &gTcloUpdate;
   UnityWindowId window;
   UnityRect moveResizeRect = {0};
   char temp[1024];

   Debug("%s: name:%s args:'%s'\n", __FUNCTION__, name, args);

   if (sscanf(args, "%u %d %d %d %d",
              &window,
              &moveResizeRect.x,
              &moveResizeRect.y,
              &moveResizeRect.width,
              &moveResizeRect.height) != 5) {
      Debug("%s: Invalid RPC arguments.\n", __FUNCTION__);
      return RpcIn_SetRetVals(result, resultLen,
                              "Invalid arguments.",
                              FALSE);
   }

   if (!UnityPlatformMoveResizeWindow(unity.up, window, &moveResizeRect)) {
      Debug("%s: Could not read window coordinates.\n", __FUNCTION__);
      return RpcIn_SetRetVals(result, resultLen,
                              "Could not read window coordinates",
                              FALSE);
   }

   /*
    *  Send back the new (post move/resize operation) window coordinates.
    */

   DynBuf_SetSize(buf, 0);
   Str_Sprintf(temp, sizeof temp, "%d %d %d %d", moveResizeRect.x,
               moveResizeRect.y, moveResizeRect.width, moveResizeRect.height);
   DynBuf_AppendString(buf, temp);

   /*
    * Write the final result into the result out parameters and return!
    */

   *result = (char *)DynBuf_Get(buf);
   *resultLen = DynBuf_GetSize(buf);

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloSetDesktopConfig --
 *
 *     RPC handler for 'unity.set.desktop.config'. The RPC takes the form of:
 *     {1,1} {1,2} {2,1} {2,2} 1
 *     for a 2 x 2 virtual desktop where the upper right {1,2} is the currently
 *     active desktop.
 *
 * Results:
 *     TRUE if everything is successful.
 *     FALSE otherwise.
 *
 * Side effects:
 *     Might change virtual desktop configuration in the guest.
 *
 *----------------------------------------------------------------------------
 */

static Bool
UnityTcloSetDesktopConfig(char const **result,  // OUT
                          size_t *resultLen,    // OUT
                          const char *name,     // IN
                          const char *args,     // IN
                          size_t argsSize,      // IN
                          void *clientData)     // IN: ignored
{
   unsigned int index = 0;
   char *desktopStr = NULL;
   char *errorMsg;
   uint32 initialDesktopIndex = 0;

   Debug("%s: name:%s args:'%s'\n", __FUNCTION__, name, args);

   if (argsSize == 0) {
      errorMsg = "Invalid arguments: desktop config is expected";
      goto error;
   }

   unity.virtDesktopArray.desktopCount = 0;
   /* Read the virtual desktop configuration. */
   while ((desktopStr = StrUtil_GetNextToken(&index, args, " ")) != NULL) {
      UnityVirtualDesktop desktop;
      uint32 desktopCount = unity.virtDesktopArray.desktopCount;

      if (sscanf(desktopStr, "{%d,%d}", &desktop.x, &desktop.y) == 2) {
         if (desktopCount >= MAX_VIRT_DESK - 1) {
            errorMsg = "Invalid arguments: too many desktops";
            goto error;
         }
         unity.virtDesktopArray.desktops[desktopCount] = desktop;
         unity.virtDesktopArray.desktopCount++;
      } else if (sscanf(desktopStr, "%u", &initialDesktopIndex) == 1) {
         if (initialDesktopIndex >= unity.virtDesktopArray.desktopCount) {
            errorMsg = "Invalid arguments: current desktop is out of bounds";
            goto error;
         }
         /* All done with arguments at this point - stop processing */
         free(desktopStr);
         break;
      } else {
         errorMsg = "Invalid arguments: invalid desktop config";
         goto error;
      }
      free(desktopStr);
      desktopStr = NULL;
   }

   /*
    * Call the platform specific function to set the desktop configuration.
    */

   if (!UnityPlatformSetDesktopConfig(unity.up, &unity.virtDesktopArray)) {
      errorMsg = "Could not set desktop configuration";
      goto error;
   }

   if (!UnityPlatformSetInitialDesktop(unity.up, initialDesktopIndex)) {
      errorMsg = "Could not set initial desktop";
      goto error;
   }

   return RpcIn_SetRetVals(result, resultLen,
                           "",
                           TRUE);
error:
   free(desktopStr);
   unity.virtDesktopArray.desktopCount = 0;
   Debug("%s: %s\n", __FUNCTION__, errorMsg);

   return RpcIn_SetRetVals(result, resultLen,
                           errorMsg,
                           FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloSetDesktopActive --
 *
 *     RPC handler for 'unity.set.desktop.active'.
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

static Bool
UnityTcloSetDesktopActive(char const **result,  // OUT
                          size_t *resultLen,    // OUT
                          const char *name,     // IN
                          const char *args,     // IN
                          size_t argsSize,      // IN
                          void *clientData)     // IN: ignored
{
   UnityDesktopId desktopId = 0;
   char *errorMsg;

   Debug("%s: name:%s args:'%s'\n", __FUNCTION__, name, args);

   if (unity.isEnabled == FALSE) {
      errorMsg = "Unity not enabled - cannot change active desktop";
      goto error;
   }

   if (sscanf(args, " %d", &desktopId) != 1) {
      errorMsg = "Invalid arguments: expected \"desktopId\"";
      goto error;
   }

   if (desktopId >= unity.virtDesktopArray.desktopCount) {
      errorMsg = "Desktop does not exist in the guest";
      goto error;
   }

   /*
    * Call the platform specific function to set the desktop active.
    */

   if (!UnityPlatformSetDesktopActive(unity.up, desktopId)) {
      errorMsg = "Could not set active desktop";
      goto error;
   }

   /*
    * Update the uwt with the new active desktop info.
    */

   UnityWindowTracker_ChangeActiveDesktop(&unity.tracker, desktopId);

   return RpcIn_SetRetVals(result, resultLen,
                           "",
                           TRUE);
error:
   Debug("%s: %s\n", __FUNCTION__, errorMsg);
   return RpcIn_SetRetVals(result, resultLen,
                           errorMsg,
                           FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityTcloSetWindowDesktop --
 *
 *     RPC handler for 'unity.set.window.desktop'.
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

static Bool
UnityTcloSetWindowDesktop(char const **result,  // OUT
                          size_t *resultLen,    // OUT
                          const char *name,     // IN
                          const char *args,     // IN
                          size_t argsSize,      // IN
                          void *clientData)     // IN: ignored
{
   UnityWindowId windowId;
   uint32 desktopId = 0;
   char *errorMsg;

   Debug("%s: name:%s args:'%s'\n", __FUNCTION__, name, args);

   if (unity.isEnabled == FALSE) {
      errorMsg = "Unity not enabled - cannot set window desktop";
      goto error;
   }

   if (sscanf(args, " %u %d", &windowId, &desktopId) != 2) {
      errorMsg = "Invalid arguments: expected \"windowId desktopId\"";
      goto error;
   }

   if (desktopId >= unity.virtDesktopArray.desktopCount) {
      errorMsg = "The desktop does not exist in the guest";
      goto error;
   }

   /*
    * Call the platform specific function to move the window to the
    * specified desktop.
    */

   if (!UnityPlatformSetWindowDesktop(unity.up, windowId, desktopId)) {
      errorMsg = "Could not move the window to the desktop";
      goto error;
   }

   UnityWindowTracker_ChangeWindowDesktop(&unity.tracker, windowId, desktopId);

   return RpcIn_SetRetVals(result, resultLen,
                           "",
                           TRUE);
error:
   Debug("%s: %s\n", __FUNCTION__, errorMsg);
   return RpcIn_SetRetVals(result, resultLen,
                           errorMsg,
                           FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityUpdateState --
 *
 *     Communicate unity state changes to vmx.
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

static Bool
UnityUpdateState(void)
{
   Bool ret = TRUE;
   XDR xdrs;
   UnityActiveProto message;
   char *val;

   if (DynXdr_Create(&xdrs) == NULL) {
      return FALSE;
   }

   val = Str_Asprintf(NULL, "%s ", UNITY_RPC_UNITY_ACTIVE);
   if (!val || !DynXdr_AppendRaw(&xdrs, val, strlen(val))) {
      Debug("%s: Failed to create state string.\n", __FUNCTION__);
      ret = FALSE;
      goto out;
   }
   memset(&message, 0, sizeof message);
   message.ver = UNITY_ACTIVE_V1;
   message.UnityActiveProto_u.unityActive = unity.isEnabled;
   if (!xdr_UnityActiveProto(&xdrs, &message)) {
      Debug("%s: Failed to append message content.\n", __FUNCTION__);
      ret = FALSE;
      goto out;
   }

   if (!RpcOut_SendOneRaw(DynXdr_Get(&xdrs), xdr_getpos(&xdrs), NULL, NULL)) {
      Debug("%s: Failed to send Unity state RPC.\n", __FUNCTION__);
      ret = FALSE;
   } else {
      Debug("%s: success\n", __FUNCTION__);
   }
out:
   free(val);
   DynXdr_Destroy(&xdrs, TRUE);
   return ret;
}
