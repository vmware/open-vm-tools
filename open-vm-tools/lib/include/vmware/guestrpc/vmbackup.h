/*********************************************************
 * Copyright (C) 2007-2017,2020 VMware, Inc. All rights reserved.
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
 * How often (in milliseconds) does the guest send keep alive messages
 * to the host during long operations.
 */
#define VMBACKUP_KEEP_ALIVE_PERIOD        5000

/* These are RPC messages used between the VMX and the Tools. */
#define VMBACKUP_PROTOCOL_PREFIX          "vmbackup."
#define VMBACKUP_PROTOCOL_START           VMBACKUP_PROTOCOL_PREFIX"start"
#define VMBACKUP_PROTOCOL_START_WITH_OPTS VMBACKUP_PROTOCOL_PREFIX"startWithOpts"
#define VMBACKUP_PROTOCOL_ABORT           VMBACKUP_PROTOCOL_PREFIX"abort"
#define VMBACKUP_PROTOCOL_SNAPSHOT_DONE   VMBACKUP_PROTOCOL_PREFIX"snapshotDone"
#define VMBACKUP_PROTOCOL_EVENT_SET       VMBACKUP_PROTOCOL_PREFIX"eventSet"
#define VMBACKUP_PROTOCOL_SNAPSHOT_COMPLETED \
   VMBACKUP_PROTOCOL_PREFIX"snapshotCompleted"

/* These are responses to messages sent to the guest. */
#define VMBACKUP_PROTOCOL_ERROR           "protocol.error"
#define VMBACKUP_PROTOCOL_RESPONSE        "protocol.response"

/* These are events sent from the guest. */
#define VMBACKUP_EVENT_RESET              "reset"
#define VMBACKUP_EVENT_REQUESTOR_ABORT    "req.aborted"
#define VMBACKUP_EVENT_REQUESTOR_DONE     "req.done"
#define VMBACKUP_EVENT_REQUESTOR_ERROR    "req.error"
#define VMBACKUP_EVENT_REQUESTOR_MANIFEST "req.manifest"
#define VMBACKUP_EVENT_GENERIC_MANIFEST   "req.genericManifest"
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

