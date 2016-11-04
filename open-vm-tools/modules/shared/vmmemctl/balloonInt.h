/*********************************************************
 * Copyright (C) 2012,2014-2016 VMware, Inc. All rights reserved.
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

/*********************************************************
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *********************************************************/

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/
#ifndef BALLOONINT_H_
# define BALLOONINT_H_

#include "balloon_def.h"

/*
 * Compile-Time Options
 */

#define BALLOON_NAME                    "vmmemctl"
#define BALLOON_NAME_VERBOSE            "VMware memory control driver"

// Capabilities for Windows are defined at run-tine (see nt/vmballoon.c)
#if defined __linux__ || (defined __APPLE__ && defined __LP64__)
#define BALLOON_CAPABILITIES    (BALLOON_BASIC_CMDS|BALLOON_BATCHED_CMDS|\
                                 BALLOON_BATCHED_2M_CMDS)
#elif defined __FreeBSD__ || (defined __APPLE__ && !defined __LP64__)
#define BALLOON_CAPABILITIES    (BALLOON_BASIC_CMDS|BALLOON_BATCHED_CMDS)
#else
#define BALLOON_CAPABILITIES    BALLOON_BASIC_CMDS
#endif

#define BALLOON_RATE_ADAPT      1

#define BALLOON_DEBUG           1
#define BALLOON_DEBUG_VERBOSE   0

#define BALLOON_POLL_PERIOD             1 /* sec */
#define BALLOON_NOSLEEP_ALLOC_MAX       16384

#define BALLOON_RATE_ALLOC_MIN          512
#define BALLOON_RATE_ALLOC_MAX          2048
#define BALLOON_RATE_ALLOC_INC          16

#define BALLOON_RATE_FREE_MIN           512
#define BALLOON_RATE_FREE_MAX           16384
#define BALLOON_RATE_FREE_INC           16

/*
 * Move it to bora/public/balloon_def.h later, if needed. Note that
 * BALLOON_PAGE_ALLOC_FAILURE is an internal error code used for
 * distinguishing page allocation failures from monitor-backdoor errors.
 * We use value 1000 because all monitor-backdoor error codes are < 1000.
 */
#define BALLOON_PAGE_ALLOC_FAILURE      1000

#define BALLOON_STATS

#ifdef	BALLOON_STATS
#define	STATS_INC(stat)	(stat)++
#define	STATS_DEC(stat)	(stat)--
#else
#define	STATS_INC(stat)
#define	STATS_DEC(stat)
#endif

#define PPN_2_PA(_ppn)  ((PPN64)(_ppn) << PAGE_SHIFT)
#define PA_2_PPN(_pa)   ((_pa) >> PAGE_SHIFT)

#endif /* !BALLOONINT_H_ */
