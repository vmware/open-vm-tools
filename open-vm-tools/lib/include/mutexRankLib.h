/*********************************************************
 * Copyright (C) 2010-2019 VMware, Inc. All rights reserved.
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

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * MXUser mutex ranks for bora/lib code.
 *
 * The ranks define the ordering in which locks are allowed to be acquired.
 *
 * Only locks with higher rank numbers (generally more localized)
 * can be acquired while a lock with a lower rank number is active.
 *
 * bora/lib lock rank space is from RANK_libLockBase on up to
 * RANK_LEAF (see vm_basic_defs).asdf
 *
 * (Keep all of the below offsets in hex).
 */

/*
 * hostDeviceInfo HAL lock
 *
 * Must be < vmhs locks since this is held around the RANK_vmhsHDILock
 * callback lock which vmhs passes into that library.
 */
#define RANK_hdiHALLock             (RANK_libLockBase + 0x1005)

/*
 * vmhs locks (must be < vigor)
 */
#define RANK_vmhsHDILock            (RANK_libLockBase + 0x3002)
#define RANK_vmhsThrMxLock          (RANK_libLockBase + 0x3005)
#define RANK_vmhsVmxMxLock          (RANK_libLockBase + 0x3005)

/*
 * hgfs locks
 */
#define RANK_hgfsSessionArrayLock    (RANK_libLockBase + 0x4010)
#define RANK_hgfsSharedFolders       (RANK_libLockBase + 0x4030)
#define RANK_hgfsNotifyLock          (RANK_libLockBase + 0x4040)
#define RANK_hgfsFileIOLock          (RANK_libLockBase + 0x4050)
#define RANK_hgfsSearchArrayLock     (RANK_libLockBase + 0x4060)
#define RANK_hgfsNodeArrayLock       (RANK_libLockBase + 0x4070)

/*
 * vigor (must be < VMDB range and < disklib, see bug 741290)
 */
#define RANK_vigorOnlineLock         (RANK_libLockBase + 0x4400)
#define RANK_vigorOfflineLock        (RANK_libLockBase + 0x4410)

/*
 * filtlib (must be > vigor and < disklib and workercmlp, PR 1340298)
 */
#define RANK_filtLibPollLock         (RANK_vigorOfflineLock + 1)

/*
 * filtib lock which protects a disk's allocation bitmap state.
 * Must be > filtLibPollLock, as it could be acquire with the poll lock held.
 * And as evidenced by PR1437159, it must also be lower than workerLibCmplLock.
 */
#define RANK_filtLibAllocBitmapLock (RANK_filtLibPollLock + 1)

/*
 * remoteUSB (must be < workerCmpl)
 */
#define RANK_remoteUSBGlobalLock (RANK_filtLibAllocBitmapLock + 1)

/*
 * workerLib default completion lock
 *
 * Used for workerLib callers who don't provide their own lock. Held
 * around arbitrary completion callbacks so it makes sense to be of
 * a low rank.
 *
 * Must be > RANK_vigorOfflineLock because we may queue work in Vigor
 * offline.
 *
 * Must be < RANK_nfcLibLock, because NFC uses AIO Generic to perform
 * async writes to the virtual disk.
 *
 * Must be > RANK_filtLibPollLock so that filtlib timers can wait
 * for queued work.
 *
 * Must be > RANK_filtLibAllocBitmapLock due to PR1437159.
 *
 * Must be > RANK_remoteUSBGlobalLock so that virtual CCID can wait for
 * queued work.
 */
#define RANK_workerLibCmplLock       (RANK_remoteUSBGlobalLock + 1)

/*
 * NFC lib lock
 */
#define RANK_nfcLibInitLock          (RANK_libLockBase + 0x4505)
#define RANK_nfcLibLock              (RANK_libLockBase + 0x4506)

/*
 * Policy lib lock
 */
#define RANK_policyLibLock           (RANK_libLockBase + 0x4605)

/*
 * disklib and I/O related locks
 */
#define RANK_diskLibLock             (RANK_libLockBase + 0x5001)
#define RANK_digestLibLock           (RANK_libLockBase + 0x5004)
#define RANK_nasPluginLock           (RANK_libLockBase + 0x5007)
#define RANK_nasPluginMappingLock    (RANK_libLockBase + 0x5008)
#define RANK_diskLibPluginLock       (RANK_libLockBase + 0x5010)
#define RANK_vmioPluginRootLock      (RANK_libLockBase + 0x5020)
#define RANK_vmioPluginSysLock       (RANK_libLockBase + 0x5040)
#define RANK_fsCmdLock               (RANK_libLockBase + 0x5050)
#define RANK_scsiStateLock           (RANK_libLockBase + 0x5060)
#define RANK_parInitLock             (RANK_libLockBase + 0x5070)
#define RANK_namespaceLock           (RANK_libLockBase + 0x5080)
#define RANK_objLibInitLock          (RANK_libLockBase + 0x5085)
#define RANK_vvolLibLock             (RANK_libLockBase + 0x5090)
#define RANK_aioMgrInitLock          (RANK_libLockBase + 0x5095)

/*
 * Persistent-memory logical and hardware  management locks
 */
/* The nvdimm layer is the hardware layer */
#define RANK_nvdHandleLock          (RANK_libLockBase + 0x5300)
/* The pmem layer is the logical layer */
#define RANK_pmemHandleLock         (RANK_libLockBase + 0x5310)

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
 * USB range:
 * (RANK_libLockBase + 0x6500, RANK_libLockBase + 0x6600)
 */

#define RANK_usbArbLibGlobalLock     (RANK_libLockBase + 0x6505)
#define RANK_usbEnumGlobalLock       (RANK_libLockBase + 0x6506)
#define RANK_usbArbLibAsockLock      (RANK_libLockBase + 0x6507)
#define RANK_usbEnumBackendLock      (RANK_libLockBase + 0x6508)
#define RANK_sensorQueueLock         (RANK_libLockBase + 0x6509)

/*
 * misc locks
 *
 * Assuming ordering is important here for the listed locks. Other
 * non-leaf locks are usually defined with RANK_LEAF - 1.
 *
 * At least:
 *    impersonate < pollDefault
 *    keyLocator < preference (for checking AESNI)
 *    keyLocator < sslState (bug 743010)
 *    configDb < keyLocator (for unlocking dictionaries)
 *    battery/button < preference
 *    workerLib < something for sure under VThread_Create
 *    licenseCheck < preference
 *    sslState < getSafeTmpDir
 */

#define RANK_vigorTransportListLock  (RANK_libLockBase + 0x7010)
#define RANK_batteryLock             (RANK_libLockBase + 0x7030)
#define RANK_buttonLock              (RANK_libLockBase + 0x7040)
#define RANK_impersonateLock         (RANK_libLockBase + 0x7045)
#define RANK_pollDefaultLock         (RANK_libLockBase + 0x7050)
#define RANK_workerLibLock           (RANK_libLockBase + 0x7060)
#define RANK_configDbLock            (RANK_libLockBase + 0x7070)
#define RANK_keyLocatorLock          (RANK_libLockBase + 0x7080)
#define RANK_sslStateLock            (RANK_libLockBase + 0x7085)
#define RANK_getSafeTmpDirLock       (RANK_libLockBase + 0x7086)
#define RANK_licenseCheckLock        (RANK_libLockBase + 0x7090)
#define RANK_preferenceLock          (RANK_libLockBase + 0x7100)

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* _LIBMUTEXRANK_H */
