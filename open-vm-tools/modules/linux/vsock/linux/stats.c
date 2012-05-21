/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * stats.c --
 *
 *      Linux stats for the VMCI Stream Sockets protocol.
 */

#include "driver-config.h"

#include <linux/socket.h>
#include "compat_sock.h"

#include "af_vsock.h"
#include "stats.h"

#ifdef VSOCK_GATHER_STATISTICS
uint64 vSockStatsCtlPktCount[VSOCK_PACKET_TYPE_MAX];
uint64 vSockStatsConsumeQueueHist[VSOCK_NUM_QUEUE_LEVEL_BUCKETS];
uint64 vSockStatsProduceQueueHist[VSOCK_NUM_QUEUE_LEVEL_BUCKETS];
Atomic_uint64 vSockStatsConsumeTotal;
Atomic_uint64 vSockStatsProduceTotal;
#endif
