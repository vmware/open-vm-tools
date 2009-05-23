/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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
 * foundryMsg.c --
 *
 * This is a library for formatting and parsing the messages sent
 * between a foundry client and the VMX. It is a stand-alone library 
 * so it can be used by the VMX tree without also linking in the 
 * entire foundry client-side library.
 */

#include "vmware.h"
#include "vm_version.h"
#include "util.h"
#include "str.h"
#include "base64.h"

#include "vixOpenSource.h"
#include "vixCommands.h"

static char PlainToObfuscatedCharMap[256];
static char ObfuscatedToPlainCharMap[256];


/*
 * An entry in the command info table. There is one VixCommandInfo per op
 * code, and each entry contains a description of the op code plus security-
 * related metadata.
 */
typedef struct VixCommandInfo {
   int                         opCode;
   const char                  *commandName;
   VixCommandSecurityCategory  category;
   Bool                        used;     // Is there an opcode for this entry?
} VixCommandInfo;

#define VIX_DEFINE_COMMAND_INFO(x, category) { x, #x, category, TRUE }
#define VIX_DEFINE_UNUSED_COMMAND  { 0, NULL, VIX_COMMAND_CATEGORY_UNKNOWN, FALSE }

/*
 * Contains the information for every VIX command op code. This table is
 * organized to allow for direct look up, so it must be complete. Any index
 * that does not correspond to a valid VIX op code must be marked with
 * VIX_DEFINE_UNUSED_COMMAND.
 *
 * When you add or remove a command to vixCommands.h, this table needs to
 * be updated as well. When adding a new command, you need to give it a
 * security category. There are descriptions of the categories in vixCommands.h
 * where they are defined, but in general, if the command affects the host or
 * a VM (but not the guest), then the command should be CATEGORY_PRIVILEGED.
 * If the command is a guest command (a command the runs inside the guest
 * OS) than it should be CATEGORY_ALWAYS_ALLOWED. Also, if a command is
 * required to establish a connection with the VMX, it needs to be
 * CATEGORY_ALWAYS_ALLOWED.
 */

static const VixCommandInfo vixCommandInfoTable[] = {
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_UNKNOWN,
                           VIX_COMMAND_CATEGORY_UNKNOWN),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_VM_POWERON,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_VM_POWEROFF,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_VM_RESET,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_VM_SUSPEND,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_RUN_PROGRAM,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_PROPERTY,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SET_PROPERTY,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_KEYSTROKES,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_READ_REGISTRY,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_WRITE_REGISTRY,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_COPY_FILE_FROM_GUEST_TO_HOST,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_COPY_FILE_FROM_HOST_TO_GUEST,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CREATE_SNAPSHOT,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_REMOVE_SNAPSHOT,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_REVERT_TO_SNAPSHOT,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_VM_CLONE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_DELETE_GUEST_FILE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GUEST_FILE_EXISTS,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_FIND_VM,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CALL_PROCEDURE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_REGISTRY_KEY_EXISTS,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_WIN32_WINDOW_MESSAGE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CONSOLIDATE_SNAPSHOTS,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_INSTALL_TOOLS,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CANCEL_INSTALL_TOOLS,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_UPGRADE_VIRTUAL_HARDWARE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SET_NIC_BANDWIDTH,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CREATE_DISK,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CREATE_FLOPPY,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_RELOAD_VM,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_DELETE_VM,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SYNCDRIVER_FREEZE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SYNCDRIVER_THAW,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_HOT_ADD_DISK,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_HOT_REMOVE_DISK,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SET_GUEST_PRINTER,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_WAIT_FOR_TOOLS,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CREATE_RUNNING_VM_SNAPSHOT,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CONSOLIDATE_RUNNING_VM_SNAPSHOT,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_NUM_SHARED_FOLDERS,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_SHARED_FOLDER_STATE, 
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_EDIT_SHARED_FOLDER_STATE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_REMOVE_SHARED_FOLDER,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_ADD_SHARED_FOLDER,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_RUN_SCRIPT_IN_GUEST,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_OPEN_VM,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_DISK_PROPERTIES,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_OPEN_URL,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   /* GET_HANDLE_STATE is needed for the initial handshake */
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_HANDLE_STATE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SET_HANDLE_STATE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CREATE_WORKING_COPY,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_DISCARD_WORKING_COPY,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SAVE_WORKING_COPY,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CAPTURE_SCREEN,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_VMDB_VALUES,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SET_VMDB_VALUES,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_READ_XML_FILE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_TOOLS_STATE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CHANGE_SCREEN_RESOLUTION,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_DIRECTORY_EXISTS,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_DELETE_GUEST_REGISTRY_KEY,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_DELETE_GUEST_DIRECTORY,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_DELETE_GUEST_EMPTY_DIRECTORY,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CREATE_TEMPORARY_FILE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_LIST_PROCESSES,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_MOVE_GUEST_FILE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CREATE_DIRECTORY,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CHECK_USER_ACCOUNT,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_LIST_DIRECTORY,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_REGISTER_VM,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_UNREGISTER_VM,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_UNUSED_COMMAND,
   /* CREATE_SESSION_KEY is needed for the initial handshake */
   VIX_DEFINE_COMMAND_INFO(VIX_CREATE_SESSION_KEY_COMMAND,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VMXI_HGFS_SEND_PACKET_COMMAND,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_KILL_PROCESS,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_VM_FORK_COMMAND,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_LOGOUT_IN_GUEST,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_READ_VARIABLE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_WRITE_VARIABLE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CONNECT_DEVICE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_IS_DEVICE_CONNECTED,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_FILE_INFO,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SET_FILE_INFO,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_MOUSE_EVENTS,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_OPEN_TEAM,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_FIND_HOST_DEVICES,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_ANSWER_MESSAGE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_ENABLE_SHARED_FOLDERS,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_MOUNT_HGFS_FOLDERS,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_HOT_EXTEND_DISK,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_UNUSED_COMMAND,
   
   /*
    * vProbes are only available remotely through VIX, so we should
    * let them through. If they get added to the VMODL with their own
    * permissions, then we will have to revisit.
    */
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_VPROBES_VERSION,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_VPROBES,
                    VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_VPROBE_GET_GLOBALS,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_VPROBE_LOAD,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_VPROBE_RESET,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   
   /* LIST_USB_DEVICES is needed for the initial handshake. */
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_LIST_USB_DEVICES,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CONNECT_HOST,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CREATE_LINKED_CLONE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_STOP_SNAPSHOT_LOG_RECORDING,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_STOP_SNAPSHOT_LOG_PLAYBACK,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   /*
    * HOWTO: Adding a new Vix Command. Step 2b.
    * Take the command you added to vixCommands.h, and add it to this
    * table. The command needs to go in the index that matches the command
    * ID as specified in the enum in vixCommands.h.
    */
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SAMPLE_COMMAND,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_GUEST_NETWORKING_CONFIG,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SET_GUEST_NETWORKING_CONFIG,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_FAULT_TOLERANCE_REGISTER,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_FAULT_TOLERANCE_UNREGISTER,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_FAULT_TOLERANCE_CONTROL,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_FAULT_TOLERANCE_QUERY_SECONDARY,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_VM_PAUSE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_VM_UNPAUSE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_SNAPSHOT_LOG_INFO,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SET_REPLAY_SPEED,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_ANSWER_USER_MESSAGE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SET_CLIENT_LOCALE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_PERFORMANCE_DATA,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_REFRESH_RUNTIME_PROPERTIES,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_SNAPSHOT_SCREENSHOT,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_ADD_TIMEMARKER,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_WAIT_FOR_USER_ACTION_IN_GUEST,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_VMDB_END_TRANSACTION,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_VMDB_SET,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CHANGE_VIRTUAL_HARDWARE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_HOT_PLUG_CPU,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_HOT_PLUG_MEMORY,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_HOT_ADD_DEVICE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_HOT_REMOVE_DEVICE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_DEBUGGER_ATTACH,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_DEBUGGER_DETACH,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_DEBUGGER_SEND_COMMAND,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_RECORD_STATE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SET_RECORD_STATE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_REMOVE_RECORD_STATE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_REPLAY_STATE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SET_REPLAY_STATE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_REMOVE_REPLAY_STATE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CANCEL_USER_PROGRESS_MESSAGE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   /* GET_VMX_DEVICE_STATE is needed for the initial handshake. */   
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_VMX_DEVICE_STATE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_NUM_TIMEMARKERS,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_TIMEMARKER,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_REMOVE_TIMEMARKER,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SET_SNAPSHOT_INFO,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SNAPSHOT_SET_MRU,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_LOGOUT_HOST,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_HOT_PLUG_BEGIN_BATCH,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_HOT_PLUG_COMMIT_BATCH,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_TRANSFER_CONNECTION,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_TRANSFER_REQUEST,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_TRANSFER_FINAL_DATA,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_ADD_ROLLING_SNAPSHOT_TIER,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_REMOVE_ROLLING_SNAPSHOT_TIER,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_LIST_ROLLING_SNAPSHOT_TIER,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_ADD_ROLLING_SNAPSHOT_TIER_VMX,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_REMOVE_ROLLING_SNAPSHOT_TIER_VMX,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_LIST_ROLLING_SNAPSHOT_TIER_VMX,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_LIST_FILESYSTEMS,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

};


static const VixCommandInfo *VixGetCommandInfoForOpCode(int opCode);

static void VixMsgInitializeObfuscationMapping(void);

static char *VixMsgEncodeBuffer(const uint8 *buffer,
                                size_t bufferLength,
                                Bool includeEncodingId);

static char *VixMsgDecodeBuffer(const char *str,
                                Bool nullTerminateResult,
                                size_t *bufferLength);


/*
 *----------------------------------------------------------------------------
 *
 * VixMsg_AllocResponseMsg --
 *
 *      Allocate and initialize a response message.
 *
 * Results:
 *      The message, with the headers properly initialized.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

VixCommandResponseHeader *
VixMsg_AllocResponseMsg(const VixCommandRequestHeader *requestHeader, // IN
                        VixError error,                               // IN
                        uint32 additionalError,                       // IN
                        size_t responseBodyLength,                    // IN
                        const void *responseBody,                     // IN
                        size_t *responseMsgLength)                    // OUT
{
   char *responseBuffer = NULL;
   VixCommandResponseHeader *responseHeader;
   size_t totalMessageSize;

   ASSERT((NULL != responseBody) || (0 == responseBodyLength));

   /*
    * We don't have scatter/gather, so copy everything into one buffer.
    */
   totalMessageSize = sizeof(VixCommandResponseHeader) + responseBodyLength;
   if (totalMessageSize > VIX_COMMAND_MAX_SIZE) {
      /*
       * We don't want to allocate any responses larger than
       * VIX_COMMAND_MAX_SIZE, since the VMX will ignore them.
       * If we hit this ASSERT, we will need to either revise this
       * value, or start packetizing certain commands.
       */
      ASSERT(0);
      return NULL;
   }

   responseBuffer = Util_SafeMalloc(totalMessageSize);
   responseHeader = (VixCommandResponseHeader *) responseBuffer;

   VixMsg_InitResponseMsg(responseHeader,
                          requestHeader,
                          error,
                          additionalError,
                          totalMessageSize);

   if ((responseBodyLength > 0) && (responseBody)) {
      memcpy(responseBuffer + sizeof(VixCommandResponseHeader), 
             responseBody,
             responseBodyLength);
   }

   if (NULL != responseMsgLength) {
      *responseMsgLength = totalMessageSize;
   }

   return responseHeader;
} // VixMsg_AllocResponseMsg


/*
 *----------------------------------------------------------------------------
 *
 * VixMsg_InitResponseMsg --
 *
 *      Initialize a response message.
 *
 * Results:
 *      The message, with the headers properly initialized.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
VixMsg_InitResponseMsg(VixCommandResponseHeader *responseHeader,     // IN
                       const VixCommandRequestHeader *requestHeader, // IN
                       VixError error,                               // IN
                       uint32 additionalError,                       // IN
                       size_t totalMessageSize)                      // IN
{
   size_t responseBodyLength;

   ASSERT(NULL != responseHeader);
   ASSERT(totalMessageSize >= sizeof(VixCommandResponseHeader));

   responseBodyLength = totalMessageSize - sizeof(VixCommandResponseHeader);

   /*
    * Fill in the response header.
    */
   responseHeader->commonHeader.magic = VIX_COMMAND_MAGIC_WORD;
   responseHeader->commonHeader.messageVersion = VIX_COMMAND_MESSAGE_VERSION;
   responseHeader->commonHeader.totalMessageLength = totalMessageSize;
   responseHeader->commonHeader.headerLength = sizeof(VixCommandResponseHeader);
   responseHeader->commonHeader.bodyLength = responseBodyLength;
   responseHeader->commonHeader.credentialLength = 0;
   responseHeader->commonHeader.commonFlags = 0;
   if (NULL != requestHeader) {
      responseHeader->requestCookie = requestHeader->cookie;
   } else {
      responseHeader->requestCookie = 0;
   }
   responseHeader->responseFlags = 0;
   responseHeader->duration = 0xFFFFFFFF;
   responseHeader->error = error;
   responseHeader->additionalError = additionalError;
   responseHeader->errorDataLength = 0;
} // VixMsg_InitResponseMsg


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg_AllocRequestMsg --
 *
 *      Allocate and initialize a request message.
 *
 * Results:
 *      The message, with the headers properly initialized.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixCommandRequestHeader *
VixMsg_AllocRequestMsg(size_t msgHeaderAndBodyLength,    // IN
                       int opCode,                    // IN
                       uint64 cookie,                 // IN
                       int credentialType,            // IN
                       const char *userNamePassword)  // IN
{
   size_t totalMessageSize;
   VixCommandRequestHeader *commandRequest = NULL;
   size_t credentialLength = 0;
   size_t namePasswordLength = 0;
   char *destPtr;

   if ((VIX_USER_CREDENTIAL_NAME_PASSWORD == credentialType) 
      || (VIX_USER_CREDENTIAL_HOST_CONFIG_SECRET == credentialType)
      || (VIX_USER_CREDENTIAL_HOST_CONFIG_HASHED_SECRET == credentialType)) {
      /*
       * Both of these are optional.
       */
      if (NULL != userNamePassword) {
         namePasswordLength = strlen(userNamePassword);
         credentialLength += namePasswordLength;
      }
      /*
       * Add 1 to each string to include '\0' for the end of the string.
       */
      credentialLength += 1;
   } else {
      credentialLength = 0;
   }

   totalMessageSize = msgHeaderAndBodyLength + credentialLength;
   if (totalMessageSize > VIX_COMMAND_MAX_REQUEST_SIZE) {
      /*
       * We don't want to allocate any requests larger than
       * VIX_COMMAND_MAX_REQUEST_SIZE, since the VMX will ignore them.
       * If we hit this ASSERT, we will need to either revise this
       * value, or start packetizing certain commands.
       */
      ASSERT(0);
      return NULL;
   }

   commandRequest = (VixCommandRequestHeader *) 
                        Util_SafeCalloc(1, totalMessageSize);

   commandRequest->commonHeader.magic = VIX_COMMAND_MAGIC_WORD;
   commandRequest->commonHeader.messageVersion = VIX_COMMAND_MESSAGE_VERSION;
   commandRequest->commonHeader.totalMessageLength =
      msgHeaderAndBodyLength + credentialLength;
   commandRequest->commonHeader.headerLength = sizeof(VixCommandRequestHeader);
   commandRequest->commonHeader.bodyLength = msgHeaderAndBodyLength -
      sizeof(VixCommandRequestHeader);
   commandRequest->commonHeader.credentialLength = credentialLength;
   commandRequest->commonHeader.commonFlags = VIX_COMMAND_REQUEST;

   commandRequest->opCode = opCode;
   commandRequest->cookie = cookie;
   commandRequest->timeOut = 0xFFFFFFFF;
   commandRequest->requestFlags = 0;

   commandRequest->userCredentialType = credentialType;

   if ((VIX_USER_CREDENTIAL_NAME_PASSWORD == credentialType)
         || (VIX_USER_CREDENTIAL_HOST_CONFIG_SECRET == credentialType)
         || (VIX_USER_CREDENTIAL_HOST_CONFIG_HASHED_SECRET == credentialType)) {
      destPtr = (char *) commandRequest;
      destPtr += commandRequest->commonHeader.headerLength;
      destPtr += commandRequest->commonHeader.bodyLength;
      if (NULL != userNamePassword) {
         Str_Strcpy(destPtr, userNamePassword, namePasswordLength + 1);
         destPtr += namePasswordLength;
      }
      *(destPtr++) = 0;
   }

   return commandRequest;
} // VixMsg_AllocRequestMsg


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg_ValidateMessage --
 *
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixMsg_ValidateMessage(const void *vMsg, // IN
                       size_t msgLength) // IN
{
   const VixMsgHeader *message;

   if ((NULL == vMsg) || (msgLength < sizeof *message)) {
      return VIX_E_INVALID_MESSAGE_HEADER;
   }

   /*
    * Sanity check the header.
    * Some basic rules: All the length values in the VixMsgHeader
    * struct are uint32. The headerLength must be large enough to
    * accomodate the base header: VixMsgHeader. The bodyLength and
    * the credentialLength can be 0. 
    *
    * We cannot compare message->totalMessageLength and msgLength.
    * When we first read just the header, message->totalMessageLength 
    * is > msgLength. When we have read the whole message, then
    * message->totalMessageLength <= msgLength. So, it depends on
    * when we call this function. Instead, we just make sure the message
    * is internally consistent, and then rely on the higher level code to
    * decide how much to read and when it has read the whole message.
    */
   message = vMsg;
   if ((VIX_COMMAND_MAGIC_WORD != message->magic)
         || (message->headerLength < sizeof(VixMsgHeader))
         || (message->totalMessageLength
               < ((uint64)message->headerLength + message->bodyLength + message->credentialLength))
         || (message->totalMessageLength > VIX_COMMAND_MAX_SIZE)
         || (VIX_COMMAND_MESSAGE_VERSION != message->messageVersion)) {
      return VIX_E_INVALID_MESSAGE_HEADER;
   }

   return VIX_OK;
} // VixMsg_ValidateMessage


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg_ValidateRequestMsg --
 *
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixMsg_ValidateRequestMsg(const void *vMsg, // IN
                          size_t msgLength) // IN
{
   VixError err;
   const VixCommandRequestHeader *message;

   err = VixMsg_ValidateMessage(vMsg, msgLength);
   if (VIX_OK != err) {
      return(err);
   }

   /*
    * Sanity check the parts of the header that are specific to requests.
    */
   message = vMsg;
   if (message->commonHeader.headerLength < sizeof(VixCommandRequestHeader)) {
      return VIX_E_INVALID_MESSAGE_HEADER;
   }

   if (message->commonHeader.totalMessageLength > VIX_COMMAND_MAX_REQUEST_SIZE) {
      return VIX_E_INVALID_MESSAGE_HEADER;
   }

   if (!(VIX_COMMAND_REQUEST & message->commonHeader.commonFlags)) {
      return VIX_E_INVALID_MESSAGE_HEADER;
   }

   return VIX_OK;
} // VixMsg_ValidateRequestMsg


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg_ValidateResponseMsg --
 *
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixMsg_ValidateResponseMsg(const void *vMsg, // IN
                           size_t msgLength) // IN
{
   VixError err;
   const VixCommandResponseHeader *message;

   if ((NULL == vMsg) || (msgLength < sizeof *message)) {
      return VIX_E_INVALID_MESSAGE_HEADER;
   }

   err = VixMsg_ValidateMessage(vMsg, msgLength);
   if (VIX_OK != err) {
      return(err);
   }

   /*
    * Sanity check the parts of the header that are specific to responses.
    */
   message = vMsg;
   if (message->commonHeader.headerLength < sizeof(VixCommandResponseHeader)) {
      return VIX_E_INVALID_MESSAGE_HEADER;
   }

   if (VIX_COMMAND_REQUEST & message->commonHeader.commonFlags) {
      return VIX_E_INVALID_MESSAGE_HEADER;
   }

   return VIX_OK;
} // VixMsg_ValidateResponseMsg


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg_ParseWriteVariableRequest --
 *
 *      Extract the value's name and the value itself from the request
 *      message, while validating message.
 *
 *      The strings returned from this function just point to memory in
 *      the message itself, so they must not be free()'d.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixMsg_ParseWriteVariableRequest(VixMsgWriteVariableRequest *msg,   // IN
                                 char **valueName,                  // OUT
                                 char **value)                      // OUT
{
   VixError err;
   char *valueNameLocal = NULL;
   char *valueLocal = NULL;
   uint64 headerAndBodyLength;

   if ((NULL == msg) || (NULL == valueName) || (NULL == value)) {
      ASSERT(0);
      err = VIX_E_FAIL;
      goto abort;
   }

   *valueName = NULL;
   *value = NULL;

   /*
    * In most cases we will have already called VixMsg_ValidateResponseMsg()
    * on this request before, but call it here so that this function will
    * always be sufficient to validate the request.
    */
   err = VixMsg_ValidateRequestMsg(msg,
                                   msg->header.commonHeader.totalMessageLength);
   if (VIX_OK != err) {
      goto abort;
   }

   if (msg->header.commonHeader.totalMessageLength < sizeof *msg) {
      err = VIX_E_INVALID_MESSAGE_BODY;
      goto abort;
   }

   headerAndBodyLength = (uint64) msg->header.commonHeader.headerLength
                            + msg->header.commonHeader.bodyLength;

   if (headerAndBodyLength < ((uint64) sizeof *msg
                                 + msg->nameLength + 1
                                 + msg->valueLength + 1)) {
      err = VIX_E_INVALID_MESSAGE_BODY;
      goto abort;
   }

   valueNameLocal = ((char *) msg) + sizeof(*msg);
   if ('\0' != valueNameLocal[msg->nameLength]) {
      err = VIX_E_INVALID_MESSAGE_BODY;
      goto abort;
   }

   valueLocal = valueNameLocal + msg->nameLength + 1;
   if ('\0' != valueLocal[msg->valueLength]) {
      err = VIX_E_INVALID_MESSAGE_BODY;
      goto abort;
   }

   *valueName = valueNameLocal;
   *value = valueLocal;
   err = VIX_OK;

abort:

   return err;
} // VixMsg_ParseWriteVariableRequest


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsgInitializeObfuscationMapping --
 *
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void 
VixMsgInitializeObfuscationMapping(void)
{
   size_t charIndex;
   static Bool initializedTable = FALSE;

   if (initializedTable) {
      return;
   }

   for (charIndex = 0; charIndex < sizeof(PlainToObfuscatedCharMap); charIndex++) {
      PlainToObfuscatedCharMap[charIndex] = 0;
      ObfuscatedToPlainCharMap[charIndex] = 0;
   }

   PlainToObfuscatedCharMap['\\'] = '1';
   PlainToObfuscatedCharMap['\''] = '2';
   PlainToObfuscatedCharMap['\"'] = '3';
   PlainToObfuscatedCharMap[' '] = '4';
   PlainToObfuscatedCharMap['\r'] = '5';
   PlainToObfuscatedCharMap['\n'] = '6';
   PlainToObfuscatedCharMap['\t'] = '7';

   ObfuscatedToPlainCharMap['1'] = '\\';
   ObfuscatedToPlainCharMap['2'] = '\'';
   ObfuscatedToPlainCharMap['3'] = '\"';
   ObfuscatedToPlainCharMap['4'] = ' ';
   ObfuscatedToPlainCharMap['5'] = '\r';
   ObfuscatedToPlainCharMap['6'] = '\n';
   ObfuscatedToPlainCharMap['7'] = '\t';

   initializedTable = TRUE;
} // VixMsgInitializeObfuscationMapping


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg_ObfuscateNamePassword --
 *
 *       This is NOT ENCRYPTION.
 *
 *       This function does 2 things:
 *          * It removes spaces, quotes and other characters that may make 
 *             parsing params in a string difficult. The name and password is
 *             passed from the VMX to the tools through the backdoor as a 
 *             string containing quoted parameters.
 *
 *          * It means that somebody doing a trivial string search on 
 *             host memory won't see a name/password.
 *
 *          This is used ONLY between the VMX and guest through the backdoor.
 *          This is NOT secure.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
VixMsg_ObfuscateNamePassword(const char *userName,      // IN
                             const char *password)      // IN
{
   char *packedBuffer = NULL;
   char *resultString = NULL;
   char *destPtr;
   size_t packedBufferLength = 0;
   size_t nameLength = 0;
   size_t passwordLength = 0;
   
   if (NULL != userName) {
      nameLength = strlen(userName);
   }
   if (NULL != password) {
      passwordLength = strlen(password);
   }
   /*
    * Leave space for null terminating characters.
    */
   packedBufferLength = nameLength + 1 + passwordLength + 1;
   packedBuffer = Util_SafeMalloc(packedBufferLength);
   destPtr = packedBuffer;
   if (NULL != userName) {
      Str_Strcpy(destPtr, userName, nameLength + 1);
      destPtr += nameLength;
   }
   *(destPtr++) = 0;
   if (NULL != password) {
      Str_Strcpy(destPtr, password, passwordLength + 1);
      destPtr += passwordLength;
   }
   *(destPtr++) = 0;

   resultString = VixMsgEncodeBuffer(packedBuffer, packedBufferLength, FALSE);

   Util_ZeroFree(packedBuffer, packedBufferLength);

   return(resultString);
} // VixMsg_ObfuscateNamePassword


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg_DeObfuscateNamePassword --
 *
 *      This reverses VixMsg_ObfuscateNamePassword. 
 *      See the notes for that procedure.
 *
 * Results:
 *      Bool. TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VixMsg_DeObfuscateNamePassword(const char *packagedName,   // IN
                               char **userNameResult,      // OUT
                               char **passwordResult)      // OUT
{
   Bool success = FALSE;
   char *packedString = NULL;
   char *srcPtr;
   size_t packedStringLength;

   packedString = VixMsgDecodeBuffer(packagedName, FALSE, &packedStringLength);
   if (NULL == packedString) {
      goto abort;
   }

   srcPtr = packedString;
   if (NULL != userNameResult) {
      *userNameResult = Util_SafeStrdup(srcPtr);
   }
   srcPtr = srcPtr + strlen(srcPtr);
   srcPtr++;
   if (NULL != passwordResult) {
      *passwordResult = Util_SafeStrdup(srcPtr);
   }
   success = TRUE;

abort:
   Util_ZeroFree(packedString, packedStringLength);

   return(success);
} // VixMsg_DeObfuscateNamePassword


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg_EncodeString --
 *
 *       This makes a string safe to pass over a backdoor Tclo command as a 
 *       string. It base64 encodes a string, which removes quote, space,
 *       backslash, and other characters. This will also allow us to pass
 *       UTF-8 strings.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
VixMsg_EncodeString(const char *str)  // IN
{
   if (NULL == str) {
      str = "";
   }

   return VixMsgEncodeBuffer(str, strlen(str), TRUE);
} // VixMsg_EncodeString


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsgEncodeBuffer --
 *
 *       This makes a string safe to pass over a backdoor Tclo command as a 
 *       string. It base64 encodes a string, which removes quote, space,
 *       backslash, and other characters. This will also allow us to pass
 *       UTF-8 strings.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
VixMsgEncodeBuffer(const uint8 *buffer,     // IN
                   size_t bufferLength,     // IN
                   Bool includeEncodingId)  // IN: Add 'a' (ASCII) at start of output
{
   char *base64String = NULL;
   char *resultString = NULL;
   size_t resultBufferLength = 0;
   char *srcPtr;
   char *endSrcPtr;
   char *destPtr;
   size_t base64Length;
   
   base64Length = Base64_EncodedLength((uint8 const *) buffer,
                                       bufferLength);
   base64String = Util_SafeMalloc(base64Length);
   if (!(Base64_Encode((uint8 const *) buffer,
                       bufferLength,
                       base64String, 
                       base64Length,
                       &base64Length))) {
      goto abort;
   }

   VixMsgInitializeObfuscationMapping();

   /*
    * Expand it to make space for escaping some characters.
    */
   resultBufferLength = base64Length * 2;
   if (includeEncodingId) {
      resultBufferLength++;
   }

   resultString = Util_SafeMalloc(resultBufferLength + 1);
   destPtr = resultString;
   srcPtr = base64String;
   endSrcPtr = base64String + base64Length;

   if (includeEncodingId) {
      /*
       * Start with the character-set type. 
       *   'a' means ASCII.
       */
      *(destPtr++) = 'a';
   }

   /*
    * Now, escape problematic characters.
    */
   while (srcPtr < endSrcPtr)
   {
      if (PlainToObfuscatedCharMap[(unsigned int) (*srcPtr)]) {
         *(destPtr++) = '\\';
         *(destPtr++) = PlainToObfuscatedCharMap[(unsigned int) (*srcPtr)];
      } else {
         *(destPtr++) = *srcPtr;
      }

      srcPtr++;
   }

   ASSERT_NOT_IMPLEMENTED((destPtr - resultString) <= resultBufferLength);
   *destPtr = 0;

abort:
   free(base64String);

   return resultString;
} // VixMsgEncodeBuffer

/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg_DecodeString --
 *
 *       This reverses VixMsg_EncodeString. 
 *       See the notes for that procedure.
 *
 * Results:
 *      A pointer to the decoded string, or NULL on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
VixMsg_DecodeString(const char *str)   // IN
{
   /*
    * Check the character set. 
    *   'a' means ASCII.
    */
   if ((NULL == str) || ('a' != *str)) {
      return(NULL);
   }

   return VixMsgDecodeBuffer(str + 1, TRUE, NULL);
} // VixMsg_DecodeString


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsgDecodeBuffer --
 *
 *      This reverses VixMsgEncodeBuffer. 
 *      See the notes for that procedure.
 *
 * Results:
 *      A pointer to the decoded string, or NULL on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
VixMsgDecodeBuffer(const char *str,           // IN
                   Bool nullTerminateResult,  // OUT
                   size_t *bufferLength)      // OUT: Optional
{
   char *base64String = NULL;
   char *resultStr = NULL;
   char *srcPtr;
   char *destPtr;
   size_t resultStrAllocatedLength;
   size_t resultStrLogicalLength;

   if (NULL != bufferLength) {
      *bufferLength = 0;
   }

   /*
    * Remove escaped special characters.
    * Do this in a private copy because we will change the string in place.
    */
   VixMsgInitializeObfuscationMapping();
   base64String = Util_SafeStrdup(str);
   destPtr = base64String;
   srcPtr = base64String;

   while (*srcPtr) {
      if ('\\' == *srcPtr) {
         srcPtr++;
         /*
          * There should never be a null byte as part of an escape character or
          * an escape character than translates into a null byte.
          */
         if ((0 == *srcPtr)
                || (0 == ObfuscatedToPlainCharMap[(unsigned int) (*srcPtr)])) {
            goto abort;
         }
         *(destPtr++) = ObfuscatedToPlainCharMap[(unsigned int) (*srcPtr)];
      } else {
         *(destPtr++) = *srcPtr;
      }
      srcPtr++;
   }
   *destPtr = 0;

   /*
    * Add 1 to the Base64_DecodedLength(), since we base64 encoded the string
    * without the NUL terminator and need to add one.
    */
   resultStrAllocatedLength = Base64_DecodedLength(base64String,
                                                   destPtr - base64String);
   if (nullTerminateResult) {
      resultStrAllocatedLength += 1;
   }

   resultStr = Util_SafeMalloc(resultStrAllocatedLength);
   if (!Base64_Decode(base64String,
                      resultStr,
                      resultStrAllocatedLength,
                      &resultStrLogicalLength)
          || (resultStrLogicalLength > resultStrAllocatedLength)) {
      free(resultStr);
      resultStr = NULL;
      goto abort;
   }

   if (nullTerminateResult) {
      ASSERT_NOT_IMPLEMENTED(resultStrLogicalLength < resultStrAllocatedLength);
      resultStr[resultStrLogicalLength] = 0;
   }

   if (NULL != bufferLength) {
      *bufferLength = resultStrLogicalLength;
   }

abort:
   free(base64String);

   return(resultStr);
} // VixMsgDecodeBuffer


/*
 *-----------------------------------------------------------------------------
 *
 * VixAsyncOp_ValidateCommandInfoTable --
 *
 *      Checkes that the command info table is generally well-formed.
 *      Makes sure that the table is big enough to contain all the
 *      command op codes and that they are present in the right order.
 *
 * Results:
 *      Bool
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
VixMsg_ValidateCommandInfoTable(void)
{
   int i;

   /*
    * Check at compile time that there is as many entries in the
    * command info table as there are commands. We need the +1 since
    * VIX_COMMAND_UNKNOWN is in the table and its opcode is -1.
    *
    * If this has failed for you, you've probably added a new command to VIX
    * without adding it to the command info table above.
    */
   ASSERT_ON_COMPILE(ARRAYSIZE(vixCommandInfoTable) 
                        == (VIX_COMMAND_LAST_NORMAL_COMMAND + 1));

   /*
    * Iterated over all the elements in the command info table to make
    * sure that op code matches the index (they are shifted by one because
    * of VIX_COMMAND_UNKNOWN) and that every used entry has a non-NULL name.
    */
   for (i = 0; i < ARRAYSIZE(vixCommandInfoTable); i++) {
      if (vixCommandInfoTable[i].used &&
          ((vixCommandInfoTable[i].opCode != (i - 1)) ||
           (NULL == vixCommandInfoTable[i].commandName))) {
         Warning("%s: Mismatch or NULL in command with op code %d at "
                 "index %d.\n",
                 __FUNCTION__, vixCommandInfoTable[i].opCode, i);
         return FALSE;
      }
   }

   return TRUE;
} // VixMsg_ValidateCommandInfoTable


/*
 *-----------------------------------------------------------------------------
 *
 * VixAsyncOp_GetDebugStrForOpCode --
 *
 *      Get a human readable string representing the given op code, or
 *      "Unrecognized op" if the op code is invalid.
 *
 * Results:
 *      const char *
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

const char *
VixAsyncOp_GetDebugStrForOpCode(int opCode)  // IN
{
   const char *opName = "Unrecognized op";
   const VixCommandInfo *commandInfo;

   commandInfo = VixGetCommandInfoForOpCode(opCode);
   if (NULL != commandInfo) {
      opName = commandInfo->commandName;
      ASSERT(NULL != opName);
   }

   return opName;
} // VixAsyncOp_GetDebugStrForOpCode


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg_GetCommandSecurityCategory --
 *
 *      Get the security category asociated with the given op code.
 *
 * Results:
 *      VixCommandSecurityCategory: the security category for the op code,
 *      or VIX_COMMAND_CATEGORY_UNKNOWN is the op code is invalid.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VixCommandSecurityCategory
VixMsg_GetCommandSecurityCategory(int opCode)  // IN
{
   VixCommandSecurityCategory category = VIX_COMMAND_CATEGORY_UNKNOWN;
   const VixCommandInfo *commandInfo;

   commandInfo = VixGetCommandInfoForOpCode(opCode);
   if (NULL != commandInfo) {
      category = commandInfo->category;
   }

   return category;
} // VixMsg_GetCommandSecurityCategory


/*
 *-----------------------------------------------------------------------------
 *
 * VixGetCommandInfoForOpCode --
 *
 *      Looks up the information for an opcode from the global op code table.
 *
 * Results:
 *      A const pointer to the command info struct for the opCode, or NULL
 *      if the op code is invalid.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static const VixCommandInfo *
VixGetCommandInfoForOpCode(int opCode)  // IN
{
   const VixCommandInfo *commandInfo = NULL;

   if ((opCode >= VIX_COMMAND_UNKNOWN) &&
       (opCode < VIX_COMMAND_LAST_NORMAL_COMMAND)) {
      /* Add 1 to the op code, since VIX_COMMAND_UNKNOWN is -1 */
      if (vixCommandInfoTable[opCode + 1].used) {
         commandInfo = &vixCommandInfoTable[opCode + 1];
      }
   }

   return commandInfo;
} // VixGetCommandInfoForOpCode
