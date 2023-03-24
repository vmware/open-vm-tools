/*********************************************************
 * Copyright (c) 2008-2023 VMware, Inc. All rights reserved.
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
 * ghIntegrationCommon.h --
 *
 *	Common data structures and definitions used by Guest/Host Integration.
 */

#ifndef _GHINTEGRATIONCOMMON_H_
#define _GHINTEGRATIONCOMMON_H_

/*
 * Common data structures and definitions used by Guest/Host Integration.
 */
#define GHI_HGFS_SHARE_URL_SCHEME_UTF8    "x-vmware-share"
#define GHI_HGFS_SHARE_URL_UTF8           "x-vmware-share://"
#define GHI_HGFS_SHARE_URL                _T(GHI_HGFS_SHARE_URL_UTF8)

/*
 * Messages over different channels will be handled by
 * different modules.
 */
#define GHI_CHANNEL_TOOLS_USER                  0  // Handled by tools module
                                                   // in local VM (TOOLS_DND_NAME guestRPC)
                                                   // or by VDPUnityMKSControl module
                                                   // in View RMKS
#define GHI_CHANNEL_TOOLS_MAIN                  1  // Handled by tools module
                                                   // in local VM (TOOLS_DAEMON_NAME guestRPC)
#define GHI_CHANNEL_VIEW_REMOTE_SHARED_FOLDER   2  // VDPSharedFolderMgrMKSControl module
                                                   // in View RMKS
#define GHI_CHANNEL_DND                         3  // DnD for both local VM and View RMKS.
#define GHI_CHANNEL_VIEW_REMOTE_RDE_COMMON      4  // VDPRdeCommonMKSControl module
                                                   // in View RMKS
#define GHI_CHANNEL_VIEW_USB_REDIRECTION        5  // VDPUsbRedirectionMKSControl module in
                                                   // View RMKS
#define GHI_CHANNEL_VIEW_REMOTE_VDP_COMMON      6  // Handled by View VDP core module
#define GHI_CHANNEL_VIEW_PROTOCOL               7  // Interactions with different protocols
                                                   // in View RMKS
#define GHI_CHANNEL_FCP                         8  // FCP for View RMKS
#define GHI_CHANNEL_VIEW_SDR                    9  // Handled by View SDR
#define GHI_CHANNEL_VIEW_WHFB_REDIRECTION       10 // WhfbRedirectionMKSControl module in
                                                   // View RMKS
#define GHI_CHANNEL_VIEW_SCREEN_CAPTURE         11 // ScreenCapture in View RMKS
#define GHI_CHANNEL_COUNT                       12

typedef uint32 GHIChannelType;

#define GHI_REQUEST_SUCCESS_OK                  0  // Guest received the message and returned OK.
#define GHI_REQUEST_SUCCESS_ERROR               1  // Guest received the message but returned ERROR.
#define GHI_REQUEST_GUEST_RPC_FAILED            2  // Not sent to guest
                                                   // or guest failed to return,
                                                   // including timeout.
#define GHI_REQUEST_GENERAL_ERROR               3  // General error, can be guest error
                                                   // or prc error.
#define GHI_REQUEST_FAILED_WITH_UTF8_MESSAGE    4  // Failed and with utf8 error message returned.

typedef uint32 GHIRequestResult;

#define GHI_GUEST_CHANNEL_BITS(channel)   ((channel) << 24)
#define GHI_GUEST_GET_MSG_CHANNEL(msg)    (((msg) >> 24) & 0xff)
typedef uint32 GHIGuestToHostMessageType;


/*
 * MKS->UI messages over GHI_CHANNEL_VIEW_REMOTE_SHARED_FOLDER.
 *
 * Only for View product.
 */
#define GHI_CHANNEL_VIEW_REMOTE_SHARED_FOLDER_BITS \
            GHI_GUEST_CHANNEL_BITS(GHI_CHANNEL_VIEW_REMOTE_SHARED_FOLDER)
#define GHI_GUEST_RDPDR_CAP   (GHI_CHANNEL_VIEW_REMOTE_SHARED_FOLDER_BITS | 0x000001)


/*
 * UI->MKS Messages over GHI_CHANNEL_DND.
 */
#define GHI_DND_DND_HOST_GUEST_CMD              "ghi.dnd.dnd.hostguest"
#define GHI_DND_COPYPASTE_HOST_GUEST_CMD        "ghi.dnd.copypaste.hostguest"
#define GHI_DND_HOST_SHAKEHAND_CMD              "ghi.dnd.shakehand"
#define GHI_DND_HOST_GETFILES_CMD               "ghi.dnd.host.getfiles"
#define GHI_DND_HOST_GETFILES_ANSWER_OVERWRITE  "ghi.dnd.host.getfiles.answer.overwrite"
#define GHI_DND_HOST_SENDFILES_CMD              "ghi.dnd.host.sendfiles"
#define GHI_DND_HOST_TRANSFERFILES_CANCEL_CMD   "ghi.dnd.host.transferfiles.cancel"
#define GHI_DND_HOST_ADDBLOCK_CMD               "ghi.dnd.host.addblock"
#define GHI_DND_HOST_REMOVEBLOCK_CMD            "ghi.dnd.host.removeblock"

/*
 * Results of UI->MKS Messages over GHI_CHANNEL_DND.
 */
#define GHI_DND_GUEST_RET_MAX_LEN               64
#define GHI_DND_GUEST_RET_ERROR                 "error"
#define GHI_DND_GUEST_RET_INPROGRESS            "inProgress"
#define GHI_DND_GUEST_RET_DONE                  "done"

/*
 * MKS->UI messages over GHI_CHANNEL_DND.
 */
#define GHI_CHANNEL_DND_BITS                          GHI_GUEST_CHANNEL_BITS(GHI_CHANNEL_DND)
#define GHI_GUEST_DND_DND_CMD                         (GHI_CHANNEL_DND_BITS | 0x000001)
#define GHI_GUEST_DND_COPYPASTE_CMD                   (GHI_CHANNEL_DND_BITS | 0x000002)
#define GHI_GUEST_DND_NOTIFY_BLOCKROOT                (GHI_CHANNEL_DND_BITS | 0x000003)
#define GHI_GUEST_DND_TRANSFERFILES_PROGRESS          (GHI_CHANNEL_DND_BITS | 0x000004)
#define GHI_GUEST_DND_GETFILE_OVERWRITE_QUESTION      (GHI_CHANNEL_DND_BITS | 0x000005)
#define GHI_GUEST_DND_CAPABILITY                      (GHI_CHANNEL_DND_BITS | 0x000006)


/*
 * UI->MKS Messages over GHI_CHANNEL_VIEW_REMOTE_RDE_COMMON.
 */
#define GHI_RDE_COMMON_GENERIC_CMD              "ghi.rde.generic"
#define GHI_RDE_COMMON_SET_IME_ENABLED_CMD      "ghi.rde.set.ime.enabled"
#define GHI_RDE_COMMON_SET_IME_HOST_KEYS_CMD    "ghi.rde.set.ime.host.keys"

/*
 * MKS->UI messages over GHI_CHANNEL_VIEW_REMOTE_RDE_COMMON.
 */
#define GHI_CHANNEL_VIEW_REMOTE_RDE_COMMON_BITS \
            GHI_GUEST_CHANNEL_BITS(GHI_CHANNEL_VIEW_REMOTE_RDE_COMMON)
#define GHI_GUEST_RDE_COMMON_HOST_SET_DPI \
            (GHI_CHANNEL_VIEW_REMOTE_RDE_COMMON_BITS | 0x000001)
#define GHI_GUEST_RDE_COMMON_UNLOCK_DESKTOP \
            (GHI_CHANNEL_VIEW_REMOTE_RDE_COMMON_BITS | 0x000002)
#define GHI_GUEST_RDE_COMMON_CLIPBOARD_DATA_SENT_DONE \
            (GHI_CHANNEL_VIEW_REMOTE_RDE_COMMON_BITS | 0x000003)
#define GHI_GUEST_RDE_COMMON_GENERIC \
            (GHI_CHANNEL_VIEW_REMOTE_RDE_COMMON_BITS | 0x000004)

/*
 * MKS->UI messages over GHI_CHANNEL_VIEW_USB_REDIRECTION.
 */
#define GHI_CHANNEL_VIEW_USB_REDIRECTION_BITS \
            GHI_GUEST_CHANNEL_BITS(GHI_CHANNEL_VIEW_USB_REDIRECTION)
#define GHI_GUEST_USB_REDIRECTION_USB_INSTANCE_ID \
            (GHI_CHANNEL_VIEW_USB_REDIRECTION_BITS | 0x000001)
#define GHI_GUEST_USB_REDIRECTION_DEVICES_FILTER_STATUS \
            (GHI_CHANNEL_VIEW_USB_REDIRECTION_BITS | 0x000002)

/*
 * UI->MKS messages over GHI_CHANNEL_VIEW_USB_REDIRECTION.
 */
#define GHI_HOST_USB_REDIRECTION_STARTUSBD_CMD  "ghi.usb.redirection.startusbd"

 /*
  * UI->MKS messages over GHI_CHANNEL_VIEW_PROTOCOL.
  */
#define GHI_SET_BUFFER_WITHOUT_AUDIO_CMD \
        "ghi.view.protocol.set.buffer.without.audio"

/*
 * MKS->UI messages over GHI_CHANNEL_FCP, used by View FCP.
 */
#define GHI_CHANNEL_FCP_BITS                          GHI_GUEST_CHANNEL_BITS(GHI_CHANNEL_FCP)
#define GHI_GUEST_FCP_TRANSFERFILES_PROGRESS          (GHI_CHANNEL_FCP_BITS | 0x000001)

/*
 * UI->MKS Messages over GHI_CHANNEL_FCP, used by View FCP.
 */
#define GHI_FCP_HOST_TRANSFERFILES_CANCEL_CMD   "ghi.fcp.host.transferfiles.cancel"

/*
 * MKS->UI messages over GHI_CHANNEL_VIEW_REMOTE_VDP_COMMON.
 */
#define GHI_CHANNEL_VIEW_REMOTE_VDP_COMMON_BITS \
           GHI_GUEST_CHANNEL_BITS(GHI_CHANNEL_VIEW_REMOTE_VDP_COMMON)
#define GHI_GUEST_VDP_COMMON_CAP_FEATURES \
           (GHI_CHANNEL_VIEW_REMOTE_VDP_COMMON_BITS | 0x000001)
#define GHI_GUEST_VDP_COMMON_CAP_RECEIVED \
           (GHI_CHANNEL_VIEW_REMOTE_VDP_COMMON_BITS | 0x000002)

/*
 * UI->MKS messages over GHI_CHANNEL_VIEW_REMOTE_VDP_COMMON.
 */
#define GHI_HOST_VDP_COMMON_SYNC_GUEST_LEDS_CMD  "ghi.mks.common.sync.guest.leds"
#define GHI_HOST_VDP_COMMON_GET_GUEST_CAPS_CMD   "ghi.mks.common.get.guest.caps"

/*
 * MKS->UI messages over GHI_CHANNEL_VIEW_WHFB_REDIRECTION
 */
#define GHI_CHANNEL_VIEW_WHFB_REDIRECTION_BITS \
           GHI_GUEST_CHANNEL_BITS(GHI_CHANNEL_VIEW_WHFB_REDIRECTION)
#define GHI_GUEST_WHFB_REDIRECTION_UNLOCK_REQUEST \
           (GHI_CHANNEL_VIEW_WHFB_REDIRECTION_BITS | 0x000001)

/*
 * UI->MKS messages over GHI_CHANNEL_VIEW_WHFB_REDIRECTION.
 */
#define GHI_WHFB_REDIRECTION_SET_SESSIONPIN_CMD              "ghi.whfb.set.sessionpin"
#define GHI_WHFB_REDIRECTION_SET_USERVERIFICATIONRESULT_CMD  "ghi.whfb.set.userverificationresult"

/*
 * Capabilities for the message GHI_GUEST_VDP_COMMON_CAP_FEATURES
 */
typedef enum {
   VDP_COMMON_SET_KEYBOARD_STATE_CAP = 0,
   VDP_COMMON_CAP_ITEM_COUNT
} VDPCommonCapType;

/*
 * UI->MKS messages over GHI_CHANNEL_VIEW_SDR
 */
#define GHI_VIEW_SDR_ADD_DRIVE      "ghi.view.sdr.add.drive"
#define GHI_VIEW_SDR_REMOVE_DRIVE   "ghi.view.sdr.remove.drive"

/*
 * MKS->UI messages over GHI_CHANNEL_VIEW_SDR.
 */
#define GHI_CHANNEL_VIEW_SDR_BITS         GHI_GUEST_CHANNEL_BITS(GHI_CHANNEL_VIEW_SDR)
#define GHI_VIEW_SDR_VDP_CONNECTED        (GHI_CHANNEL_VIEW_SDR_BITS | 0x000001)
#define GHI_VIEW_SDR_VDP_DISCONNECTED     (GHI_CHANNEL_VIEW_SDR_BITS | 0x000002)
#define GHI_VIEW_SDR_VDP_SDRPOLICY        (GHI_CHANNEL_VIEW_SDR_BITS | 0x000003)


/*
 * UI->MKS messages over GHI_CHANNEL_VIEW_SCREEN_CAPTURE
 */
#define GHI_VIEW_SCREEN_CAPTURE_TAKE_SNAPSHOT      "ghi.view.screen.capture.take.snapshot"
#define GHI_VIEW_SCREEN_CAPTURE_ENUM_TOPOLOGY      "ghi.view.screen.capture.enum.topology"

/*
 * MKS->UI messages over GHI_CHANNEL_VIEW_SCREEN_CAPTURE.
 */
#define GHI_CHANNEL_VIEW_SCREEN_CAPTURE_BITS \
           GHI_GUEST_CHANNEL_BITS(GHI_CHANNEL_VIEW_SCREEN_CAPTURE)
#define GHI_GUEST_SCREEN_CAPTURE_FUNC_READY        (GHI_CHANNEL_VIEW_SCREEN_CAPTURE_BITS | 0x000001)
#define GHI_GUEST_SCREEN_CAPTURE_TOPOLOGY          (GHI_CHANNEL_VIEW_SCREEN_CAPTURE_BITS | 0x000002)

#endif // ifndef _GHINTEGRATIONCOMMON_H_
