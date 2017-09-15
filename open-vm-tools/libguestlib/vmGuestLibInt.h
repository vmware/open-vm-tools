/*********************************************************
 * Copyright (C) 2003-2016 VMware, Inc. All rights reserved.
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


#ifndef _VM_GUEST_LIB_INT_H_
#define _VM_GUEST_LIB_INT_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "vmware.h"
#include "vmGuestLib.h"


/*
 * Backdoor string for retrieving guestlib info
 */

#define VMGUESTLIB_BACKDOOR_COMMAND_STRING "guestlib.info.get"
#define VMGUESTLIB_STATDATA_COMMAND_STRING "guestlib.stat.get"


/*
 * Current version of the VMGuestLibData structure
 */

#define   VMGUESTLIB_DATA_VERSION 3


/* Stat types with a valid bit per stat. */

typedef struct {
   Bool valid;   // Indicates whether this stat is valid on this system
   uint32 value; // Actual stat value.
} StatUint32;

/*
 * This structure comes from the backdoor and hence uses 32-bit
 * natural packing, which is 4-byte aligned. When moved over to
 * 64-bit VMs it becomes 8-byte aligned. To avoid this varying 
 * padding, adding the 3-byte "padding" field and using pack(1)
 * to make sure we always have 4-byte alignment. 
 */
typedef 
#include "vmware_pack_begin.h"
struct {
   Bool valid;       // Indicates whether this stat is valid on this system
   uint8 padding[3]; 
   uint64 value;     // Actual stat value.
}
#include "vmware_pack_end.h"
StatUint64;



/*
 * This is version 1 of the data structure GuestLib uses to obtain
 * stats over the backdoor from the VMX/VMKernel. It is deprecated.
 */
#if 0

typedef
#include "vmware_pack_begin.h"
struct VMGuestLibDataV1 {
   uint32 version;
   VMSessionId sessionId;

   /* Statistics */

   /* CPU statistics */
   uint32 cpuReservationMHz;
   uint32 cpuLimitMHz;
   uint32 cpuShares;
   uint64 cpuUsedMs;

   /* Host processor speed */
   uint32 hostMHz;

   /* Memory statistics */
   uint32 memReservationMB;
   uint32 memLimitMB;
   uint32 memShares;
   uint32 memMappedMB;
   uint32 memActiveMB;
   uint32 memOverheadMB;
   uint32 memBalloonedMB;
   uint32 memSwappedMB;

   /* Elapsed time */
   uint64 elapsedMs;

   /*
    * Resource pool path. See groupPathName in Sched_GuestLibInfo,
    * defined in VMKernel's sched_ext.h. This needs to be at least
    * as big as SCHED_GROUP_PATHNAME_LEN.
    */
   char resourcePoolPath[512];
}
#include "vmware_pack_end.h"
VMGuestLibDataV1;

#endif // #if 0

typedef
#include "vmware_pack_begin.h"
struct {
   uint32 version;
   VMSessionId sessionId;
}
#include "vmware_pack_end.h"
VMGuestLibHeader;

/*
 * This is version 2 of the data structure GuestLib uses to obtain
 * stats over the backdoor from the VMX/VMKernel. It is not
 * exposed to users of the GuestLib API.
 */

typedef
#include "vmware_pack_begin.h"
struct VMGuestLibDataV2 {
   /* Header */
   VMGuestLibHeader hdr;

   /* Statistics */

   /* CPU statistics */
   StatUint32 cpuReservationMHz;
   StatUint32 cpuLimitMHz;
   StatUint32 cpuShares;
   StatUint64 cpuUsedMs;

   /* Host processor speed */
   StatUint32 hostMHz;

   /* Memory statistics */
   StatUint32 memReservationMB;
   StatUint32 memLimitMB;
   StatUint32 memShares;
   StatUint32 memMappedMB;
   StatUint32 memActiveMB;
   StatUint32 memOverheadMB;
   StatUint32 memBalloonedMB;
   StatUint32 memSwappedMB;
   StatUint32 memSharedMB;
   StatUint32 memSharedSavedMB;
   StatUint32 memUsedMB;

   /* Elapsed time */
   StatUint64 elapsedMs;

   /*
    * Resource pool path. See groupPathName in Sched_GuestLibInfo,
    * defined in VMKernel's sched_ext.h. This needs to be at least
    * as big as SCHED_GROUP_PATHNAME_LEN.
    */
   struct {
      Bool valid;
      char value[512];
   } resourcePoolPath;
}
#include "vmware_pack_end.h"
VMGuestLibDataV2;


/*
 * This is version 3 of the data structure GuestLib uses to obtain stats over
 * the backdoor from the VMX/VMKernel. It is not exposed to users of the
 * GuestLib API. This struct is sent on the wire.
 *
 * The buffer contains a variable length array of statistics, that are
 * marshalled from XDR spec generated code. Each data field has a discriminant
 * preceding the payload, which enables the client to detect fields that it
 * doesn't recognize.
 *
 * V3 is a superset of V2 and a major protocol change. Any extensions to the
 * wire protocol that just add statistics, can do so within V3. Just update the
 * .x file to add a new discriminant for the union, at the end of the list. V3
 * clients may assume that the fields are ordered on the wire in increasing order 
 * of the discriminant list. So, a client may stop processing at the first 
 * unrecognized field. V3 payload contains all available guestlib statistics
 * supported by the host.
 */

typedef
#include "vmware_pack_begin.h"
struct VMGuestLibDataV3 {
   /* Header */
   VMGuestLibHeader hdr;

   /* Statistics */
   uint32 dataSize;
   char data[0];
}
#include "vmware_pack_end.h"
VMGuestLibDataV3;

#endif /* _VM_GUEST_LIB_INT_H_ */
