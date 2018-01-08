/*********************************************************
 * Copyright (C) 2010-2017 VMware, Inc. All rights reserved.
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
 * mutexRank.h --
 *
 *	Base lock rank defines. See userlock.h for the related APIs.
 */

#ifndef _MUTEXRANK_H_
#define _MUTEXRANK_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Core rank defines.
 *
 * ANY RANK USAGE ABOVE RANK_LEAF IS RESERVED BY THE PI GROUP.
 */

#define RANK_UNRANKED            0
#define RANK_LEAF                0xFF000000
#define RANK_INVALID             0xFFFFFFFF

/*
 * For situations where we need to create locks on behalf of
 * third-party code, but we don't know what ranking scheme, if any,
 * that code uses.  For now, the only usage is in bora/lib/ssl.
 */
#define RANK_THIRDPARTY          RANK_UNRANKED

/*
 * Log lock rank.
 *
 * Very special case. Don't change it. The effect is that critical
 * logging code cannot call anything else which requires a lock, but
 * everyone else can safely Log() while holding a leaf lock.
 */
#define RANK_logLock             (RANK_LEAF + 2)

/*
 * overheadMem lock rank.
 *
 * Very special case. Don't change it. One must be able to enter
 * the overheadMem Facility at any rank (RANK_LEAF or lower) and
 * still be able to acquire a lock in overheadMem *AND* be able
 * to Log().
 */

#define RANK_overheadMem         (RANK_LEAF + 1)

/*
 * bora/lib/allocTrack rank (not really).
 *
 * This is another special case. It hooks malloc/free and the like,
 * and thus can basically sneak in underneath anyone. To that end
 * allocTrack uses unranked, native locks internally to avoid any
 * complications.
 */

/*
 * VMX/VMM/device lock rank space.
 *
 * This rank space is at the bottom, from 1 to RANK_VMX_LEAF. See
 * vmx/public/mutexRankVMX.h for definitions.
 */

/*
 * Foundry lock rank space.
 *
 * This rank space is from RANK_foundryLockBase on up to
 * RANK_foundryLockLeaf. See apps/lib/foundry/mutexRankFoundry.h for
 * definitions.
 */
#define RANK_foundryLockBase     0x80000000

/*
 * bora/lib lock rank space.
 *
 * This rank space is from RANK_libLockBase on up to RANK_LEAF. See
 * lib/public/mutexRankLib.h for definitions.
 */
#define RANK_libLockBase         0xF0000000

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // ifndef _MUTEXRANK_H_
