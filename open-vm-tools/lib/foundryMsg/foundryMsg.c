/*********************************************************
 * Copyright (c) 2004-2016, 2019, 2021, 2023 VMware, Inc. All rights reserved.
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
#include "util.h"
#include "str.h"
#include "base64.h"

#include "vixOpenSource.h"
#include "vixCommands.h"
#include "unicodeBase.h"

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
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
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
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_RELOAD_VM,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_DELETE_VM,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
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
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   /* GET_HANDLE_STATE is needed for the initial handshake */
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_HANDLE_STATE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_UNUSED_COMMAND,
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
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
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
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_LOGOUT_IN_GUEST,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_READ_VARIABLE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_WRITE_VARIABLE,
                           VIX_COMMAND_CATEGORY_MIXED),
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
   VIX_DEFINE_UNUSED_COMMAND,

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_ANSWER_MESSAGE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_ENABLE_SHARED_FOLDERS,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_MOUNT_HGFS_FOLDERS,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_HOT_EXTEND_DISK,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_UNUSED_COMMAND,

   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CONNECT_HOST,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CREATE_LINKED_CLONE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
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
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_VM_PAUSE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_VM_UNPAUSE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_PERFORMANCE_DATA,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_SNAPSHOT_SCREENSHOT,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_WAIT_FOR_USER_ACTION_IN_GUEST,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
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
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   /* GET_VMX_DEVICE_STATE is needed for the initial handshake. */
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GET_VMX_DEVICE_STATE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
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

   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,

   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,
   VIX_DEFINE_UNUSED_COMMAND,

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_LIST_FILESYSTEMS,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CHANGE_DISPLAY_TOPOLOGY,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SUSPEND_AND_RESUME,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_REMOVE_BULK_SNAPSHOT,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_COPY_FILE_FROM_READER_TO_GUEST,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_GENERATE_NONCE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CHANGE_DISPLAY_TOPOLOGY_MODES,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_QUERY_CHILDREN,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_LIST_FILES,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CREATE_DIRECTORY_EX,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_MOVE_GUEST_FILE_EX,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_MOVE_GUEST_DIRECTORY,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CREATE_TEMPORARY_FILE_EX,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CREATE_TEMPORARY_DIRECTORY,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SET_GUEST_FILE_ATTRIBUTES,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_COPY_FILE_FROM_GUEST_TO_READER,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_START_PROGRAM,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_LIST_PROCESSES_EX,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_READ_ENV_VARIABLES,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_INITIATE_FILE_TRANSFER_FROM_GUEST,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_INITIATE_FILE_TRANSFER_TO_GUEST,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_ACQUIRE_CREDENTIALS,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_RELEASE_CREDENTIALS,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_VALIDATE_CREDENTIALS,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_TERMINATE_PROCESS,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_DELETE_GUEST_FILE_EX,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_DELETE_GUEST_DIRECTORY_EX,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_HOT_CHANGE_MONITOR_TYPE,
                           VIX_COMMAND_CATEGORY_PRIVILEGED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_ADD_AUTH_ALIAS,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_REMOVE_AUTH_ALIAS,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_LIST_AUTH_PROVIDER_ALIASES,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_LIST_AUTH_MAPPED_ALIASES,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_CREATE_REGISTRY_KEY,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_LIST_REGISTRY_KEYS,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_DELETE_REGISTRY_KEY,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_SET_REGISTRY_VALUE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_LIST_REGISTRY_VALUES,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_DELETE_REGISTRY_VALUE,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),

   VIX_DEFINE_COMMAND_INFO(VIX_COMMAND_REMOVE_AUTH_ALIAS_BY_CERT,
                           VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED),
};


static const VixCommandInfo *VixGetCommandInfoForOpCode(int opCode);

static void VixMsgInitializeObfuscationMapping(void);

static VixError VixMsgEncodeBuffer(const uint8 *buffer,
                                   size_t bufferLength,
                                   Bool includeEncodingId,
                                   char **result);

static VixError VixMsgDecodeBuffer(const char *str,
                                   Bool nullTerminateResult,
                                   char **result,
                                   size_t *bufferLength);

static VixError VMAutomationMsgParserInit(const char *caller,
                                          unsigned int line,
                                          VMAutomationMsgParser *state,
                                          const VixMsgHeader *msg,
                                          size_t headerLength,
                                          size_t fixedLength,
                                          size_t miscDataLength,
                                          const char *packetType);


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
                       int opCode,                       // IN
                       uint64 cookie,                    // IN
                       int credentialType,               // IN
                       const char *credential)           // IN
{
   size_t totalMessageSize;
   VixCommandRequestHeader *commandRequest = NULL;
   size_t providedCredentialLength = 0;
   size_t totalCredentialLength = 0;

   if ((VIX_USER_CREDENTIAL_NAME_PASSWORD == credentialType)
      || (VIX_USER_CREDENTIAL_HOST_CONFIG_SECRET == credentialType)
      || (VIX_USER_CREDENTIAL_HOST_CONFIG_HASHED_SECRET == credentialType)
      || (VIX_USER_CREDENTIAL_TICKETED_SESSION == credentialType)
      || (VIX_USER_CREDENTIAL_SSPI == credentialType)
      || (VIX_USER_CREDENTIAL_SAML_BEARER_TOKEN == credentialType)
      || (VIX_USER_CREDENTIAL_SAML_BEARER_TOKEN_HOST_VERIFIED == credentialType)) {
      /*
       * All of these are optional.
       */
      if (NULL != credential) {
         providedCredentialLength = strlen(credential);
         totalCredentialLength += providedCredentialLength;
      }
      /*
       * Add 1 to each string to include '\0' for the end of the string.
       */
      totalCredentialLength += 1;
   } else {
      totalCredentialLength = 0;
   }

   totalMessageSize = msgHeaderAndBodyLength + totalCredentialLength;
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
      msgHeaderAndBodyLength + totalCredentialLength;
   commandRequest->commonHeader.headerLength = sizeof(VixCommandRequestHeader);
   commandRequest->commonHeader.bodyLength = msgHeaderAndBodyLength -
      sizeof(VixCommandRequestHeader);
   commandRequest->commonHeader.credentialLength = totalCredentialLength;
   commandRequest->commonHeader.commonFlags = VIX_COMMAND_REQUEST;

   commandRequest->opCode = opCode;
   commandRequest->cookie = cookie;
   commandRequest->timeOut = 0xFFFFFFFF;
   commandRequest->requestFlags = 0;

   commandRequest->userCredentialType = credentialType;

   if ((VIX_USER_CREDENTIAL_NAME_PASSWORD == credentialType)
         || (VIX_USER_CREDENTIAL_HOST_CONFIG_SECRET == credentialType)
         || (VIX_USER_CREDENTIAL_HOST_CONFIG_HASHED_SECRET == credentialType)
         || (VIX_USER_CREDENTIAL_TICKETED_SESSION == credentialType)
         || (VIX_USER_CREDENTIAL_SSPI == credentialType)
         || (VIX_USER_CREDENTIAL_SAML_BEARER_TOKEN == credentialType)
         || (VIX_USER_CREDENTIAL_SAML_BEARER_TOKEN_HOST_VERIFIED == credentialType)) {
      char *destPtr = (char *) commandRequest;

      destPtr += commandRequest->commonHeader.headerLength;
      destPtr += commandRequest->commonHeader.bodyLength;
      if (NULL != credential) {
         Str_Strcpy(destPtr, credential, providedCredentialLength + 1);
         destPtr += providedCredentialLength;
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
    * Confidence check the header.
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
    * Confidence check the parts of the header that are specific to requests.
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

   if ((VIX_REQUESTMSG_INCLUDES_AUTH_DATA_V1 & message->requestFlags) &&
       (message->commonHeader.totalMessageLength <
          (uint64)message->commonHeader.headerLength +
          message->commonHeader.bodyLength +
          message->commonHeader.credentialLength +
          sizeof (VixMsgAuthDataV1))) {
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
    * Confidence check the parts of the header that are specific to responses.
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
      goto quit;
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
      goto quit;
   }

   if (msg->header.commonHeader.totalMessageLength < sizeof *msg) {
      err = VIX_E_INVALID_MESSAGE_BODY;
      goto quit;
   }

   headerAndBodyLength = (uint64) msg->header.commonHeader.headerLength
                            + msg->header.commonHeader.bodyLength;

   if (headerAndBodyLength < ((uint64) sizeof *msg
                                 + msg->nameLength + 1
                                 + msg->valueLength + 1)) {
      err = VIX_E_INVALID_MESSAGE_BODY;
      goto quit;
   }

   valueNameLocal = ((char *) msg) + sizeof(*msg);
   if ('\0' != valueNameLocal[msg->nameLength]) {
      err = VIX_E_INVALID_MESSAGE_BODY;
      goto quit;
   }

   valueLocal = valueNameLocal + msg->nameLength + 1;
   if ('\0' != valueLocal[msg->valueLength]) {
      err = VIX_E_INVALID_MESSAGE_BODY;
      goto quit;
   }

   *valueName = valueNameLocal;
   *value = valueLocal;
   err = VIX_OK;

quit:

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

VixError
VixMsg_ObfuscateNamePassword(const char *userName,      // IN
                             const char *password,      // IN
                             char **result)             // OUT
{
   VixError err = VIX_OK;
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
   packedBuffer = VixMsg_MallocClientData(packedBufferLength);
   if (packedBuffer == NULL) {
      err = VIX_E_OUT_OF_MEMORY;
      goto quit;
   }

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

   err = VixMsgEncodeBuffer(packedBuffer, packedBufferLength, FALSE,
                            &resultString);
   if (err != VIX_OK) {
      goto quit;
   }

quit:
   Util_ZeroFree(packedBuffer, packedBufferLength);

   if (err == VIX_OK) {
      *result = resultString;
   }

   return err;
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
 *      VixError. VIX_OK if successful.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixMsg_DeObfuscateNamePassword(const char *packagedName,   // IN
                               char **userNameResult,      // OUT
                               char **passwordResult)      // OUT
{
   VixError err;
   char *packedString = NULL;
   char *srcPtr;
   size_t packedStringLength;
   char *userName = NULL;
   char *passwd = NULL;

   err = VixMsgDecodeBuffer(packagedName, FALSE,
                            &packedString, &packedStringLength);
   if (err != VIX_OK) {
      goto quit;
   }

   srcPtr = packedString;
   if (NULL != userNameResult) {
      Bool allocateFailed;
      userName = VixMsg_StrdupClientData(srcPtr, &allocateFailed);
      if (allocateFailed) {
         err = VIX_E_OUT_OF_MEMORY;
         goto quit;
      }
   }
   srcPtr = srcPtr + strlen(srcPtr);
   srcPtr++;
   if (NULL != passwordResult) {
      Bool allocateFailed;
      passwd = VixMsg_StrdupClientData(srcPtr, &allocateFailed);
      if (allocateFailed) {
         err = VIX_E_OUT_OF_MEMORY;
         goto quit;
      }
   }

   if (NULL != userNameResult) {
      *userNameResult = userName;
      userName = NULL;
   }
   if (NULL != passwordResult) {
      *passwordResult = passwd;
      passwd = NULL;
   }

quit:
   Util_ZeroFree(packedString, packedStringLength);
   Util_ZeroFreeString(userName);
   Util_ZeroFreeString(passwd);

   return err;
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

VixError
VixMsg_EncodeString(const char *str,  // IN
                    char **result)    // OUT
{
   if (NULL == str) {
      str = "";
   }

   /*
    * Coverity flags this as a buffer overrun in the case where str is
    * assigned the empty string above, claiming that the underlying
    * Base64_Encode function directly indexes the array str at index 2;
    * however, that indexing is only done if the string length is greater
    * than 2, and clearly strlen("") is 0.
    */
   /* coverity[overrun-buffer-val] */
   return VixMsgEncodeBuffer(str, strlen(str), TRUE, result);
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

VixError
VixMsgEncodeBuffer(const uint8 *buffer,     // IN
                   size_t bufferLength,     // IN
                   Bool includeEncodingId,  // IN: Add 'a' (ASCII) at start of output
                   char ** result)          // OUT
{
   VixError err = VIX_OK;
   char *base64String = NULL;
   char *resultString = NULL;
   size_t resultBufferLength = 0;
   char *srcPtr;
   char *endSrcPtr;
   char *destPtr;
   size_t base64Length;

   base64Length = Base64_EncodedLength((uint8 const *) buffer,
                                       bufferLength);
   base64String = VixMsg_MallocClientData(base64Length);
   if (base64String == NULL) {
      err = VIX_E_OUT_OF_MEMORY;
      goto quit;
   }

   if (!(Base64_Encode((uint8 const *) buffer,
                       bufferLength,
                       base64String,
                       base64Length,
                       &base64Length))) {
      err = VIX_E_FAIL;
      goto quit;
   }

   VixMsgInitializeObfuscationMapping();

   /*
    * Expand it to make space for escaping some characters.
    */
   resultBufferLength = base64Length * 2;
   if (includeEncodingId) {
      resultBufferLength++;
   }

   resultString = VixMsg_MallocClientData(resultBufferLength + 1);
   if (resultString == NULL) {
      err = VIX_E_OUT_OF_MEMORY;
      goto quit;
   }

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

   VERIFY((destPtr - resultString) <= resultBufferLength);
   *destPtr = 0;

quit:
   free(base64String);

   if (err == VIX_OK) {
      *result = resultString;
   }

   return err;
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
 *      VixError. VIX_OK if successful.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixMsg_DecodeString(const char *str,   // IN
                    char **result)     // OUT
{
   /*
    * Check the character set.
    *   'a' means ASCII.
    */
   if ((NULL == str) || ('a' != *str)) {
      *result = NULL;
      return VIX_E_INVALID_ARG;
   }

   return VixMsgDecodeBuffer(str + 1, TRUE, result, NULL);
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
 *      VixError. VIX_OK if successful.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixMsgDecodeBuffer(const char *str,           // IN
                   Bool nullTerminateResult,  // OUT
                   char **result,             // OUT
                   size_t *bufferLength)      // OUT: Optional
{
   VixError err = VIX_OK;
   char *base64String = NULL;
   char *resultStr = NULL;
   char *srcPtr;
   char *destPtr;
   size_t resultStrAllocatedLength;
   size_t resultStrLogicalLength;
   Bool allocateFailed;

   if (NULL != bufferLength) {
      *bufferLength = 0;
   }

   /*
    * Remove escaped special characters.
    * Do this in a private copy because we will change the string in place.
    */
   VixMsgInitializeObfuscationMapping();
   base64String = VixMsg_StrdupClientData(str, &allocateFailed);
   if (allocateFailed) {
      err = VIX_E_OUT_OF_MEMORY;
      goto quit;
   }
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
            goto quit;
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
      goto quit;
   }

   if (nullTerminateResult) {
      VERIFY(resultStrLogicalLength < resultStrAllocatedLength);
      resultStr[resultStrLogicalLength] = 0;
   }

   if (NULL != bufferLength) {
      *bufferLength = resultStrLogicalLength;
   }

quit:
   free(base64String);

   if (err == VIX_OK) {
      *result = resultStr;
   }

   return err;
} // VixMsgDecodeBuffer


/*
 *-----------------------------------------------------------------------------
 *
 * VixAsyncOp_ValidateCommandInfoTable --
 *
 *      Checks that the command info table is generally well-formed.
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


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg_AllocGenericRequestMsg --
 *
 *      Allocate and initialize a generic request message.
 *
 *      Assumes the caller holds the lock to 'propertyList'.
 *
 * Results:
 *      Returns VixError.
 *      Upon retrun, *request will contain either the message with the
 *      headers properly initialized or NULL.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixMsg_AllocGenericRequestMsg(int opCode,                         // IN
                              uint64 cookie,                      // IN
                              int credentialType,                 // IN
                              const char *userNamePassword,       // IN
                              int options,                        // IN
                              VixPropertyListImpl *propertyList,  // IN
                              VixCommandGenericRequest **request) // OUT
{
   VixError err;
   VixCommandGenericRequest *requestLocal = NULL;
   size_t msgHeaderAndBodyLength;
   char *serializedBufferBody = NULL;
   size_t serializedBufferLength = 0;

   if (NULL == request) {
      ASSERT(0);
      err = VIX_E_FAIL;
      goto quit;
   }

   *request = NULL;

   if (NULL != propertyList) {
      err = VixPropertyList_Serialize(propertyList,
                                      FALSE,
                                      &serializedBufferLength,
                                      &serializedBufferBody);
      if (VIX_OK != err) {
         goto quit;
      }
   }

   msgHeaderAndBodyLength = sizeof(*requestLocal) + serializedBufferLength;
   requestLocal = (VixCommandGenericRequest *)
      VixMsg_AllocRequestMsg(msgHeaderAndBodyLength,
                             opCode,
                             cookie,
                             credentialType,
                             userNamePassword);
   if (NULL == requestLocal) {
      err = VIX_E_FAIL;
      goto quit;
   }

   requestLocal->options = options;
   requestLocal->propertyListSize = serializedBufferLength;

   if (NULL != serializedBufferBody) {
      char *dst = (char *)request + sizeof(*request);
      memcpy(dst, serializedBufferBody, serializedBufferLength);
   }

   *request = requestLocal;
   err = VIX_OK;

 quit:
   free(serializedBufferBody);

   return err;
}  // VixMsg_AllocGenericRequestMsg


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg_ParseGenericRequestMsg --
 *
 *      Extract the options and property list from the request
 *      message, while validating message.
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
VixMsg_ParseGenericRequestMsg(const VixCommandGenericRequest *request,  // IN
                              int *options,                             // OUT
                              VixPropertyListImpl *propertyList)        // OUT
{
   VixError err;
   uint64 headerAndBodyLength;

   if ((NULL == request) || (NULL == options) || (NULL == propertyList)) {
      ASSERT(0);
      err = VIX_E_FAIL;
      goto quit;
   }

   *options = 0;
   VixPropertyList_Initialize(propertyList);

   /*
    * In most cases we will have already called VixMsg_ValidateResponseMsg()
    * on this request before, but call it here so that this function will
    * always be sufficient to validate the request.
    */
   err = VixMsg_ValidateRequestMsg(request,
                                   request->header.commonHeader.totalMessageLength);
   if (VIX_OK != err) {
      goto quit;
   }

   if (request->header.commonHeader.totalMessageLength < sizeof *request) {
      err = VIX_E_INVALID_MESSAGE_BODY;
      goto quit;
   }

   headerAndBodyLength = (uint64) request->header.commonHeader.headerLength
      + request->header.commonHeader.bodyLength;

   if (headerAndBodyLength < ((uint64) sizeof *request
                              + request->propertyListSize)) {
      err = VIX_E_INVALID_MESSAGE_BODY;
      goto quit;
   }

   if (request->propertyListSize > 0) {
      const char *serializedBuffer = (const char *) request + sizeof(*request);

      err = VixPropertyList_Deserialize(propertyList,
                                        serializedBuffer,
                                        request->propertyListSize,
                                        VIX_PROPERTY_LIST_BAD_ENCODING_ERROR);
      if (VIX_OK != err) {
         goto quit;
      }
   }

   *options = request->options;
   err = VIX_OK;

 quit:

   return err;
} // VixMsg_ParseGenericRequestMsg



/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg_ParseSimpleResponseWithString --
 *
 *      Takes a response packet that consists of a VixCommandResponseHeader
 *      followed by a string containing the response data, validates
 *      the packet, and then passes out a pointer to that string.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixMsg_ParseSimpleResponseWithString(const VixCommandResponseHeader *response,  // IN
                                     const char **result)                       // OUT
{
   VixError err;
   VMAutomationMsgParser parser;

   err = VMAutomationMsgParserInitResponse(&parser, response, sizeof *response);
   if (VIX_OK != err) {
      goto quit;
   }

   err = VMAutomationMsgParserGetOptionalString(&parser,
                                                response->commonHeader.bodyLength,
                                                result);

quit:
   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg_MallocClientData --
 *
 *      Allocates the memory needed to copy from a client-provided buffer.
 *
 * Results:
 *      Pointer to allocated memory
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void *
VixMsg_MallocClientData(size_t size)  // IN
{
   return malloc(size);
} // VixMsg_MallocClientData


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg_ReallocClientData --
 *
 *      Reallocates the memory needed to copy from a client-provided buffer.
 *
 * Results:
 *      Pointer to allocated memory
 *
 * Side effects:
 *      Frees memory pointed to by ptr.
 *
 *-----------------------------------------------------------------------------
 */

void *
VixMsg_ReallocClientData(void *ptr,   // IN
                         size_t size) // IN
{
   return realloc(ptr, size);
} // VixMsg_ReallocClientData


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg_StrdupClientData --
 *
 *      Allocates memory and copies client-provided string.
 *
 * Results:
 *      Pointer to allocated string
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
VixMsg_StrdupClientData(const char *s,          // IN
                        Bool *allocateFailed)   // OUT
{
   char* newString = NULL;

   ASSERT(allocateFailed);
   if (NULL == allocateFailed) {
      goto quit;
   }

   *allocateFailed = FALSE;

   if (NULL != s) {
#if defined(_WIN32)
         newString = _strdup(s);
#else
         newString = strdup(s);
#endif
      if (NULL == newString) {
         *allocateFailed = TRUE;
      }
   }

quit:
   return newString;
} // VixMsg_StrdupClientData


/*
 *-----------------------------------------------------------------------------
 *
 * __VMAutomationValidateString --
 *
 *      Verifies that string at specified address is NUL terminated within
 *      specified number of bytes, and is valid UTF-8.
 *
 * Results:
 *      VixError.  VIX_OK on success.  Some other VIX_* code if message is malformed.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static VixError
__VMAutomationValidateString(const char  *caller,              // IN
                             unsigned int line,                // IN
                             const char  *buffer,              // IN
                             size_t       available)           // IN
{
   size_t stringLength;

   /*
    * NUL terminated string needs at least one byte - NUL one.
    */
   if (available < 1) {
      Log("%s(%u): Message body too short to contain string.\n", caller, line);
      return VIX_E_INVALID_MESSAGE_BODY;
   }

   /*
    * Reject message if there is no NUL before request end.  There must
    * be one...
    */

   stringLength = Str_Strlen(buffer, available);
   if (stringLength >= available) {
      Log("%s(%u): Variable string is not NUL terminated "
          "before message end.\n", caller, line);
      return VIX_E_INVALID_MESSAGE_BODY;
   }

   /*
    * If string is shorter than expected, complain.  Maybe it is too strict,
    * but clients seems to not send malformed messages, so keep doing this.
    */

   if (stringLength + 1 != available) {
      Log("%s(%u): Retrieved fixed string \"%s\" with "
          "trailing garbage.\n", caller, line, buffer);
      return VIX_E_INVALID_MESSAGE_BODY;
   }

   /*
    * If string is not UTF-8, reject it.  We do not want to pass non-UTF-8
    * strings through vmx bowels - they could hit some ASSERT somewhere...
    */

   if (!Unicode_IsBufferValid(buffer, stringLength, STRING_ENCODING_UTF8)) {
      Log("%s(%u): Variable string is not an UTF8 string.\n", caller, line);
      return VIX_E_INVALID_UTF8_STRING;
   }

   return VIX_OK;
}


/*
 *-----------------------------------------------------------------------------
 *
 * __VMAutomationValidateStringInBuffer --
 *
 *      Verifies that string at specified address is NUL terminated within
 *      specified number of bytes, and is valid UTF-8.
 *      String does not have to occupy the entire buffer.
 *
 * Results:
 *      VixError.  VIX_OK on success.
 *      Some other VIX_* code if message is malformed.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static VixError
__VMAutomationValidateStringInBuffer(const char  *caller,              // IN
                                     unsigned int line,                // IN
                                     const char  *buffer,              // IN
                                     size_t       available,           // IN
                                     size_t      *strLen)              // IN
{
   size_t stringLength;

   /*
    * NUL terminated string needs at least one byte - NUL one.
    */
   if (available < 1) {
      Log("%s(%u): Message body too short to contain string.\n", caller, line);
      return VIX_E_INVALID_MESSAGE_BODY;
   }

   /*
    * Reject message if there is no NUL before request end.  There must
    * be one...
    */

   stringLength = Str_Strlen(buffer, available);
   *strLen = stringLength;

   if (stringLength >= available) {
      Log("%s(%u): Variable string is not NUL terminated "
          "before message end.\n", caller, line);
      return VIX_E_INVALID_MESSAGE_BODY;
   }

   /*
    * If string is not UTF-8, reject it.  We do not want to pass non-UTF-8
    * strings through vmx bowels - they could hit some ASSERT somewhere...
    */

   if (!Unicode_IsBufferValid(buffer, stringLength, STRING_ENCODING_UTF8)) {
      Log("%s(%u): Variable string is not an UTF8 string.\n", caller, line);
      return VIX_E_INVALID_UTF8_STRING;
   }

   return VIX_OK;
}


/*
 *-----------------------------------------------------------------------------
 *
 * __VMAutomationMsgParserInitRequest --
 * VMAutomationMsgParserInitRequest --
 *
 *      Initializes request parser, and performs basic message validation
 *      not performed elsewhere.
 *
 * Results:
 *      VixError.  VIX_OK on success.  Some other VIX_* code if message is malformed.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
__VMAutomationMsgParserInitRequest(const char *caller,                  // IN
                                   unsigned int line,                   // IN
                                   VMAutomationMsgParser *state,        // OUT (opt)
                                   const VixCommandRequestHeader *msg,  // IN
                                   size_t fixedLength)                  // IN
{
   size_t miscDataLength = 0;

   /*
    * If the VM is encrypted, there is additional data factored into
    * the total message size that needs to be accounted for.
    */

   if (VIX_REQUESTMSG_INCLUDES_AUTH_DATA_V1 & msg->requestFlags) {
      miscDataLength = sizeof(VixMsgAuthDataV1);
   } else {
      miscDataLength = 0;
   }

   return VMAutomationMsgParserInit(caller, line, state, &msg->commonHeader,
                                    sizeof *msg, fixedLength, miscDataLength, "request");
}


/*
 *-----------------------------------------------------------------------------
 *
 * __VMAutomationMsgParserInitResponse --
 * VMAutomationMsgParserInitResponse --
 *
 *      Initializes response parser, and performs basic message validation
 *      not performed elsewhere.
 *
 * Results:
 *      VixError.  VIX_OK on success.  Some other VIX_* code if message is malformed.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
__VMAutomationMsgParserInitResponse(const char *caller,                  // IN
                                    unsigned int line,                   // IN
                                    VMAutomationMsgParser *state,        // OUT (opt)
                                    const VixCommandResponseHeader *msg, // IN
                                    size_t fixedLength)                  // IN
{
   return VMAutomationMsgParserInit(caller, line, state, &msg->commonHeader,
                                    sizeof *msg, fixedLength, 0, "response");
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMAutomationMsgParserInit --
 *
 *      Initializes message parser, and performs basic message validation
 *      not performed elsewhere.
 *
 * Results:
 *      VixError. VIX_OK on success. Some other VIX_* code if message is malformed.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static VixError
VMAutomationMsgParserInit(const char *caller,              // IN
                          unsigned int line,               // IN
                          VMAutomationMsgParser *state,    // OUT (opt)
                          const VixMsgHeader *msg,         // IN
                          size_t headerLength,             // IN
                          size_t fixedLength,              // IN
                          size_t miscDataLength,           // IN
                          const char *packetType)          // IN
{
   uint32 headerAndBodyLength;
   // use int64 to prevent overflow
   int64 computedTotalLength = (int64)msg->headerLength +
      (int64)msg->bodyLength + (int64)msg->credentialLength +
      (int64)miscDataLength;

   int64 extBodySize = (int64)msg->headerLength + (int64)msg->bodyLength -
      (int64)fixedLength;

   if (computedTotalLength != (int64)msg->totalMessageLength) {
      Log("%s:%d, header information mismatch.\n", __FILE__, __LINE__);
      return VIX_E_INVALID_MESSAGE_HEADER;
   }

   if (extBodySize < 0) {
      Log("%s:%d, %s too short.\n", __FILE__, __LINE__, packetType);
      return VIX_E_INVALID_MESSAGE_HEADER;
   }

   /*
    * Protocol allows for headerLength expansion, but predefined structures
    * do not anticipate that even a bit. So give up if header length is
    * incompatible with our structures.
    */

   if (msg->headerLength != headerLength) {
      Log("%s(%u): %s header length %u is not supported "
          "(%"FMTSZ"u is required).\n",
          caller, line, packetType, msg->headerLength, headerLength);
      return VIX_E_INVALID_MESSAGE_HEADER;
   }

   /*
    * Message looks reasonable.  Skip over fixed part.
    */

   headerAndBodyLength = msg->headerLength + msg->bodyLength;

   if (state) {
      state->currentPtr = (const char *)msg + fixedLength;
      state->endPtr = (const char *)msg + headerAndBodyLength;
   }
   return VIX_OK;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMAutomation_VerifyRequestLength --
 *
 *      Ensures that request contains at least fixedLength bytes in
 *      header and body.
 *
 * Results:
 *      VixError.  VIX_OK on success.  Some other VIX_* code if message is malformed.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VMAutomation_VerifyRequestLength(const VixCommandRequestHeader *request, // IN
                                 size_t fixedLength)                     // IN
{
   return VMAutomationMsgParserInitRequest(NULL, request, fixedLength);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMAutomationMsgParserGetRemainingData --
 *
 *      Fetches all data remaining in the request.
 *
 * Results:
 *      Pointer to the data.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

const void *
VMAutomationMsgParserGetRemainingData(VMAutomationMsgParser *state,   // IN/OUT
                                      size_t *length)                 // OUT
{
   const void *data;

   *length = state->endPtr - state->currentPtr;
   data = state->currentPtr;
   state->currentPtr = state->endPtr;

   return data;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMAutomationMsgParserGetData --
 * __VMAutomationMsgParserGetData --
 *
 *      Fetches specified number of bytes.
 *
 * Results:
 *      VixError.  VIX_OK on success.  Some other VIX_* code if message is malformed.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
__VMAutomationMsgParserGetData(const char *caller,             // IN
                               unsigned int line,              // IN
                               VMAutomationMsgParser *state,   // IN/OUT
                               size_t length,                  // IN
                               const char **result)            // OUT (opt)
{
   size_t available;

   available = state->endPtr - state->currentPtr;

   /* If message is too short, return an error. */
   if (available < length) {
      Log("%s(%u): Message has only %"FMTSZ"u bytes available when "
          "looking for %"FMTSZ"u bytes od data.\n",
          caller, line, available, length);
      return VIX_E_INVALID_MESSAGE_BODY;
   }

   if (result) {
      *result = state->currentPtr;
   }
   state->currentPtr += length;
   return VIX_OK;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMAutomationMsgParserGetOptionalString --
 * __VMAutomationMsgParserGetOptionalString --
 *
 *      Fetches string of specified length from the request.  Length includes
 *      terminating NUL byte, which must be present.  Length of zero results
 *      in NULL being returned.
 *
 * Results:
 *      VixError.  VIX_OK on success.  Some other VIX_* code if message is malformed.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
__VMAutomationMsgParserGetOptionalString(const char *caller,           // IN
                                         unsigned int line,            // IN
                                         VMAutomationMsgParser *state, // IN/OUT
                                         size_t length,                // IN
                                         const char **result)          // OUT
{
   if (length) {
      VixError err;
      const char *string;

      err = __VMAutomationMsgParserGetData(caller, line, state, length,
                                           &string);
      if (VIX_OK != err) {
         return err;
      }
      err = __VMAutomationValidateString(caller, line, string, length);
      if (VIX_OK != err) {
         return err;
      }
      *result = string;
   } else {
      *result = NULL;
   }
   return VIX_OK;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMAutomationMsgParserGetOptionalStrings --
 * __VMAutomationMsgParserGetOptionalStrings --
 *
 *      Fetches an array of strings from the request.  Length includes the
 *      terminating NUL byte of each string.
 *
 * Results:
 *      VixError.  VIX_OK on success.
 *      Some other VIX_* code if message is malformed.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
__VMAutomationMsgParserGetOptionalStrings(const char *caller,   // IN
                                          unsigned int line,    // IN
                                VMAutomationMsgParser *state,   // IN/OUT
                                          uint32 count,         // IN
                                          size_t length,        // IN
                                          const char **result) // OUT
{
   VixError err = VIX_OK;
   const char *buffer;
   const char *theResult;
   int i;
   size_t strLen;

   if (0 == count) {
      *result = NULL;
      goto quit;
   }

   err = __VMAutomationMsgParserGetData(caller, line, state, length,
                                        &buffer);
   if (VIX_OK != err) {
      return err;
   }

   theResult = buffer;

   for (i = 0; i < count; ++i) {
      err = __VMAutomationValidateStringInBuffer(caller, line,
                                                 buffer, length, &strLen);
      if (VIX_OK != err) {
         return err;
      }
      ASSERT(strLen < length);
      buffer += (strLen + 1);
      length -= (strLen + 1);
   }

   /*
    * If string is shorter than expected, complain.  Maybe it is too strict,
    * but clients seems to not send malformed messages, so keep doing this.
    */

   if (length != 0) {
      Log("%s(%u): Retrieved an array of string with trailing garbage.\n",
          caller, line);
      return VIX_E_INVALID_MESSAGE_BODY;
   }

   *result = theResult;

quit:

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMAutomationMsgParserGetString --
 * __VMAutomationMsgParserGetString --
 *
 *      Fetches string of specified length from the request.  Length of
 *      string is specified in number of usable characters: function consumes
 *      length + 1 bytes from request, and first length bytes must be non-NUL,
 *      while length+1st byte must be NUL.
 *
 * Results:
 *      VixError.  VIX_OK on success.  Some other VIX_* code if message is malformed.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
__VMAutomationMsgParserGetString(const char *caller,            // IN
                                 unsigned int line,             // IN
                                 VMAutomationMsgParser *state,  // IN/OUT
                                 size_t length,                 // IN
                                 const char **result)           // OUT
{
   VixError err;
   const char *string;

   length++;
   if (!length) {
      Log("%s(%u): String is too long.\n", caller, line);
      return VIX_E_INVALID_ARG;
   }
   err = __VMAutomationMsgParserGetData(caller, line, state, length,
                                        &string);
   if (VIX_OK != err) {
      return err;
   }
   err = __VMAutomationValidateString(caller, line, string, length);
   if (VIX_OK != err) {
      return err;
   }

   *result = string;
   return VIX_OK;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMAutomationMsgParserGetPropertyList --
 * __VMAutomationMsgParserGetPropertyList --
 *
 *      Fetches specified number of bytes.
 *
 * Results:
 *      VixError.  VIX_OK on success.  Some other VIX_* code if message is malformed.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
__VMAutomationMsgParserGetPropertyList(const char *caller,            // IN
                                       unsigned int line,             // IN
                                       VMAutomationMsgParser *state,  // IN/OUT
                                       size_t length,                 // IN
                                       VixPropertyListImpl *propList) // IN/OUT
{
   VixError err;

   err = VIX_OK;
   if (length) {
      const char *data;

      err = __VMAutomationMsgParserGetData(caller, line, state, length,
                                           &data);
      if (VIX_OK == err) {
         err = VixPropertyList_Deserialize(propList, data, length,
                                           VIX_PROPERTY_LIST_BAD_ENCODING_ERROR);
      }
   }

   return err;
}
