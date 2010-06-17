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

/*
 * hgfsTransport.h --
 *
 * Transport file shared between guest drivers and host.
 */

#ifndef _HGFS_TRANSPORT_H_
# define _HGFS_TRANSPORT_H_

#include "vmci_defs.h"

/****************************************
 * Vsock, Tcp specific data structures  *
 ****************************************/

/* Some fudged values for TCP over sockets. */
#define HGFS_HOST_PORT 2000

/* Socket packet magic. */
#define HGFS_SOCKET_VERSION1   1

/*
 * Socket status codes.
 */

typedef enum {
   HGFS_SOCKET_STATUS_SUCCESS,                  /* Socket header is good. */
   HGFS_SOCKET_STATUS_SIZE_MISMATCH,            /* Size and version are incompatible. */
   HGFS_SOCKET_STATUS_VERSION_NOT_SUPPORTED,    /* Version not handled by remote. */
   HGFS_SOCKET_STATUS_INVALID_PACKETLEN,        /* Message len exceeds maximum. */
} HgfsSocketStatus;

/*
 * Socket flags.
 */

typedef uint32 HgfsSocketFlags;

/* Used by backdoor proxy socket client to Hgfs server (out of VMX process). */
#define HGFS_SOCKET_SYNC         (1 << 0)

/* Socket packet header. */
typedef
#include "vmware_pack_begin.h"
struct HgfsSocketHeader {
   uint32 version;            /* Header version. */
   uint32 size;               /* Header size, should match for the specified version. */
   HgfsSocketStatus status;   /* Status: always success when sending (ignored) valid on replies. */
   uint32 packetLen;          /* The length of the packet to follow. */
   HgfsSocketFlags flags;     /* The flags to indicate how to deal with the packet. */
}
#include "vmware_pack_end.h"
HgfsSocketHeader;

#define HgfsSocketHeaderInit(hdr, _version, _size, _status, _pktLen, _flags) \
   do {                                                                      \
      (hdr)->version    = (_version);                                        \
      (hdr)->size       = (_size);                                           \
      (hdr)->status     = (_status);                                         \
      (hdr)->packetLen  = (_pktLen);                                         \
      (hdr)->flags      = (_flags);                                          \
   } while (0)


/************************************************
 *    VMCI specific data structures, macros     *
 ************************************************/

#define HGFS_VMCI_VERSION_1          0x1

typedef enum {
   HGFS_TS_IO_PENDING,
   HGFS_TS_IO_COMPLETE,
   HGFS_TS_IO_FAILED,
} HgfsTransportRequestState;

typedef enum {
   HGFS_ASYNC_IOREQ_SHMEM,
   HGFS_ASYNC_IOREQ_GET_PAGES,
   HGFS_ASYNC_IOREP,
} HgfsAsyncReplyFlags;

typedef enum {
   HGFS_TH_REP_GET_PAGES,
   HGFS_TH_REQUEST,
   HGFS_TH_TERMINATE_SESSION,
} HgfsTransportPacketType;

#define HGFS_VMCI_TRANSPORT_ERROR   (VMCI_ERROR_CLIENT_MIN - 1)
#define HGFS_VMCI_VERSION_MISMATCH  (VMCI_ERROR_CLIENT_MIN - 2)

/*
 * Used By : Guest and Host
 * Lives in : Inside HgfsVmciTransportHeader
 */

typedef
#include "vmware_pack_begin.h"
struct HgfsIov {
   uint64 pa;                 /* Physical addr */
   uint32 len;                /* length of data; should be <= PAGE_SIZE */
}
#include "vmware_pack_end.h"
HgfsIov;

/*
 * Used By : Guest and Host
 * Lives in : Inside HgfsVmciTransportHeader
 */

typedef
#include "vmware_pack_begin.h"
struct HgfsAsyncIov {
   uint64 pa;                 /* Physical addr */
   uint64 va;                 /* Virtual addr */
   uint32 len;                /* length of data; should be <= PAGE_SIZE */
   uint64 index;              /* Guest opaque data; should not be changed by
                                 host */
   Bool chain;                /* Are pages chained ? */
}
#include "vmware_pack_end.h"
HgfsAsyncIov;

/*
 * Every VMCI request will have this transport Header sent over
 * in the datagram by the Guest OS.
 *
 * Used By : Guest and Host
 * Lives in : Sent by Guest inside VMCI datagram
 */
typedef
#include "vmware_pack_begin.h"
struct HgfsVmciTransportHeader {
   uint32 version;                          /* Version number */
   HgfsTransportPacketType pktType;         /* Type of packet */
   uint32 iovCount;                         /* Number of iovs */
   union {
      HgfsIov iov[1];                       /* (PA, len) */
      HgfsAsyncIov asyncIov[1];
   };
}
#include "vmware_pack_end.h"
HgfsVmciTransportHeader;

/*
 * Indicates status of VMCI requests. If the requests are processed sync
 * by the hgfsServer then guest should see IO_COMPLETE otherwise IO_PENDING.
 *
 * Used By: Guest and Host
 * Lives in: Guest Memory
 */
typedef
#include "vmware_pack_begin.h"
struct HgfsVmciTransportStatus {
   HgfsTransportRequestState status; /* IO_PENDING, IO_COMPLETE, IO_FAILED etc */
   uint32 size;                      /* G->H: Size of the packet,H->G: How much more space is needed */
}
#include "vmware_pack_end.h"
HgfsVmciTransportStatus;

typedef
#include "vmware_pack_begin.h"
struct HgfsVmciAsyncResponse {
   uint64 id;            /* Id corresponding to the guest request */
}
#include "vmware_pack_end.h"
HgfsVmciAsyncResponse;

typedef
#include "vmware_pack_begin.h"
struct HgfsVmciAsyncShmem {
   uint32 count;          /* Number of iovs */
   HgfsAsyncIov iov[1];
}
#include "vmware_pack_end.h"
HgfsVmciAsyncShmem;

typedef
#include "vmware_pack_begin.h"
struct HgfsVmciAsyncReply {
   uint32 version;
   HgfsAsyncReplyFlags pktType;
   union {
     HgfsVmciAsyncResponse response;
     HgfsVmciAsyncShmem shmem;
   };
}
#include "vmware_pack_end.h"
HgfsVmciAsyncReply;

#endif /* _HGFS_TRANSPORT_H_ */

