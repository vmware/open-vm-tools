/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * @file unityCommon.h
 *
 *      Data shared between Tools, VMX, and UI for Unity
 *      RPCs, attributes, and variables.
 */

/*
 * Breakdown of components involved in Unity (as of change 748058 on
 * 2008/10/23):
 *
 *   1) Unity desktop target
 *
 *      This is the system on which Unity windows will be displayed.
 *
 *      Currently, the host operating system running VMware Workstation
 *      or Fusion.
 *
 *   2) Unity desktop source
 *
 *      This is the system from which windows will be read and
 *      enumerated, and sent over to the Unity desktop target.
 *
 *      An agent running on the desktop source will gather
 *      enumerated window data and send it to the Unity server.
 *
 *      Currently, the virtualized guest operating system running
 *      under Workstation or Fusion, and VMware Tools running on that
 *      operating system.
 *
 *      Source: bora-vmsoft/lib/unity
 *              bora/lib/unityWindowTracker
 *
 *   3) Unity client
 *
 *      This is a VNC-capable client that supports the extensions
 *      specified in VNCEncode_UnitySupported (currently,
 *      VNCUnityWindowRectCap and VNCUnityWindowRegionCap).
 *
 *      Source: bora/apps/lib/cui/unityMgr.cc
 *              bora/apps/lib/cui/unityMgrVNC.cc
 *              bora/lib/vnc/vncDecode.c
 *
 *              (used by the Workstation and Fusion hosted UI
 *              processes)
 *
 *   4) Unity server
 *
 *      This is a VNC server that recognizes the Unity VNC extensions
 *      and handles instructing the agent running on the Unity desktop
 *      source to enumerate its windows and update the server with
 *      window metadata (geometry, title, shaped region, etc.)
 *
 *      The Unity server gathers window metadata in its own Unity
 *      window tracker to decide what data to send to the Unity
 *      client.
 *
 *      Source: bora/lib/unityWindowTracker
 *              bora/mks/hostops/vncBackend.c
 *              bora/lib/vnc/vncEncode.c
 *
 *              (used by the VMX process)
 *
 */

/*
 *   Overview of Unity enter operation
 *   ---------------------------------
 *
 *   This is an idealized order of operations between the Unity
 *   client, server, and agent running on the Unity desktop source.
 *
 *   Below the diagram are notes ([1], [2], [3]...) on the current
 *   implementation (VMware Workstation and Fusion).
 *
 *   In the current implementation:
 *
 *      "Client" is the hosted UI process
 *      "Server" is the vmware-vmx process
 *      "Agent"  is the vmware-user process (VMware Tools per-user process)
 *
 *   /--------\              /--------\              /-------\
 *   | Client |              | Server |              | Agent |
 *   \--------/              \--------/              \-------/
 *
 *   [1]    RPC to start Unity
 *          mode in desktop source
 *        =========================>
 *
 *   [2]                             RPC to start enumerating
 *                                   windows of Unity desktop
 *                                   source (aka "Unity mode")
 *                                 =============================>
 *
 *   [3]                             RPC response with full update
 *                                   containing window metadata
 *                                   of Unity desktop source
 *                                 <=============================
 *
 *   [4]    RPC response with
 *          Unity mode entered
 *          successfully
 *        <======================
 *
 *   [5]    Full update containing
 *          window metadata of Unity
 *          desktop source
 *        <======================
 *
 *   [6]                             Repeating RPC updates with   /->-\
 *                                   deltas for window metadata   |   |
 *                                   of Unity desktop source      |   |
 *                                 <============================= \-<-/
 *
 *   [7]    Delta update containing
 *          window metadata of Unity
 *          desktop source
 *        <======================
 *
 *
 * -----------------------------------------------------------------------
 *
 * [1] Hosted UI opens VNC connection to VMX (through authd) with
 *     VNCUnityWindowRectCap and VNCUnityWindowRegionCap encodings supported.
 *
 *     MKS remote manager receives VNC connection socket connection
 *     and spins up MKS hostops VNC backend.
 *
 * [2] MKSRemoteMgr_StartUnityNotifications() asks Tools to start
 *     Unity by sending two TCLO RPCs via the backdoor (TODO: failure
 *     is currently ignored):
 *
 *     2a) UNITY_RPC_ENTER
 *     2b) UNITY_RPC_GET_UPDATE_FULL
 *
 * [3] Tools receives UNITY_RPC_ENTER, spins up window update thread,
 *     and creates Unity window tracker.  System settings and features not
 *     compatible with or conducive to Unity mode, such as screen savers
 *     and miscellaneous visual effects, are disabled for the duration of
 *     the Unity session.  (See UnityPlatformSaveSystemSettings.)
 *
 * [4] Tools sends a UNITY_RPC_UNITY_ACTIVE (state change) RPC to the host
 *     indicating that it has successfully entered Unity mode.  This is
 *     then recorded in VMDB under "vmx/guestTools/currentStatus/unityActive".
 *
 * [5] Tools receives UNITY_RPC_GET_UPDATE_FULL, and in response enumerates
 *     all windows running in the guest operating system.  Windows suitable*
 *     for display in Unity mode are inserted into the Unity window tracker.
 *     Tools then returns the contents of the Unity window tracker as its
 *     response to the RPC.
 *
 *     *  Examples of suitable windows are application windows, tooltips,
 *     and menus.  Examples of non-suitable windows are the desktop window,
 *     taskbar(s) (depending on configuration), and trays.
 *
 * [6] Tools continually reacts to changes in the guest operating system's
 *     windowing environment* and transforms them into input for the Unity
 *     window tracker.  In response to certain events, Tools will flush the
 *     tracker's contents to the host via the RpcOut mechanism over the
 *     backdoor.  Updates will be stored in a simple queue for step [7].
 *
 *     *  Example windowing events are window creation and deletion, move
 *     and resize.
 *
 * [7] The MKS remote manager periodically dequeues the guest's Unity window
 *     updates as part of its MKSRemoteMgr_FastPoll loop.  The updates are then
 *     fed into the MKS's own Unity window tracker.  The MKS Unity window
 *     tracker then coalesces window updates and transforms them into VNC
 *     rectangle updates for consumption by the UI.
 */

#ifndef _UNITY_COMMON_H_
#define _UNITY_COMMON_H_

#define UNITY_MAX_SETTOP_WINDOW_COUNT 100

/* 
 * RPC messages between host and guest that are part of Unity and GHI
 * protocols. 
 * If messages are added, make sure to update the corresponding JavaDoc
 * below.
 */

/**
   @defgroup UnityRpcHG Unity: Host-to-Guest RPCs
   @{

   @def         UNITY_RPC_ENTER
   @brief       Tell the guest to go into Unity mode. 
   @code
   UNITY_RPC_ENTER
   @endcode
   @note        On success, the guest will send @ref UNITY_RPC_UNITY_ACTIVE
                with an argument of @c 1 to indicate it is in Unity mode.


   @def         UNITY_RPC_GET_UPDATE
   @brief       Get an incremental or full update of window changes detected by
                the guest. 
   @todo        Deprecate this RPC in favor of using UNITY_RPC_GET_UPDATE_FULL
                and UNITY_RPC_GET_UPDATE_INCREMENTAL
   @code
   UNITY_RPC_GET_UPDATE ["incremental"] 
   @endcode
   @param incremental   If present, do an incremental update, otherwise full.
   @note                A full update reports all current state of all windows,
                        while incremental sends updates received since the last
                        UNITY_RPC_GET_UPDATE was received.


   @def         UNITY_RPC_GET_WINDOW_PATH
   @brief       Return the pathname of the application corresponding to the
                specified window. 
   @code
   UNITY_RPC_GET_WINDOW_PATH UnityWindowId
   @endcode
   @param UnityWindowId window to get path information from
   @note       The data is returned as UTF-8. 


   @def         UNITY_RPC_GET_BINARY_INFO
   @brief       Return a list of icons for a window. 
   @todo        Move this into a GHI-specific header.
   @todo        Give this RPC command an intuitive name.
   @code 
   UNITY_RPC_GET_BINARY_INFO UnityWindowId
   @endcode
   @param[in] UnityWindowId     window id to get icons for 
   @return
   @verbatim
<retval> := <name><nul><icon_data>{<icon_data>}

<name> := name of application
<icon_data> := count<nul>width<nul>height<nul>size<nul>bgraData<nul><nul>
<nul> := '\0'
@endverbatim
   @note        Icon data is in BGRA format. An alpha channel value of 255 means
                "fully opaque", and a value of 0 means "fully transparent". 


   @def         UNITY_RPC_GET_BINARY_HANDLERS
   @brief       Ask the guest to return filetypes (extensions) and URL protocols
                supported by the guest. 
   @todo        Move this into a GHI-specific header.
   @code 
   UNITY_RPC_GET_BINARY_HANDLERS 
   @endcode
   @sa ghiGetBinaryHandlers.x for format of data returned.


   @def         UNITY_RPC_OPEN_LAUNCHMENU
   @brief       Get the start menu sub-tree for a given item
   @todo        Move this into a GHI-specific header.
   @code 
   UNITY_RPC_OPEN_LAUNCHMENU root
   @endcode
   @param[in] root      Name of sub-tree, or "" for root of start menu. 
   @return
   <tt>"count handle"</tt>
   \li @c count Number of items in the sub-tree.
   \li @c handle Opaque handle passed to @ref UNITY_RPC_GET_LAUNCHMENU_ITEM.


   @def         UNITY_RPC_GET_LAUNCHMENU_ITEM
   @brief       Get the nth item in the menu sub-tree.
   @todo        Move this into a GHI-specific header.
   @code 
   UNITY_RPC_GET_LAUNCHMENU_ITEM handle index
   @endcode
   @param[in] handle    Handle returned by @ref UNITY_RPC_OPEN_LAUNCHMENU.
   @param[in] index     index of the item to retrieve (zero-indexed).
   @return Executable path corresponding to menu item as UTF-8, or ""
   @sa UNITY_RPC_SHELL_OPEN


   @def         UNITY_RPC_CLOSE_LAUNCHMENU
   @brief       Close the sub-menu, releasing all associated resources. 
   @todo        Move this into a GHI-specific header.
   @code 
   UNITY_RPC_CLOSE_LAUNCHMENU handle
   @endcode
   @param[in] handle Handle returned by @ref UNITY_RPC_OPEN_LAUNCHMENU.


   @def         UNITY_RPC_WINDOW_SETTOP 
   @brief       Raise a group of windows to the top of the window stacking order. 
   @code 
   UNITY_RPC_WINDOW_SETTOP UnityWindowId{ UnityWindowId} 
   @endcode
   @param[in] UnityWindowId{ UnityWindowId} group of windows to raise 
   @note        Order of windows is bottom to top.
   @note        Only @ref UNITY_MAX_SETTOP_WINDOW_COUNT windows can be specified.


   @def         UNITY_RPC_WINDOW_CLOSE 
   @brief       Close the specified window. 
   @code 
   UNITY_RPC_WINDOW_CLOSE UnityWindowId 
   @endcode
   @param[in] UnityWindowId window to close 


   @def         UNITY_RPC_GET_WINDOW_CONTENTS
   @brief       Retreive pixel contents of the Window. 
   @code 
   UNITY_RPC_GET_WINDOW_CONTENTS UnityWindowId 
   @endcode
   @param[in] UnityWindowId UnityWindowId of window to get contents from 
   @return      PNG image containing the window content. 


   @def         UNITY_RPC_GET_ICON_DATA     
   @brief       Return icon data for a specific window. 
   @code 
   UNITY_RPC_GET_ICON_DATA UnityWindowId type size dataOffset dataLength
   @endcode
   @param[in] UnityWindowId     window to get icon data for
   @param[in] type              UNITY_ICON_TYPE_MAIN
   @param[in] size              size of icon e.g., 16, 32, 48
   @param[in] dataOffset        offset into icon data client desires
   @param[in] dataLength        number of bytes starting at dataOffset to return
   @return                      PNG image containing the icon data.


   @def         UNITY_RPC_EXIT             
   @brief       Cease enumerating windows and leave Unity mode. 
   @code 
   UNITY_RPC_EXIT 
   @endcode
   @note        On success, the guest will send a @ref UNITY_RPC_UNITY_ACTIVE
                with an argument of 0 to indicate it has exited Unity mode.


   @def         UNITY_RPC_GET_UPDATE_FULL  
   @brief       Equivalent to @ref UNITY_RPC_GET_UPDATE with no argument. 
   @code 
   UNITY_RPC_GET_UPDATE_FULL 
   @endcode


   @def         UNITY_RPC_GET_UPDATE_INCREMENTAL 
   @brief       Equivalent to @ref UNITY_RPC_GET_UPDATE with an "incremental" argument. 
   @code 
   UNITY_RPC_GET_UPDATE_INCREMENTAL 
   @endcode


   @def         UNITY_RPC_SHELL_OPEN            
   @brief       Open the application corresponding to the passed in URI or
                regular path. 
   @todo        Move this into a GHI-specific header.
   @code 
   UNITY_RPC_SHELL_OPEN 
   @endcode
   @param[in] path URI or path of executable to open 
   @note        The URI is opaque to the caller, the caller supplies this URI
                to the host with the expecation that it will be able to
                understand the format when this RPC is invoked. 


   @def         UNITY_RPC_SHOW_TASKBAR         
   @brief       Show or hide the guest taskbar. 
   @code 
   UNITY_RPC_SHOW_TASKBAR flag
   @endcode
   @param[in] flag 0 to hide the taskbar, or 1 to show the taskbar.


   @def         UNITY_RPC_WINDOW_MOVE_RESIZE  
   @brief       Change the geometry of the specified window. 
   @code 
   UNITY_RPC_WINDOW_MOVE_RESIZE UnityWindowId x y width height
   @endcode
   @param[in] UnityWindowId     UnityWindowId of window to modify
   @param[in] x                 x coordinate of desired window origin
   @param[in] y                 y coordinate of desired window origin
   @param[in] width             desired width
   @param[in] height            desired height
   @note        Window origin refers to the upper left (north west) corner.
   @note        It is possible that the result will not be as requested due
                to the layout policy of the guest desktop or window manager.


   @def         UNITY_RPC_DESKTOP_WORK_AREA_SET 
   @brief       Specify the desktop work areas. 
   @todo        Add more detail about this RPC and why it matters.
   @code 
   UNITY_RPC_DESKTOP_WORK_AREA_SET <work_areas> 
   @endcode
   @param[in] <work_areas> list of work areas
   @verbatim
<work_areas> := <count>{ ',' x y width height } 
<count> := number of work areas to follow, 0 or greater 
@endverbatim


   @def         UNITY_RPC_WINDOW_MINIMIZE    
   @brief       Minimize the specified window.
   @code 
   UNITY_RPC_WINDOW_MINIMIZE UnityWindowId
   @endcode
   @param[in] UnityWindowId UnityWindowId of window to minimize 


   @def         UNITY_RPC_WINDOW_UNMINIMIZE
   @brief       Unminimizes a window to its pre-minimization state.
   @code 
   UNITY_RPC_WINDOW_UNMINIMIZE UnityWindowId 
   @endcode
   @param[in] UnityWindowId window to unminimize
   @note        This RPC originated as UNITY_RPC_WINDOW_RESTORE.  The actual
                GuestRpc command remains as "unity.window.restore" to maintain
                backwards compatibility.


   @def         UNITY_RPC_WINDOW_MAXIMIZE   
   @brief       Maximize the specified window.
   @code 
   UNITY_RPC_WINDOW_MAXIMIZE UnityWindowId
   @endcode
   @param[in] UnityWindowId UnityWindowId of window to maximize 


   @def         UNITY_RPC_WINDOW_UNMAXIMIZE    
   @brief       Unmaximize the specified window. 
   @code 
   UNITY_RPC_WINDOW_UNMAXIMIZE UnityWindowId
   @endcode
   @param[in] UnityWindowId UnityWindowId of window to unmaximize 


   @def         UNITY_RPC_DESKTOP_CONFIG_SET   
   @brief       Send desktop (virtual workspaces) configuration. 
   @code 
   UNITY_RPC_DESKTOP_CONFIG_SET configuration 
   @endcode
   @param[in] configuration <cell> {<cell>} <current> 
   @verbatim
<cell> := '{'row,col'}' 
<current> := currently active cell 
@endverbatim
The RPC takes the form of a set of row, column pairs, each enclosed by 
braces, and an integer. The number of rows and columns can be deduced 
by looking for the max row and column value specified in these pairs. 
The integer that follows the list of pairs defines the currently active 
desktop, as an offset (starting at 0) of the pair list.\n\n
For example: <tt>{1,1} {1,2} {2,1} {2,2} 1</tt> specifies a 2 x 2 virtual
desktop where the upper right <tt>{1,2}</tt> is the currently active desktop.


   @def         UNITY_RPC_DESKTOP_ACTIVE_SET   
   @brief       Change the active desktop to the value specified.
   @code 
   UNITY_RPC_DESKTOP_ACTIVE_SET offset
   @endcode
   @param[in] offset Offset into desktop configuration, as defined by @ref UNITY_RPC_DESKTOP_CONFIG_SET, 0 or greater 


   @def         UNITY_RPC_WINDOW_DESKTOP_SET   
   @brief       Change the desktop of the specified window. 
   @code 
   UNITY_RPC_WINDOW_DESKTOP_SET UnityWindowId offset
   @endcode
   @param[in] UnityWindowId UnityWindowId of window to move 
   @param[in] offset Offset into desktop configuration, as defined by
                     @ref UNITY_RPC_DESKTOP_CONFIG_SET, 0 or greater. 

   @}
*/

/* Host -> Guest */
#define UNITY_RPC_ENTER                   "unity.enter"
#define UNITY_RPC_GET_UPDATE              "unity.get.update"
#define UNITY_RPC_GET_WINDOW_PATH         "unity.get.window.path"
#define UNITY_RPC_GET_BINARY_INFO         "unity.get.binary.info"
#define UNITY_RPC_GET_BINARY_HANDLERS     "unity.get.binary.handlers"
#define UNITY_RPC_OPEN_LAUNCHMENU         "unity.launchmenu.open"
#define UNITY_RPC_GET_LAUNCHMENU_ITEM     "unity.launchmenu.get.item"
#define UNITY_RPC_CLOSE_LAUNCHMENU        "unity.launchmenu.close"
#define UNITY_RPC_WINDOW_SETTOP           "unity.window.settop"
#define UNITY_RPC_WINDOW_CLOSE            "unity.window.close"
#define UNITY_RPC_GET_WINDOW_CONTENTS     "unity.get.window.contents"
#define UNITY_RPC_GET_ICON_DATA           "unity.get.icon.data"
#define UNITY_RPC_EXIT                    "unity.exit"
#define UNITY_RPC_GET_UPDATE_FULL         "unity.get.update.full"
#define UNITY_RPC_GET_UPDATE_INCREMENTAL  "unity.get.update.incremental"
#define UNITY_RPC_SHELL_OPEN              "unity.shell.open"
#define UNITY_RPC_SHOW_TASKBAR            "unity.show.taskbar"
#define UNITY_RPC_WINDOW_MOVE_RESIZE      "unity.window.move_resize"
#define UNITY_RPC_DESKTOP_WORK_AREA_SET   "unity.desktop.work_area.set"
#define UNITY_RPC_WINDOW_MINIMIZE         "unity.window.minimize"
#define UNITY_RPC_WINDOW_UNMINIMIZE       "unity.window.restore"
#define UNITY_RPC_WINDOW_MAXIMIZE         "unity.window.maximize"
#define UNITY_RPC_WINDOW_UNMAXIMIZE       "unity.window.unmaximize"
#define UNITY_RPC_DESKTOP_CONFIG_SET      "unity.desktop.config.set"
#define UNITY_RPC_DESKTOP_ACTIVE_SET      "unity.desktop.active.set"
#define UNITY_RPC_WINDOW_DESKTOP_SET      "unity.window.desktop.set"

#define GHI_RPC_GUEST_SHELL_ACTION                    "ghi.guest.shell.action"
#define GHI_RPC_SET_GUEST_HANDLER                     "ghi.guest.handler.set"
#define GHI_RPC_RESTORE_DEFAULT_GUEST_HANDLER         "ghi.guest.handler.restoreDefault"

/**
   @defgroup UnityRpcGH Unity: Guest-to-Host RPCs
   @{

   @def         UNITY_RPC_PUSH_UPDATE_CMD
   @brief       Send a round of Unity Window Tracker updates to the host. 
   @todo doucment update format in unityWindowTracker.c
   @code 
   UNITY_RPC_PUSH_UPDATE_CMD commands
   @endcode
   @param[in]   A double-NUL-terminated string containing NUL-delimited update commands.
   @note        Updates are followed in the command string by Z-order information,
                and active desktop information. The update format is documented in 
                @ref unityWindowTracker.c 


   @def         UNITY_RPC_VMX_SHOW_TASKBAR        
   @brief       Ask the host to send its "show taskbar" setting.
   @code 
   UNITY_RPC_VMX_SHOW_TASKBAR 
   @endcode


   @def         UNITY_RPC_UNITY_CAP               
   @brief       Tell the host if the guest is capable of supporting Unity or not. 
   @code 
   UNITY_RPC_UNITY_CAP flag
   @endcode
   @param[in] flag If 1, guest supports Unity. If 0, it does not 

   @def         UNITY_RPC_SHOW_TASKBAR_CAP        
   @brief       Tells the host if the guest is capable of showing and hiding the
                taskbar. 
   @code 
   UNITY_RPC_SHOW_TASKBAR_CAP flag
   @endcode
   @param[in] flag If 1, taskbar visibility can be controlled. If 0, it cannot 

   @def         GHI_RPC_LAUNCHMENU_CHANGE         
   @brief       Inform the host that one or more launch menu items have changed.
   @todo        Move this define to a GHI-specific header.
   @code 
   GHI_RPC_LAUNCHMENU_CHANGE 
   @endcode


   @def         GHI_RPC_PROTOCOL_HANDLER_INFO     
   @brief       This command sends the list of protocol handlers to the host. 
   @todo        Move this define to a GHI-specific header.
   @code 
   GHI_RPC_PROTOCOL_HANDLER_INFO 
   @endcode
   @param data XDR data containing protocol handler info.
   @sa GHIPlatformGetProtocolHandlers for platform-specific list of handled protocols.
   @sa ghiProtocolHandler.x for XDR data format.


   @def         UNITY_RPC_UNITY_ACTIVE            
   @brief       Tell host we are entering or leaving Unity mode. 
   @code 
   UNITY_RPC_UNITY_ACTIVE flag
   @endcode
   @param[in] flag If 1, Unity is active. If 0, Unity is not active 

   @}
*/

/* Guest -> Host */
#define UNITY_RPC_PUSH_UPDATE_CMD         "tools.unity.push.update"
#define UNITY_RPC_VMX_SHOW_TASKBAR        "vmx.unity.show.taskbar"
#define UNITY_RPC_UNITY_CAP               "tools.capability.unity"
#define UNITY_RPC_SHOW_TASKBAR_CAP        "tools.capability.unity.taskbar"
#define GHI_RPC_LAUNCHMENU_CHANGE         "tools.ghi.launchmenu.change"
#define GHI_RPC_PROTOCOL_HANDLER_INFO     "tools.ghi.protocolhandler.info"
#define UNITY_RPC_UNITY_ACTIVE            "unity.active"

#define GHI_RPC_HOST_SHELL_ACTION         "ghi.host.shell.action"

/*
 * Currently we have four possible unity start menu roots.
 *
 * UNITY_START_MENU_LAUNCH_FOLDER is for all guest start menu 'Programs' items
 * plus favorite items from guest start menu folder.
 *
 * UNITY_START_MENU_FIXED_FOLDER is for special items like 'My Computer',
 * 'My Documents', 'Control Panel', etc.
 *
 * UNITY_START_MENU_ALL_HANDLERS_FOLDER is for all the applications that are known
 * by the guest to open files.
 *
 * UNITY_START_MENU_RESOLVED_LAUNCH_FOLDER is the same contents as
 * UNITY_START_MENU_LAUNCH_FOLDER however each item that is a shortcut (link) is resolved
 * into its destination path.
 */

#define UNITY_START_MENU_LAUNCH_FOLDER          "VMGuestLaunchItems"
#define UNITY_START_MENU_FIXED_FOLDER           "VMGuestFixedItems"
#define UNITY_START_MENU_ALL_HANDLERS_FOLDER    "VMGuestAllHandlers"
#define UNITY_START_MENU_RESOLVED_LAUNCH_FOLDER "VMGuestResolvedItems"

typedef uint32 UnityWindowId;

typedef int32 UnityDesktopId;

/*
 * Unity window state tracks the current state of the window (right now we only have
 * minimized, might have more in the future).
 *
 * These are deprecated in favor of window attributes and window types, and are retained
 * for compatibility purposes only.
 */

#define UNITY_WINDOW_STATE_MINIMIZED   (1 << 0)
#define UNITY_WINDOW_STATE_IN_FOCUS    (1 << 1)
#define UNITY_WINDOW_STATE_TOPMOST     (1 << 2)

/*
 * Unity window attributes are boolean flags that can be set in combination on a window.
 * If they are not set by the guest, it is up to the host to decide on a reasonable
 * default.
 *
 * If you add attributes here, please also update unityAttributeNames in
 * unityWindowTracker.c.
 */

typedef enum {
   UNITY_WINDOW_ATTR_BORDERLESS = 0,
   UNITY_WINDOW_ATTR_MINIMIZABLE = 1,
   UNITY_WINDOW_ATTR_MAXIMIZABLE = 2,
   UNITY_WINDOW_ATTR_MAXIMIZED = 3,
   UNITY_WINDOW_ATTR_CLOSABLE = 5,
   UNITY_WINDOW_ATTR_HAS_TITLEBAR = 6,
   UNITY_WINDOW_ATTR_VISIBLE = 7,
   UNITY_WINDOW_ATTR_CHILD_WINDOW = 8,
   UNITY_WINDOW_ATTR_HAS_TOOLBAR_BTN = 9,
   UNITY_WINDOW_ATTR_BELONGS_TO_APP = 10,
   UNITY_WINDOW_ATTR_DROPSHADOWED = 11,
   UNITY_WINDOW_ATTR_ALWAYS_ABOVE = 12,
   UNITY_WINDOW_ATTR_ALWAYS_BELOW = 13,
   UNITY_WINDOW_ATTR_DISABLED = 14,
   UNITY_WINDOW_ATTR_NOACTIVATE = 15,
   UNITY_WINDOW_ATTR_SYSMENU = 16,
   UNITY_WINDOW_ATTR_TOOLWINDOW = 17,
   UNITY_WINDOW_ATTR_APPWINDOW = 18,
   UNITY_WINDOW_ATTR_FULLSCREENABLE = 19,
   UNITY_WINDOW_ATTR_FULLSCREENED = 20,
   UNITY_WINDOW_ATTR_ATTN_WANTED = 21,
   UNITY_WINDOW_ATTR_SHADEABLE = 22,
   UNITY_WINDOW_ATTR_SHADED = 23,
   UNITY_WINDOW_ATTR_STICKABLE = 24,
   UNITY_WINDOW_ATTR_STICKY = 25,
   UNITY_WINDOW_ATTR_MODAL = 26,

   UNITY_MAX_ATTRIBUTES // Not a valid attribute
} UnityWindowAttribute;

typedef enum {
   UNITY_WINDOW_TYPE_NONE   = -1,
   UNITY_WINDOW_TYPE_NORMAL = 0,
   UNITY_WINDOW_TYPE_PANEL,
   UNITY_WINDOW_TYPE_DIALOG,
   UNITY_WINDOW_TYPE_MENU,
   UNITY_WINDOW_TYPE_TOOLTIP,
   UNITY_WINDOW_TYPE_SPLASH,
   UNITY_WINDOW_TYPE_TOOLBAR,
   UNITY_WINDOW_TYPE_DOCK,
   UNITY_WINDOW_TYPE_DESKTOP,
   UNITY_WINDOW_TYPE_COMBOBOX,
   UNITY_WINDOW_TYPE_WIDGET,

   UNITY_MAX_WINDOW_TYPES  // Not a valid window type
} UnityWindowType;

typedef enum {
   UNITY_ICON_TYPE_MAIN = 0,

   UNITY_MAX_ICONS // Not a valid icon type
} UnityIconType;

typedef uint32 UnityIconSize; // Number of pixels on the larger side of the icon (which is usually square anyways).
#define UNITY_MAX_ICON_DATA_CHUNK ((1 << 16) - 100) // 64k, minus space for a few other return values

#define UNITY_DEFAULT_COLOR "#c0c0c0"

#endif
