/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 * @file nullProvider.c
 *
 * Implements a sync provider that doesn't really do anything, so that we can at
 * least run freeze / thaw scripts if no lower-level freeze functionality is
 * available.
 */

#if !defined(_WIN32)
#  include <unistd.h>
#endif

#include "vmBackupInt.h"


/*
 ******************************************************************************
 * VmBackupNullStart --                                                 */ /**
 *
 * Calls sync(2) on POSIX systems. Sends the "commit snapshot" event to the
 * host.
 *
 * @param[in] state         Backup state.
 * @param[in] clientData    Unused.
 *
 * @return Whether successfully sent the signal to the host.
 *
 ******************************************************************************
 */

static Bool
VmBackupNullStart(VmBackupState *state,
                  void *clientData)
{
#if !defined(_WIN32)
   /*
    * This is more of a "let's at least do something" than something that
    * will actually ensure data integrity...
    */
   sync();
#endif
   VmBackup_SetCurrentOp(state, NULL, NULL, __FUNCTION__);
   return VmBackup_SendEvent(VMBACKUP_EVENT_SNAPSHOT_COMMIT, 0, "");
}


/*
 ******************************************************************************
 * VmBackupNullSnapshotDone --                                          */ /**
 *
 * Does nothing, just keep the backup state machine alive.
 *
 * @param[in] state         Backup state.
 * @param[in] clientData    Unused.
 *
 * @return TRUE.
 *
 ******************************************************************************
 */

static Bool
VmBackupNullSnapshotDone(VmBackupState *state,
                         void *clientData)
{
   VmBackup_SetCurrentOp(state, NULL, NULL, __FUNCTION__);
   return TRUE;
}


/*
 ******************************************************************************
 * VmBackupNullRelease --                                               */ /**
 *
 * Frees memory associated with this sync provider.
 *
 * @param[in] provider     The provider.
 *
 ******************************************************************************
 */

static void
VmBackupNullRelease(VmBackupSyncProvider *provider)
{
   g_free(provider);
}


/*
 ******************************************************************************
 * VmBackup_NewNullProvider --                                          */ /**
 *
 * Returns a new null provider.
 *
 * @return A VmBackupSyncProvider, never NULL.
 *
 ******************************************************************************
 */

VmBackupSyncProvider *
VmBackup_NewNullProvider(void)
{
   VmBackupSyncProvider *provider;

   provider = g_new(VmBackupSyncProvider, 1);
   provider->start = VmBackupNullStart;
   provider->snapshotDone = VmBackupNullSnapshotDone;
   provider->release = VmBackupNullRelease;
   provider->clientData = NULL;

   return provider;
}

