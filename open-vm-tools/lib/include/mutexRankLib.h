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

#ifndef _LIBMUTEXRANK_H
#define _LIBMUTEXRANK_H

#include "mutexRank.h"

/*
 * MXUser mutex ranks for bora/lib code.
 *
 * bora/lib lock rank space is from RANK_libLockBase on up to
 * RANK_LEAF (see vm_basic_defs).asdf
 *
 * (Keep all of the below offsets in hex).
 */

/*
 * hostDeviceInfo HAL lock
 *
 * Must be > vmhs locks since this is held around the RANK_vmhsHDILock
 * callback lock which vmhs passes into that library.
 */
#define RANK_hdiHALLock             (RANK_libLockBase + 0x1005)

/*
 * vmhs locks
 */
#define RANK_vmhsHDILock            (RANK_libLockBase + 0x3002)
#define RANK_vmhsThrMxLock          (RANK_libLockBase + 0x3005)
#define RANK_vmhsVmxMxLock          (RANK_libLockBase + 0x3005)
#define RANK_vmhsMvmtsMxLock        (RANK_libLockBase + 0x3005)
#define RANK_vmhsHotfixesMxLock     (RANK_libLockBase + 0x3005)

/*
 * hgfs locks
 */
#define RANK_hgfsSharedFolders       (RANK_libLockBase + 0x4003)
#define RANK_hgfsNotifyLock          (RANK_libLockBase + 0x4005)
#define RANK_hgfsFileIOLock          (RANK_libLockBase + 0x4010)
#define RANK_hgfsSearchArrayLock     (RANK_libLockBase + 0x4020)
#define RANK_hgfsNodeArrayLock       (RANK_libLockBase + 0x4030)

/*
 * SLPv2 global lock
 */
#define RANK_slpv2GlobalLock         (RANK_libLockBase + 0x4305)

/*
 * NFC lib lock
 */
#define RANK_nfcLibLock              (RANK_libLockBase + 0x4505)

/*
 * policy ops pending list lock
 */
#define RANK_popPendingListLock      (RANK_libLockBase + 0x4605)

/*
 * disklib and I/O related locks
 */
#define RANK_diskLibWrapperLock      (RANK_libLockBase + 0x5005)
#define RANK_diskLibPluginLock       (RANK_libLockBase + 0x5010)
#define RANK_vmioPluginRootLock      (RANK_libLockBase + 0x5020)
#define RANK_vmioPluginEvtLock       (RANK_libLockBase + 0x5030)
#define RANK_vmioPluginSysLock       (RANK_libLockBase + 0x5040)
#define RANK_fsCmdLock               (RANK_libLockBase + 0x5050)

/*
 * RANK_vigorClientLock < VMDB range
 */
#define RANK_vigorClientLock         (RANK_libLockBase + 0x5400)
#define RANK_vigorOfflineClientLock  (RANK_libLockBase + 0x5410)

/*
 * VMDB range:
 * (RANK_libLockBase + 0x5500, RANK_libLockBase + 0x5600)
 */
#define RANK_vmuSecPolicyLock        (RANK_libLockBase + 0x5505)
#define RANK_vmdbCnxRpcLock          (RANK_libLockBase + 0x5510)
#define RANK_vmdbCnxRpcBarrierLock   (RANK_libLockBase + 0x5520)
#define RANK_vmdbCnxLock             (RANK_libLockBase + 0x5530)
#define RANK_vmdbSecureLock          (RANK_libLockBase + 0x5540)
#define RANK_vmdbDbLock              (RANK_libLockBase + 0x5550)
#define RANK_vmdbW32HookLock         (RANK_libLockBase + 0x5560)
#define RANK_vmdbWQPoolLock          (RANK_libLockBase + 0x5570)
#define RANK_vmdbMemMapLock          (RANK_libLockBase + 0x5580)

/*
 * misc locks
 *
 * Assuming ordering is important here for the listed locks. Other
 * non-leaf locks are usually defined with RANK_LEAF - 1.
 *
 * At least:
 *    impersonate > pollDefault
 *    keyLocator > preference (for checking AESNI)
 *    configDb > keyLocator (for unlocking dictionaries)
 *    battery/button > preference
 *    workerLib > something for sure under VThread_Create
 *    licenseCheck > preference
 */

#define RANK_sslStateLock            (RANK_libLockBase + 0x7010)
#define RANK_getSafeTmpDirLock       (RANK_libLockBase + 0x7020)
#define RANK_batteryLock             (RANK_libLockBase + 0x7030)
#define RANK_buttonLock              (RANK_libLockBase + 0x7040)
#define RANK_impersonateLock         (RANK_libLockBase + 0x7045)
#define RANK_pollDefaultLock         (RANK_libLockBase + 0x7050)
#define RANK_workerLibLock           (RANK_libLockBase + 0x7060)
#define RANK_configDbLock            (RANK_libLockBase + 0x7070)
#define RANK_keyLocatorLock          (RANK_libLockBase + 0x7080)
#define RANK_licenseCheckLock        (RANK_libLockBase + 0x7090)
#define RANK_preferenceLock          (RANK_libLockBase + 0x7100)

#endif /* _LIBMUTEXRANK_H */
