/*********************************************************
 * Copyright (C) 2017-2019 VMware, Inc. All rights reserved.
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
 * @file syncManifest.h
 *
 * Interface definitions for sync driver manifest file generation.
 * On Linux, generate an xml-format manifest file when quiescing
 * is done via the sync driver backend that uses "FIFREEZE" and
 * "FITHAW" ioctls.  On other OSes, or on Linux with other sync
 * driver backends, no manifest is generated for the sync driver.
 */

#ifndef _SYNCMANIFEST_H_
#define _SYNCMANIFEST_H_

#if defined(__linux__)

typedef struct {
   char *path;
   char *providerName;
} SyncManifest;

SyncManifest *
SyncNewManifest(VmBackupState *state, SyncDriverHandle handle);

Bool
SyncManifestSend(SyncManifest *manifest);

void
SyncManifestRelease(SyncManifest *manifest);

#else /* !defined(__linux__) */

typedef void SyncManifest;

#define SyncNewManifest(s, h)            (NULL)
#define SyncManifestSend(m)              (TRUE)
#define SyncManifestRelease(m)           ASSERT(m == NULL)

#endif /* defined(__linux__) */

#endif /* _SYNCMANIFEST_H_*/
