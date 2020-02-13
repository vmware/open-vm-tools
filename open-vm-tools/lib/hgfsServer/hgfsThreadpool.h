/*********************************************************
 * Copyright (C) 2020 VMware, Inc. All rights reserved.
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

#ifndef _HGFS_THREADPOOL_H
#define _HGFS_THREADPOOL_H

/*
 * hgfsThreadpool.h --
 *
 *	Function definitions for threadpool.
 */

#include "hgfsUtil.h"       // for HgfsInternalStatus
#include "hgfsServerInt.h"  // for HgfsSessionInfo

/*
 * The maximum count of thread in threadpool.
 * For Windows 10 guest, the file explorer issues 8 asynchronous request
 * at one time.
 * Please keep this value same with the PHYSMEM_HGFS_WORKER_THREADS
 */
#define HGFS_THREADPOOL_MAX_COUNT 10

typedef void(*HgfsThreadpoolWorkItem)(void *data);

HgfsInternalStatus HgfsThreadpool_Init(void);

Bool HgfsThreadpool_Activate(void);
void HgfsThreadpool_Deactivate(void);

void HgfsThreadpool_Exit(void);
Bool HgfsThreadpool_QueueWorkItem(HgfsThreadpoolWorkItem workItem, void *data);

#endif // _HGFS_THREADPOOL_H
