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
 * These are definitions shared by the different components that implement
 * the different endpoints of the vmbackup functionality (VIM, VMX, Tools).
 */

#ifndef _VMBACKUP_DEF_H_
#define _VMBACKUP_DEF_H_

/*
 * Limit messages to a max of 1024 bytes to avoid messages of arbitrary
 * sizes being set in VMDB.
 */
#define VMBACKUP_MAX_MSG_SIZE             1024

/*
 * Maximum number of events kept in memory in VMDB and hostd.
 */
#define VMBACKUP_MAX_EVENTS               10

/*
 * How often (in milliseconds) does the guest send keep alive messages
 * to the host during long operations.
 */
#define VMBACKUP_KEEP_ALIVE_PERIOD        5000

/* VMDB paths used for communication with the VMX/guest OS. */
#define VMBACKUP_VMDB_PATH                "vmx/guestTools/backupProtocol/"
#define VMBACKUP_VMDB_EVENT_PATH          VMBACKUP_VMDB_PATH"event"
#define VMBACKUP_VMDB_REQUEST_PATH        VMBACKUP_VMDB_PATH"request"
#define VMBACKUP_VMDB_RESPONSE_PATH       VMBACKUP_VMDB_PATH"response"

/* These are RPC messages used between the VMX and the Tools. */
#define VMBACKUP_PROTOCOL_PREFIX          "vmbackup."
#define VMBACKUP_PROTOCOL_START           VMBACKUP_PROTOCOL_PREFIX"start"
#define VMBACKUP_PROTOCOL_START_WITH_OPTS VMBACKUP_PROTOCOL_PREFIX"startWithOpts"
#define VMBACKUP_PROTOCOL_ABORT           VMBACKUP_PROTOCOL_PREFIX"abort"
#define VMBACKUP_PROTOCOL_SNAPSHOT_DONE   VMBACKUP_PROTOCOL_PREFIX"snapshotDone"
#define VMBACKUP_PROTOCOL_EVENT_SET       VMBACKUP_PROTOCOL_PREFIX"eventSet"

/* These are responses to messages sent to the guest. */
#define VMBACKUP_PROTOCOL_ERROR           "protocol.error"
#define VMBACKUP_PROTOCOL_RESPONSE        "protocol.response"

/* These are events sent from the guest. */
#define VMBACKUP_EVENT_RESET              "reset"
#define VMBACKUP_EVENT_REQUESTOR_ABORT    "req.aborted"
#define VMBACKUP_EVENT_REQUESTOR_DONE     "req.done"
#define VMBACKUP_EVENT_REQUESTOR_ERROR    "req.error"
#define VMBACKUP_EVENT_REQUESTOR_MANIFEST "req.manifest"
#define VMBACKUP_EVENT_SNAPSHOT_ABORT     "prov.snapshotAbort"
#define VMBACKUP_EVENT_SNAPSHOT_COMMIT    "prov.snapshotCommit"
#define VMBACKUP_EVENT_SNAPSHOT_PREPARE   "prov.snapshotPrepare"
#define VMBACKUP_EVENT_WRITER_ERROR       "req.writerError"
#define VMBACKUP_EVENT_KEEP_ALIVE         "req.keepAlive"

/* These are the event codes sent with the events */
typedef enum {
   VMBACKUP_SUCCESS = 0,
   VMBACKUP_INVALID_STATE,
   VMBACKUP_SCRIPT_ERROR,
   VMBACKUP_SYNC_ERROR,
   VMBACKUP_REMOTE_ABORT,
   VMBACKUP_UNEXPECTED_ERROR
} VmBackupStatus;

#endif

