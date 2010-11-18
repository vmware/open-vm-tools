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

/*
 * ghIntegration.cpp --
 *
 *    Guest-host integration functions.
 */

#include "appUtil.h"
#include "ghIntegration.h"
#include "ghIntegrationInt.h"

extern "C" {
#include "vmware.h"
#include "debug.h"
#include "guest_msg_def.h"
#include "str.h"
#include "strutil.h"
#include "util.h"
};

using vmware::tools::NotifyIconCallback;

// The pointer to the platform-specific global state.
static GHIPlatform *ghiPlatformData = NULL;

/*
 *----------------------------------------------------------------------------
 *
 * GHI_IsSupported --
 *
 *     Determine whether this guest supports guest-host integration.
 *
 * Results:
 *     TRUE if the guest supports guest-host integration
 *     FALSE otherwise
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------------
 */

Bool
GHI_IsSupported(void)
{
   return GHIPlatformIsSupported();
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHI_Init --
 *
 *     One time initialization stuff.
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
GHI_Init(GMainLoop *mainLoop,             // IN
         const char **envp,               // IN
         GHIHostCallbacks hostCallbacks)  // IN
{
   Debug("%s: Enter.\n", __FUNCTION__);

   // Call the platform-specific initialization function.
   ghiPlatformData = GHIPlatformInit(mainLoop, envp, hostCallbacks);
   if (!ghiPlatformData) {
      // TODO: We should report this failure to the caller.
      Debug("%s: GHIPlatformInit returned NULL pointer!\n", __FUNCTION__);
   }

#ifdef _WIN32
   AppUtil_BuildGlobalApplicationList();
#endif // _WIN32

   Debug("%s: Exit.\n", __FUNCTION__);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHI_Cleanup --
 *
 *     One time cleanup.
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
GHI_Cleanup(void)
{
   if (NULL != ghiPlatformData) {
      GHIPlatformCleanup(ghiPlatformData);
   }
   ghiPlatformData = NULL;
}


#ifndef _WIN32
/*
 ******************************************************************************
 * GHIX11_FindDesktopUriByExec --                                        */ /**
 *
 * Given an executable path, attempt to generate an "execUri" associated with a
 * corresponding .desktop file.
 *
 * @note Returned pointer belongs to the GHI module.  Caller must not free it.
 *
 * @param[in]  execPath Input binary path.  May be absolute or relative.
 *
 * @return Pointer to a URI string on success, NULL on failure.
 *
 ******************************************************************************
 */

const char *
GHIX11_FindDesktopUriByExec(const char *exec)
{
   ASSERT(ghiPlatformData);

   return GHIX11FindDesktopUriByExec(ghiPlatformData, exec);
}
#endif // ifndef _WIN32


/*
 *----------------------------------------------------------------------------
 *
 * GHI_GetBinaryInfo --
 *
 *      Get binary information. Returns the 'friendly name' of the application, and
 *      a list of various sized icons (depending on the icons provided by the app).
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
GHI_GetBinaryInfo(const char *pathUriUtf8,                // IN: full path to the binary file
                  std::string &friendlyName,              // OUT: friendly (long) name
                  std::list<GHIBinaryIconInfo> &iconList) // OUT: List of app icons
{
   return GHIPlatformGetBinaryInfo(ghiPlatformData, pathUriUtf8, friendlyName, iconList);
}


#if !defined(OPEN_VM_TOOLS) && !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
/*
 *----------------------------------------------------------------------------
 *
 * GHI_GetBinaryHandlers --
 *
 *     Get filetypes (extensions) and URL Protocols supported by the application.
 *
 * Results:
 *     A list of the filetypes (and protocols) handled by the application.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

const FileTypeList&
GHI_GetBinaryHandlers(const char *pathUtf8)   // IN: full path to the executable
{
   return GHIPlatformGetBinaryHandlers(ghiPlatformData, pathUtf8);
}
#endif // !OPEN_VM_TOOLS && !__FreeBSD__ && !sun && !__APPLE__


/*
 *----------------------------------------------------------------------------
 *
 * GHI_OpenStartMenuTree --
 *
 *     Get the start menu sub-tree for a given item, save it in the array so
 *     it can be accessed later when the VMX needs to iterate over the items.
 *     Return the count of the items in the sub-tree and a handle to this
 *     sub-tree. The handle will be used by the VMX to iterate over the sub-items.
 *
 * Results:
 *     TRUE if everything is successful
 *     FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

Bool
GHI_OpenStartMenuTree(const char *rootUtf8,     // IN: root of the tree
                     uint32 flags,             // IN: flags from VMX
                     DynBuf *buf)              // OUT: number of items
{
   return GHIPlatformOpenStartMenuTree(ghiPlatformData, rootUtf8, flags, buf);
}


/*
 *----------------------------------------------------------------------------
 *
 * GHI_GetStartMenuItem --
 *
 *     Get the start menu item at the given index for the tree with a given handle.
 *     If there's no item at the given index, return FALSE.
 *
 * Results:
 *     TRUE if the item was found.
 *     FALSE otherwise (i.e. if the VMX provides a wrong handle or if there's
                        no items left).
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

Bool
GHI_GetStartMenuItem(uint32 handle,       // IN: tree handle
                     uint32 itemIndex,    // IN: the index of the item in the tree
                     DynBuf *buf)         // OUT: item
{
   return GHIPlatformGetStartMenuItem(ghiPlatformData, handle, itemIndex, buf);
}


/*
 *----------------------------------------------------------------------------
 *
 * GHI_CloseStartMenuTree --
 *
 *      Free all memory associated with this start menu tree and cleanup.
 *
 * Results:
 *      TRUE if the handle is valid
 *      FALSE otherwise
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
GHI_CloseStartMenuTree(uint32 handle)     // IN: handle to the tree to be closed
{
   return GHIPlatformCloseStartMenuTree(ghiPlatformData, handle);
}


/*
 *----------------------------------------------------------------------------
 *
 * GHI_ShellOpen --
 *
 *      Open the specified file with the default shell handler (ShellExecute).
 *      Note that the file path may be either a URI (originated with
 *      Tools >= NNNNN), or a regular path (originated with Tools < NNNNN).
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
GHI_ShellOpen(const char *fileURIUtf8) // IN
{
   return GHIPlatformShellOpen(ghiPlatformData, fileURIUtf8);
}


/*
 *----------------------------------------------------------------------------
 *
 * GHI_ShellAction --
 *
 *     The action command has three arguments:
 *     an action URI, a target URI, and an array of location URIs.
 *     Action URIs are in the form: "x-vmware-action://<verb>", where <verb> is
 *     the name of a specific action to perform.
 *     The target URI is a guest-specific URI that was previously given to the
 *     host (usually a path to an application to run). Note that this may be
 *     either a URI (new Tools) or a regular path (old Tools).
 *     The locations can be files or URLs. Files are typically specified as
 *     HGFS shared folder locations (see below), but can potentially use the
 *     "file://<path>" URIs as well.
 *     Each guest can specify the features it supports using capability flags:
 *
 *     Capability                      Description
 *
 *     GHI_CAP_CMD_SHELL_ACTION        Guest allows 'ghi.guest.shell.action'.
 *                                     This encompasses this entire command
 *                                     and the rest of the capabilities.
 *
 *     GHI_CAP_SHELL_ACTION_BROWSE     Guest supports the 'browse' action verb,
 *                                     used to open a file browser window with
 *                                     a given set of locations.
 *
 *     GHI_CAP_SHELL_ACTION_RUN        Guest supports the 'run' action verb,
 *                                     used for running applications as well
 *                                     as opening file or URL locations.
 *
 *     GHI_CAP_SHELL_LOCATION_HGFS     Guest supports HGFS share location URIs:
 *                                     "x-vmware-share://<path>", where <path>
 *                                     specifies a shared folder name and an
 *                                     optional path within the shared folder.
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
GHI_ShellAction(const char *actionURI,      // IN
                const char *targetURI,      // IN
                const char **locations,     // IN
                int numLocations)           // IN
{
   return GHIPlatformShellAction(ghiPlatformData,
                                 actionURI, targetURI,
                                 locations, numLocations);
}


/*
 *----------------------------------------------------------------------------
 *
 * GHI_SetGuestHandler --
 *
 *      Set the handler for the specified filetype (or URL protocol) to the
 *      given value. One of suffix, mimeType or UTI must be specified. Some platforms
 *      (windows) only support certain identifiers (suffixes in the case of windows).
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
GHI_SetGuestHandler(const char *suffix,      // IN/OPT
                    const char *mimeType,    // IN/OPT
                    const char *UTI,         // IN/OPT
                    const char *actionURI,   // IN
                    const char *targetURI)   // IN
{
   return GHIPlatformSetGuestHandler(ghiPlatformData,
                                     suffix, mimeType, UTI,
                                     actionURI, targetURI);
}



/*
 *----------------------------------------------------------------------------
 *
 * GHI_RestoreDefaultGuestHandler --
 *
 *      Restore the handler for a given type to the value in use before any
 *      changes by tools. One of suffix, mimeType or UTI must be specified.
 *      Some platforms (windows) only support certain identifiers (suffixes
 *      in the case of windows).
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
GHI_RestoreDefaultGuestHandler(const char *suffix,    // IN/OPT
                               const char* mimetype,  // IN/OPT
                               const char *UTI)       // IN/OPT
{
   return GHIPlatformRestoreDefaultGuestHandler(ghiPlatformData, suffix, mimetype, UTI);
}

/**
 * @brief Send a mouse or keyboard event to a notification icon.
 *
 * @param[in] iconId Icon Identifier.
 * @param[in] event Event Details
 * @param[in] x Mouse Position
 * @param[in] y Mouse Position
 *
 * @retval TRUE
 * @retval FALSE
 */

Bool
GHI_TrayIconSendEvent(const char *iconID,
                      uint32 event,
                      uint32 x,
                      uint32 y)
{
   return GHIPlatformTrayIconSendEvent(ghiPlatformData, iconID, event, x, y);
}


/**
 * @brief Start sending tray icon updates to the VMX.
 *
 * @retval TRUE  The RPC succeeded.
 * @retval FALSE The RPC failed.
 */

Bool
GHI_TrayIconStartUpdates()
{
   return GHIPlatformTrayIconStartUpdates(ghiPlatformData);
}

/**
 * @brief Stop sending tray icon updates to the VMX.
 *
 * @retval TRUE Operation Succeeded.
 * @retval FALSE Operation Failed.
 */

Bool
GHI_TrayIconStopUpdates()
{
   return GHIPlatformTrayIconStopUpdates(ghiPlatformData);
}


/**
 * @brief Set the specified window to be focused.
 *
 * @param[in] windowId Window handle to be focused, passing zero handle
 *                     implies that no visible window will be focused in the guest.
 *
 * @retval TRUE Operation succeeded.
 * @retval FALSE Operation failed.
 */

Bool
GHI_SetFocusedWindow(int32 windowId)
{
   return GHIPlatformSetFocusedWindow(ghiPlatformData, windowId);
}


/**
 * @brief Get the hash (or timestamp) of information returned by
 * GHIPlatformGetBinaryInfo
 *
 * @param[in]  request  Request containing which executable to get the hash for.
 * @param[out] reply    Reply to be filled with the hash, the caller must free this memory.
 *
 * @retval TRUE Operation succeeded.
 * @retval FALSE Operation failed.
 */

Bool
GHI_GetExecInfoHash(const char *execPath,
                    char **execInfoHash)
{
   return GHIPlatformGetExecInfoHash(ghiPlatformData, execPath, execInfoHash);
}


/**
 * @brief Sets the OutlookTempHgfsPath value used by hostOpen.
 *
 * When hostOpen is invoked on a file that is not on an hgfs share, it typically
 * displays an error message. However, as a special case for Microsoft Outlook
 * attachments, hostOpen will copy the file to an hgfs share, then send an RPC
 * to the host to cause it to open the file in the default host application.
 *
 * @param[in] targetURI Pointer to URI to be used as the folder for temp files.
 *
 * @returns TRUE if the OutlookTempHgfsPath value was set.
 */
Bool
GHI_SetOutlookTempFolder(const char *targetURI)
{
   return GHIPlatformSetOutlookTempFolder(ghiPlatformData, targetURI);
}


/*
 *----------------------------------------------------------------------------
 *
 * GHI_RegisterNotifyIconCallback --
 *
 *      Register the supplied callback object with the
 *      Notify Icon Manager.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

void
GHI_RegisterNotifyIconCallback(NotifyIconCallback *notifyIconCallback)  // IN
{
   GHIPlatformRegisterNotifyIconCallback(notifyIconCallback);
}


/*
 *----------------------------------------------------------------------------
 *
 * GHI_UnregisterNotifyIconCallback --
 *
 *      Unregister the supplied callback object with the
 *      Notify Icon Manager.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

void
GHI_UnregisterNotifyIconCallback(NotifyIconCallback *notifyIconCallback)   // IN
{
   GHIPlatformUnregisterNotifyIconCallback(notifyIconCallback);
}


