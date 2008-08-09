/*********************************************************
 * Copyright (C) 2007-2008 VMware, Inc. All rights reserved.
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
 * unityPlatformX11Settings.c --
 *
 *    Handles saving and restoring various system settings that are needed for Unity to
 *    work well.
 */

#include "unityX11.h"

#if !defined(VM_HAVE_X11_SS_EXT) && !defined(USING_AUTOCONF)
#error "We're not building with the X11 ScreenSaver extension."
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * GetScreensaverActive --
 *
 *      Finds out whether the screensaver is currently enabled.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise
 *
 * Side effects:
 *      Stores the current setting in *currentSetting
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GetScreensaverActive(UnityPlatform *up,    // IN
                     Bool *currentSetting) // OUT
{
   int timeout;
   int dummy;

   ASSERT(up);
   ASSERT(currentSetting);

   *currentSetting = FALSE;

#if defined(VM_HAVE_X11_SS_EXT)
   {
      int eventBase;
      int errorBase;
      if (XScreenSaverQueryExtension(up->display, &eventBase, &errorBase)) {
         XScreenSaverInfo saverInfo;

         if (!XScreenSaverQueryInfo(up->display,
                                    DefaultRootWindow(up->display),
                                    &saverInfo)) {
            return FALSE;
         }

         *currentSetting = (saverInfo.state != ScreenSaverDisabled);
      }
   }
#endif

   timeout = -1;
   XGetScreenSaver(up->display, &timeout, &dummy, &dummy, &dummy);

   if(timeout == -1) {
      return FALSE;
   }

   *currentSetting = *currentSetting || (timeout ? TRUE : FALSE);
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SetScreensaverActive --
 *
 *      Enables/disables the screensaver.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
SetScreensaverActive(UnityPlatform *up,    // IN
                     Bool currentSetting)  // IN
{
   int timeout = -1; // If XGetScreenSaver fails, use the default
   int interval = 0;
   int preferBlanking = DefaultBlanking;
   int allowExposures = DefaultExposures;
   char *xdgScreensaverPath;

   ASSERT(up);

   /*
    * There are currently three ways to enable/disable the screensaver (in order of
    * current preference):
    *
    * 1. The xdg-screensaver script that knows how to talk to the screensavers of most
    * current desktops.
    * 2. XScreenSaverSuspend, via the X screensaver extension.
    * 3. XSetScreenSaver.
    *
    * Only the first one actually works on most systems, but the other two are there to
    * catch corner cases on odd systems.
    */

   xdgScreensaverPath = g_find_program_in_path("xdg-screensaver");
   if (up->rootWindows && xdgScreensaverPath) {
      char rootWindowID[64];
      char *argv[] = {xdgScreensaverPath,
                      currentSetting ? "resume" : "suspend",
                      rootWindowID,
                      NULL};

      g_snprintf(rootWindowID, sizeof rootWindowID, "%#lx", up->rootWindows->windows[0]);

      g_spawn_sync("/", argv, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL);
   }
   g_free(xdgScreensaverPath);

#if defined(VM_HAVE_X11_SS_EXT)
   {
      int eventBase;
      int errorBase;

      if (!XScreenSaverQueryExtension(up->display, &eventBase, &errorBase)) {
         return FALSE;
      }
   /*
    * XScreenSaverSuspend is only available as of version 1.1 of the screensaver
    * extension
    */
#   if (ScreenSaverMajorVersion > 1 \
       || (ScreenSaverMajorVersion == 1 && ScreenSaverMinorVersion >= 1))
      {
         int majorVersion;
         int minorVersion;

         if (XScreenSaverQueryVersion(up->display, &majorVersion, &minorVersion)
             && (majorVersion > 1 || (majorVersion == 1 && minorVersion >= 1))) {

            XScreenSaverSuspend(up->display, !currentSetting);
            up->currentSettings[UNITY_UI_SCREENSAVER] = currentSetting;
         }
      }
#   endif

      /*
       * XXX TODO: On systems that don't have XScreenSaverSuspend, we could always monitor
       * ScreenSaverNotify events, and send a ForceScreenSaver request (with value of Reset)
       * whenever the screensaver comes on.
       */
   }
#endif

   XGetScreenSaver(up->display, &timeout, &interval, &preferBlanking, &allowExposures);

   if (!currentSetting) {
      up->savedScreenSaverTimeout = timeout; // Save the old timeout.
      timeout = 0; // Disables the screensaver.
   } else {
      timeout = up->savedScreenSaverTimeout;
      up->savedScreenSaverTimeout = -1;
   }

   XSetScreenSaver(up->display, timeout, interval, preferBlanking, allowExposures);

   if (!currentSetting) {
      /*
       * Disable the screen saver if it's already active.
       */
      XForceScreenSaver(up->display, ScreenSaverReset);
   }

   return TRUE;
}


#ifdef VM_UNIMPLEMENTED_UNITY_SETTINGS


/*
 *-----------------------------------------------------------------------------
 *
 * GetDropShadowActive --
 *
 *      Finds out whether drop shadows are currently enabled.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise
 *
 * Side effects:
 *      Stores the current setting in *currentSetting
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GetDropShadowActive(UnityPlatform *up,    // IN
                    Bool *currentSetting) // OUT
{
   ASSERT(up);
   ASSERT(currentSetting);

   NOT_IMPLEMENTED();

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SetDropShadowActive --
 *
 *      Enables/disables drop shadows.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
SetDropShadowActive(UnityPlatform *up,    // IN
                    Bool currentSetting)  // IN
{
   ASSERT(up);

   up->currentSettings[UNITY_UI_DROP_SHADOW] = currentSetting;

   NOT_IMPLEMENTED();

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetMenuAnimationActive --
 *
 *      Finds out whether menu animation is currently enabled.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise
 *
 * Side effects:
 *      Stores the current setting in *currentSetting
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GetMenuAnimationActive(UnityPlatform *up,    // IN
                       Bool *currentSetting) // OUT
{
   ASSERT(up);
   ASSERT(currentSetting);

   NOT_IMPLEMENTED();

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SetMenuAnimationActive --
 *
 *      Enables/disables the screensaver.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
SetMenuAnimationActive(UnityPlatform *up,    // IN
                       Bool currentSetting)  // IN
{
   ASSERT(up);

   up->currentSettings[UNITY_UI_MENU_ANIMATION] = currentSetting;

   NOT_IMPLEMENTED();

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetTooltipAnimationActive --
 *
 *      Finds out whether the screensaver is currently enabled.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise
 *
 * Side effects:
 *      Stores the current setting in *currentSetting
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GetTooltipAnimationActive(UnityPlatform *up,    // IN
                          Bool *currentSetting) // OUT
{
   ASSERT(up);
   ASSERT(currentSetting);

   NOT_IMPLEMENTED();

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SetTooltipAnimationActive --
 *
 *      Enables/disables the screensaver.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
SetTooltipAnimationActive(UnityPlatform *up,    // IN
                          Bool currentSetting)  // IN
{
   ASSERT(up);

   up->currentSettings[UNITY_UI_TOOLTIP_ANIMATION] = currentSetting;

   NOT_IMPLEMENTED();

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetWindowAnimationActive --
 *
 *      Finds out whether the screensaver is currently enabled.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise
 *
 * Side effects:
 *      Stores the current setting in *currentSetting
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GetWindowAnimationActive(UnityPlatform *up,    // IN
                         Bool *currentSetting) // OUT
{
   ASSERT(up);
   ASSERT(currentSetting);

   NOT_IMPLEMENTED();

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SetWindowAnimationActive --
 *
 *      Enables/disables the screensaver.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
SetWindowAnimationActive(UnityPlatform *up,    // IN
                         Bool currentSetting)  // IN
{
   ASSERT(up);

   up->currentSettings[UNITY_UI_WINDOW_ANIMATION] = currentSetting;

   NOT_IMPLEMENTED();

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetFullWindowDragActive --
 *
 *      Finds out whether the screensaver is currently enabled.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise
 *
 * Side effects:
 *      Stores the current setting in *currentSetting
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GetFullWindowDragActive(UnityPlatform *up,    // IN
                        Bool *currentSetting) // OUT
{
   ASSERT(up);
   ASSERT(currentSetting);

   NOT_IMPLEMENTED();

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SetFullWindowDragActive --
 *
 *      Enables/disables the screensaver.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
SetFullWindowDragActive(UnityPlatform *up,    // IN
                        Bool currentSetting)  // IN
{
   ASSERT(up);

   up->currentSettings[UNITY_UI_FULL_WINDOW_DRAG] = currentSetting;

   NOT_IMPLEMENTED();

   return FALSE;
}


#endif


/*
 *-----------------------------------------------------------------------------
 *
 * GetTaskbarVisible --
 *
 *      Shows or hides the taskbar as appropriate.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GetTaskbarVisible(UnityPlatform *up,    // IN
                  Bool *currentSetting) // OUT
{
   UnityPlatformWindow **allWindows;
   size_t i;
   size_t numWindows;

   ASSERT(up);
   ASSERT(currentSetting);

   numWindows = 0;
   HashTable_ToArray(up->allWindows,
                     (void ***)&allWindows,
                     &numWindows);

   if (!numWindows) {
      Debug("Couldn't find any listed windows for taskbar visibility detection.\n");
      return FALSE; // We haven't yet populated the window list for some reason
   }

   /*
    * Hunt through all the windows for ones that are of type DOCK.
    */
   *currentSetting = FALSE;
   for (i = 0; i < numWindows; i++) {
      if (UNITY_WINDOW_TYPE_DOCK == allWindows[i]->windowType
         && allWindows[i]->isViewable) {
         *currentSetting = TRUE;
         break;
      }
   }

   free(allWindows);

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformSetTaskbarVisible --
 *
 *      Shows or hides the taskbar as appropriate.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise.
 *
 * Side effects:
 *      Shows or hides task bar.
 *
 *-----------------------------------------------------------------------------
 */

Bool
UnityPlatformSetTaskbarVisible(UnityPlatform *up,   // IN
                               Bool currentSetting) // IN
{
   UnityPlatformWindow **allWindows;
   size_t i;
   size_t numWindows;

   ASSERT(up);

   up->needTaskbarSetting = FALSE;

   up->currentSettings[UNITY_UI_TASKBAR_VISIBLE] = currentSetting;

   numWindows = 0;
   HashTable_ToArray(up->allWindows,
                     (void ***) &allWindows,
                     &numWindows);

   /*
    * Hunt through all the windows for ones that are of type DOCK.
    */
   for (i = 0; i < numWindows; i++) {
      if (UNITY_WINDOW_TYPE_DOCK == allWindows[i]->windowType) {
         Window dockWindow;

         dockWindow = allWindows[i]->clientWindow;
         if (!dockWindow) {
            dockWindow = allWindows[i]->toplevelWindow;
         }

         if (currentSetting) {
            XMapWindow(up->display, dockWindow);
         } else {
            XWithdrawWindow(up->display, dockWindow, 0);
         }

         UPWindow_CheckRelevance(up, allWindows[i], NULL);
      }
   }

   free(allWindows);

   if (!numWindows) {
      /*
       * We need to repeat this call later.
       */
      up->needTaskbarSetting = TRUE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SaveVirtualDesktopSettings --
 *
 *      Saves the current virtual desktop configuration so it can be restored later on.
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
SaveVirtualDesktopSettings(UnityPlatform *up) // IN
{
   ASSERT(up);

   up->desktopInfo.savedNumDesktops = UnityPlatformGetNumVirtualDesktops(up);
   UnityPlatformGetVirtualDesktopLayout(up, up->desktopInfo.savedLayoutData);
   up->desktopInfo.savedCurrentDesktop = UnityX11GetCurrentDesktop(up);
}


/*
 *-----------------------------------------------------------------------------
 *
 * RestoreVirtualDesktopSettings --
 *
 *      Restores the saved virtual desktop configuration.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Guest's virtual desktop config may be changed.
 *
 *-----------------------------------------------------------------------------
 */

void
RestoreVirtualDesktopSettings(UnityPlatform *up) // IN
{
   size_t tempDesktops;
   ASSERT(up);

   memcpy(up->desktopInfo.layoutData,
          up->desktopInfo.savedLayoutData,
          sizeof up->desktopInfo.layoutData);
   tempDesktops = up->desktopInfo.numDesktops;
   up->desktopInfo.numDesktops = up->desktopInfo.savedNumDesktops;
   UnityPlatformSyncDesktopConfig(up);

   /*
    * ...because numDesktops also refers to the size of the guestDesktopToUnity and
    * unityDesktopToGuest arrays.
    */
   up->desktopInfo.numDesktops = tempDesktops;

   UnityX11SetCurrentDesktop(up, up->desktopInfo.savedCurrentDesktop);
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformSaveSystemSettings --
 *
 *      Stub to make unity.c happy. This function is called at a very inconvenient time
 *      for the X11 port, so I just call its UnityX11 equivalent at the appropriate point
 *      in StartHelperThreads instead.
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
UnityPlatformSaveSystemSettings(UnityPlatform *up) // IN
{
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityX11SaveSystemSettings --
 *
 *      Save and disable certain system settings here:
 *      a. If a screen saver is enabled, disable it
 *      b. If animation for menus or tooltips is enabled, disable it
 *      c. If menu shading is enabled, disable it
 *      d. If full window drag is disabled, enable it
 *      e. If window animation is enabled, disable it
 *      f. Hide the task bar.
 *
 *      Right now on X11, only the screensaver and task bar make any sense.
 *
 *      Remember all the settings changed above in a bit mask,
 *      so we can restore them later when the user exits
 *      the Unity mode.
 *
 *      Note, that the system ui changes made here will not
 *      persist after the system reboot.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      A bunch of system ui settings might be changed.
 *
 *----------------------------------------------------------------------------
 */

void
UnityX11SaveSystemSettings(UnityPlatform *up) // IN
{
   ASSERT(up);

   /*
    * We only want to remember current settings if we do not have saved settings already.
    * One of the reasons why we might have saved settings already is because we are
    * re-entering unity without cleanly exiting it first (if the VM was suspended).  In
    * this case, theoretically, all the right settings are set already and
    * up->originalSettings contains the original user settings that we do not want to
    * overwrite.
    */
   if (!up->haveOriginalSettings) {
      Bool *originalSettings = up->originalSettings;

      memset(up->originalSettings, 0, sizeof up->originalSettings);

      if (!GetScreensaverActive(up, &originalSettings[UNITY_UI_SCREENSAVER])) {
         originalSettings[UNITY_UI_SCREENSAVER] = TRUE;
      }

#ifdef VM_UNIMPLEMENTED_UNITY_SETTINGS
      if (!GetDropShadowActive(up, &originalSettings[UNITY_UI_DROP_SHADOW])) {
         originalSettings[UNITY_UI_DROP_SHADOW] = TRUE;
      }

      if (!GetMenuAnimationActive(up, &originalSettings[UNITY_UI_MENU_ANIMATION])) {
         originalSettings[UNITY_UI_MENU_ANIMATION] = TRUE;
      }

      if (!GetTooltipAnimationActive(up,
                                     &originalSettings[UNITY_UI_TOOLTIP_ANIMATION])) {
         originalSettings[UNITY_UI_TOOLTIP_ANIMATION] = TRUE;
      }

      if (!GetWindowAnimationActive(up, &originalSettings[UNITY_UI_WINDOW_ANIMATION])) {
         originalSettings[UNITY_UI_WINDOW_ANIMATION] = TRUE;
      }

      if (!GetFullWindowDragActive(up, &originalSettings[UNITY_UI_FULL_WINDOW_DRAG])) {
         originalSettings[UNITY_UI_FULL_WINDOW_DRAG] = TRUE;
      }
#endif

      if (!GetTaskbarVisible(up, &originalSettings[UNITY_UI_TASKBAR_VISIBLE])) {
         originalSettings[UNITY_UI_TASKBAR_VISIBLE] = TRUE;
      }

      SaveVirtualDesktopSettings(up);

      up->haveOriginalSettings = TRUE;

      memcpy(up->currentSettings, originalSettings, sizeof up->currentSettings);
   }

   SetScreensaverActive(up, FALSE);
#ifdef VM_UNIMPLEMENTED_UNITY_SETTINGS
   SetDropShadowActive(up, FALSE);
   SetMenuAnimationActive(up, FALSE);
   SetTooltipAnimationActive(up, FALSE);
   SetWindowAnimationActive(up, FALSE);
   SetFullWindowDragActive(up, FALSE);
#endif

   {
      char *reply = NULL;
      size_t replyLen;
      Bool shouldBeVisible = FALSE;

      if (!RpcOut_sendOne(&reply, &replyLen, UNITY_RPC_VMX_SHOW_TASKBAR)) {
         Debug("%s: could not get the VMX show taskbar setting, assuming FALSE\n",
               __FUNCTION__);
      } else {
         uint32 value = 0;

         if (StrUtil_StrToUint(&value, reply)) {
            shouldBeVisible = (value == 0) ? FALSE : TRUE;
         }
      }

      Debug("TASKBAR SHOULD BE VISIBLE: %d\n", shouldBeVisible);

      UnityPlatformSetTaskbarVisible(up, shouldBeVisible);

      free(reply);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformRestoreSystemSettings --
 *
 *      Stub to make unity.c happy. This function is called at a very inconvenient time
 *      for the X11 port, so I just call its UnityX11 equivalent at the appropriate point
 *      in KillHelperThreads instead.
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
UnityPlatformRestoreSystemSettings(UnityPlatform *up) // IN
{
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityX11RestoreSystemSettings --
 *
 *      Restore system ui settings to what they used to be
 *      before we entered unity mode.
 *
 *      This includes:
 *      a. Enable screen saver if it was disabled
 *      b. Enable menu and tool tip animation if it was disabled
 *      c. Enable menu shading is enabled if it was disabled
 *      d. Disable full window drag is disabled if it was enabled
 *      e. Enable window animation if it was disabled
 *      f. Show the task bar if it was hidden
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      A bunch of system ui settings might be changed.
 *
 *----------------------------------------------------------------------------
 */

void
UnityX11RestoreSystemSettings(UnityPlatform *up) // IN
{
   ASSERT(up);

   Debug("UnityPlatformRestoreSystemSettings\n");
   if (up->currentSettings[UNITY_UI_SCREENSAVER]
       != up->originalSettings[UNITY_UI_SCREENSAVER]) {
      SetScreensaverActive(up, up->originalSettings[UNITY_UI_SCREENSAVER]);
      Debug("%s: Restored screen saver\n", __FUNCTION__);
   }

#ifdef VM_UNIMPLEMENTED_UNITY_SETTINGS
   if (up->currentSettings[UNITY_UI_DROP_SHADOW]
       != up->originalSettings[UNITY_UI_DROP_SHADOW]) {
      SetDropShadowActive(up, up->originalSettings[UNITY_UI_DROP_SHADOW]);
      Debug("%s: Restored drop shadows\n", __FUNCTION__);
   }

   if (up->currentSettings[UNITY_UI_MENU_ANIMATION]
       != up->originalSettings[UNITY_UI_MENU_ANIMATION]) {
      SetMenuAnimationActive(up, up->originalSettings[UNITY_UI_MENU_ANIMATION]);
      Debug("%s: Restored menu animation\n", __FUNCTION__);
   }

   if (up->currentSettings[UNITY_UI_TOOLTIP_ANIMATION]
       != up->originalSettings[UNITY_UI_TOOLTIP_ANIMATION]) {
      SetTooltipAnimationActive(up, up->originalSettings[UNITY_UI_TOOLTIP_ANIMATION]);
      Debug("%s: Restored tool tip animation\n", __FUNCTION__);
   }

   if (up->currentSettings[UNITY_UI_WINDOW_ANIMATION]
       != up->originalSettings[UNITY_UI_WINDOW_ANIMATION]) {
      SetWindowAnimationActive(up, up->originalSettings[UNITY_UI_WINDOW_ANIMATION]);
      Debug("%s: Restored window animation\n", __FUNCTION__);
   }

   if (up->currentSettings[UNITY_UI_FULL_WINDOW_DRAG]
       != up->originalSettings[UNITY_UI_FULL_WINDOW_DRAG]) {
      SetFullWindowDragActive(up, up->originalSettings[UNITY_UI_FULL_WINDOW_DRAG]);
      Debug("%s: Restored outline drag.\n", __FUNCTION__);
   }
#endif

   if (up->currentSettings[UNITY_UI_TASKBAR_VISIBLE]
       != up->originalSettings[UNITY_UI_TASKBAR_VISIBLE]) {
      UnityPlatformSetTaskbarVisible(up, up->originalSettings[UNITY_UI_TASKBAR_VISIBLE]);
      Debug("%s: Restored taskbar visibility.\n", __FUNCTION__);
   }

   RestoreVirtualDesktopSettings(up);

   /*
    * The user's settings have been restored, which means the originalSettings info will
    * not be relevant next time we go into SaveSystemSettings().
    */
   up->haveOriginalSettings = FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityPlatformShowTaskbar  --
 *
 *      Show/hide the taskbar while in Unity mode.
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
UnityPlatformShowTaskbar(UnityPlatform *up,   // IN
                         Bool showTaskbar)    // IN
{
   ASSERT(up);

   /*
    * If we are in Unity mode, we need to hide/show the taskbar.
    * If the user asked to show the taskbar and the taskbar was previously hidden,
    * we need to show the taskbar and readjust the work area.
    * Other cases (when the taskbar is already shown and user wants to show it,
    * for example), should theoretically never happen, but if they do, we just
    * ignore them because there's not much we can do.
    */

   if (UnityPlatformIsUnityRunning(up)) {
      Debug("Host asked us to show the taskbar %d\n", showTaskbar);
      UnityPlatformSetTaskbarVisible(up, showTaskbar);
   } else {
      Debug("%s: We are not in Unity mode, ignore the show taskbar command\n",
            __FUNCTION__);
   }

   UnityPlatformSendPendingUpdates(up, UNITY_UPDATE_INCREMENTAL);
}
