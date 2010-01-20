/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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

#ifndef _VMXNET2_DEF_H_
#define _VMXNET2_DEF_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#include "net_sg.h"
#include "vmxnet_def.h"


/*
 * Magic number that identifies this version of the vmxnet protocol.
 */
#define VMXNET2_MAGIC			0xbabe864f

/* size of the rx ring */
#define VMXNET2_MAX_NUM_RX_BUFFERS		128
#define VMXNET2_DEFAULT_NUM_RX_BUFFERS	        100


/* size of the rx ring when enhanced vmxnet is used */
#define ENHANCED_VMXNET2_MAX_NUM_RX_BUFFERS     512 
#define ENHANCED_VMXNET2_DEFAULT_NUM_RX_BUFFERS 150 

/* size of the 2nd rx ring */
#define VMXNET2_MAX_NUM_RX_BUFFERS2             2048 
#define VMXNET2_DEFAULT_NUM_RX_BUFFERS2	        512

/* size of the tx ring */
#define VMXNET2_MAX_NUM_TX_BUFFERS		128
#define VMXNET2_DEFAULT_NUM_TX_BUFFERS	        100

/* size of the tx ring when tso/jf is used */
#define VMXNET2_MAX_NUM_TX_BUFFERS_TSO          512
#define VMXNET2_DEFAULT_NUM_TX_BUFFERS_TSO	256

enum {
   VMXNET2_OWNERSHIP_DRIVER,
   VMXNET2_OWNERSHIP_DRIVER_PENDING,
   VMXNET2_OWNERSHIP_NIC,
   VMXNET2_OWNERSHIP_NIC_PENDING,
   VMXNET2_OWNERSHIP_NIC_FRAG,
   VMXNET2_OWNERSHIP_DRIVER_FRAG,
};

#define VMXNET2_SG_DEFAULT_LENGTH	6

typedef struct Vmxnet2_SG_Array {
   uint16	addrType;
   uint16	length;
   NetSG_Elem	sg[VMXNET2_SG_DEFAULT_LENGTH];
} Vmxnet2_SG_Array;

typedef struct Vmxnet2_RxRingEntry {
   uint64		paddr;		/* Physical address of the packet data. */
   uint32		bufferLength;	/* The length of the data at paddr. */
   uint32		actualLength;	/* The actual length of the received data. */
   uint16               ownership;	/* Who owns the packet. */
   uint16		flags;		/* Flags as defined below. */
   uint32               index;          /* 
                                         * Currently:
                                         *
                                         * This is being used as an packet index to
                                         * rx buffers.
                                         *
                                         * Originally: 
                                         *
					 * was void* driverData ("Driver specific data.")
					 * which was used for sk_buf**s in Linux and
                                         * VmxnetRxBuff*s in Windows.  It could not be
					 * here because the structure needs to be the
					 * same size between architectures, and it was
					 * not used on the device side, anyway.  Look
					 * for its replacement in
					 * Vmxnet_Private.rxRingBuffPtr on Linux and
					 * VmxnetAdapter.rxRingBuffPtr on Windows.
					 */
} Vmxnet2_RxRingEntry;

/*
 * Vmxnet2_RxRingEntry flags:
 * 
 * VMXNET2_RX_HW_XSUM_OK       The hardware verified the TCP/UDP checksum.
 * VMXNET2_RX_WITH_FRAG        More data is in the 2nd ring
 * VMXNET2_RX_FRAG_EOP         This is the last frag, the only valid flag for
 *                             2nd ring entry
 *
 */
#define VMXNET2_RX_HW_XSUM_OK  0x01
#define VMXNET2_RX_WITH_FRAG   0x02
#define VMXNET2_RX_FRAG_EOP    0x04

typedef struct Vmxnet2_TxRingEntry {
   uint16		flags;		/* Flags as defined below. */
   uint16 	        ownership;	/* Who owns this packet. */
   uint32               extra;          /*
					 * was void* driverData ("Driver specific data.")
					 * which was used for sk_buf*s in Linux and
                                         * VmxnetTxInfo*s in Windows.  It could not be
					 * here because the structure needs to be the
					 * same size between architectures, and it was
					 * not used on the device side, anyway.  Look
					 * for its replacement in
					 * Vmxnet_Private.txRingBuffPtr on Linux and
					 * VmxnetAdapter.txRingBuffPtr on Windows.
					 */
   uint32               tsoMss;         /* TSO pkt MSS */
   Vmxnet2_SG_Array	sg;		/* Packet data. */
} Vmxnet2_TxRingEntry;

/*
 * Vmxnet2_TxRingEntry flags:
 *
 *   VMXNET2_TX_CAN_KEEP	The implementation can return the tx ring entry 
 *				to the driver when it is ready as opposed to 
 *				before the transmit call from the driver completes.
 *   VMXNET2_TX_RING_LOW	The driver's transmit ring buffer is low on free
 *				slots.
 *   VMXNET2_TX_HW_XSUM         The hardware should perform the TCP/UDP checksum
 *   VMXNET2_TX_TSO             The hardware should do TCP segmentation.
 *   VMXNET2_TX_PINNED_BUFFER   The driver used one of the preallocated vmkernel
 *                              buffers *and* it has been pinned with Net_PinTxBuffers.
 *   VMXNET2_TX_MORE            This is *not* the last tx entry for the pkt.
 *                              All flags except VMXNET2_TX_MORE are ignored
 *                              for the subsequent tx entries.
 */
#define VMXNET2_TX_CAN_KEEP	     0x0001
#define VMXNET2_TX_RING_LOW	     0x0002
#define VMXNET2_TX_HW_XSUM           0x0004
#define VMXNET2_TX_TSO	             0x0008
#define VMXNET2_TX_PINNED_BUFFER     0x0010
#define VMXNET2_TX_MORE              0x0020

/*
 * Structure used by implementations.  This structure allows the inline
 * functions below to be used.
 */
typedef struct Vmxnet2_RxRingInfo {
   Vmxnet2_RxRingEntry    *base;       /* starting addr of the ring */
   uint32                  nicNext;    /* next entry to use in the ring */
   uint32                  ringLength; /* # of entries in the ring */
   PA                      startPA;    /* PA of the starting addr of the ring */
#ifdef VMX86_DEBUG
   const char             *name;
#endif
} Vmxnet2_RxRingInfo;

typedef struct Vmxnet2_TxRingInfo {
   Vmxnet2_TxRingEntry    *base;       /* starting addr of the ring */
   uint32                  nicNext;    /* next entry to use in the ring */
   uint32                  ringLength; /* # of entries in the ring */
   PA                      startPA;    /* PA of the starting addr of the ring */
#ifdef VMX86_DEBUG
   const char             *name;
#endif
} Vmxnet2_TxRingInfo;

typedef struct Vmxnet2_ImplData {
   Vmxnet2_RxRingInfo    rxRing;
   Vmxnet2_RxRingInfo    rxRing2;
   Vmxnet2_TxRingInfo    txRing;

   struct PhysMem_Token	  *ddPhysMemToken;
} Vmxnet2_ImplData;

/* 
 * Used internally for performance studies. By default this will be off so there 
 * should be no compatibilty or other interferences.
 */

/* #define ENABLE_VMXNET2_PROFILING    */


#ifdef ENABLE_VMXNET2_PROFILING
typedef struct Vmxnet2_VmmStats {
   uint64      vIntTSC;             /* the time that virtual int was posted */
   uint64      actionsCount;        /* Number of actions received */
   uint64      numWasteActions;     /* Number of non-productive actions */
}  Vmxnet2_VmmStats;
#endif

typedef struct Vmxnet2_DriverStats {
   uint32	transmits;	   /* # of times that the drivers transmit function */
				   /*   is called. The driver could transmit more */
				   /*   than one packet per call. */
   uint32	pktsTransmitted;   /* # of packets transmitted. */
   uint32	noCopyTransmits;   /* # of packets that are transmitted without */
				   /*   copying any data. */
   uint32	copyTransmits;	   /* # of packets that are transmittted by copying */
				   /*   the data into a buffer. */
   uint32	maxTxsPending;	   /* Max # of transmits outstanding. */
   uint32	txStopped;	   /* # of times that transmits got stopped because */
				   /*   the tx ring was full. */
   uint32	txRingOverflow;	   /* # of times that transmits got deferred bc */
				   /*   the tx ring was full.  This must be >= */
				   /*   txStopped since there will be one */
				   /*   txStopped when the ring fills up and then */
				   /*   one txsRingOverflow for each packet that */
				   /*   that gets deferred until there is space. */
   uint32	interrupts;	   /* # of times interrupted. */
   uint32	pktsReceived;	   /* # of packets received. */
   uint32	rxBuffersLow;	   /* # of times that the driver was low on */
				   /*   receive buffers. */
#ifdef ENABLE_VMXNET2_PROFILING
    Vmxnet2_VmmStats  vmmStats;     /* vmm related stats for perf study */
#endif
} Vmxnet2_DriverStats;

/*
 * Shared data structure between the vm, the vmm, and the vmkernel.
 * This structure was originally arranged to try to group common data 
 * on 32-byte cache lines, but bit rot and the fact that we no longer
 * run on many CPUs with that cacheline size killed that optimization.
 * vmxnet3 should target 128 byte sizes and alignments to optimize for
 * the 64 byte cacheline pairs on P4.
 */
typedef struct Vmxnet2_DriverData {
   /*
    * Magic must be first.
    */
   Vmxnet_DDMagic       magic;

   /*
    * Receive fields. 
    */
   uint32		rxRingLength;		/* Length of the receive ring. */
   uint32		rxDriverNext;		/* Index of the next packet that will */
						/*   be filled in by the impl */

   uint32		rxRingLength2;	        /* Length of the 2nd receive ring. */
   uint32		rxDriverNext2;	        /* Index of the next packet that will */
						/*   be filled in by the impl */

   uint32		notUsed1;               /* was "irq" */

   /*
    * Interface flags and multicast filter.
    */
   uint32		ifflags;
   uint32		LADRF[VMXNET_MAX_LADRF];

   /*
    * Transmit fields
    */
   uint32               txDontClusterSize;      /* All packets <= this will be transmitted */
                                                /* immediately, regardless of clustering */
                                                /* settings [was fill[1]] */
   uint32		txRingLength;		/* Length of the transmit ring. */
   uint32		txDriverCur;		/* Index of the next packet to be */
						/*   returned by the implementation.*/
   uint32		txDriverNext;		/* Index of the entry in the ring */
						/*   buffer to use for the next packet.*/
   uint32		txStopped;  		/* The driver has stopped transmitting */
						/*   because its ring buffer is full.*/
   uint32		txClusterLength;	/* Maximum number of packets to */
						/*   put in the ring buffer before */
						/*   asking the implementation to */
						/*   transmit the packets in the buffer.*/
   uint32		txNumDeferred;          /* Number of packets that have been */
						/*   queued in the ring buffer since */
						/*   the last time the implementation */
						/*   was asked to transmit. */
   uint32		notUsed3;               /* This field is deprecated but still used */
                                                /* as minXmitPhysLength on the escher branch. */
                                                /* It cannot be used for other purposes */
                                                /* until escher vms no longer are allowed */
                                                /* to install this driver. */

   uint32              totalRxBuffers;          /* used by esx for max rx buffers */
   uint64              rxBufferPhysStart;       /* used by esx for pinng rx buffers */
   /*
    * Extra fields for future expansion.
    */
   uint32		extra[2];

   uint16               maxFrags;               /* # of frags the driver can handle */
   uint16               featureCtl;             /* for driver to enable some feature */

   /*
    * The following fields are used to save the nicNext indexes part
    * of implData in the vmkernel when disconnecting the adapter, we
    * need them when we reconnect.  This mechanism is used for
    * checkpointing as well.
    */
   uint32               savedRxNICNext;
   uint32               savedRxNICNext2;
   uint32               savedTxNICNext;

   /*
    * Fields used during initialization or debugging.
    */
   uint32		length;
   uint32		rxRingOffset;
   uint32		rxRingOffset2;
   uint32		txRingOffset;   
   uint32		debugLevel;
   uint32		txBufferPhysStart;
   uint32		txBufferPhysLength;
   uint32		txPktMaxSize;

   /*
    * Driver statistics.
    */
   Vmxnet2_DriverStats	stats;
} Vmxnet2_DriverData;

/* 
 * Shared between VMM and Vmkernel part of vmxnet2 to optimize action posting
 * VMM writes 1 (don't post) or 0 (okay to post) and vmk reads this.
 */
typedef struct VmxnetVMKShared {
   uint32  dontPostActions;  
} VmxnetVMKShared;

#if defined VMX86_VMX || defined VMKERNEL

/*
 * Inline functions used to assist the implementation of the vmxnet interface.
 */

/*
 * Get the next empty packet out of the receive ring and move to 
 * the next packet.
 */
static INLINE Vmxnet2_RxRingEntry *
Vmxnet2_GetNextRx(Vmxnet2_RxRingInfo *ri, uint16 ownership)
{
   Vmxnet2_RxRingEntry *rre = ri->base + ri->nicNext;
   if (rre->ownership == ownership) {
      VMXNET_INC(ri->nicNext, ri->ringLength);
   } else {
      rre = NULL;
   }

   return rre;
}

/*
 * Return ownership of a packet in the receive ring to the driver.
 */
static INLINE void
Vmxnet2_PutRx(Vmxnet2_RxRingEntry *rre, uint32 pktLength, uint16 ownership)
{
   rre->actualLength = pktLength;
   COMPILER_MEM_BARRIER();
   rre->ownership = ownership;
}

/*
 * Get the next pending packet out of the transmit ring.
 */
static INLINE Vmxnet2_TxRingEntry *
Vmxnet2_GetNextTx(Vmxnet2_TxRingInfo *ri)
{
   Vmxnet2_TxRingEntry *txre = ri->base + ri->nicNext;
   if (txre->ownership == VMXNET2_OWNERSHIP_NIC) {
      return txre;
   } else {
      return NULL;
   }
}

/*
 * Move to the next entry in the transmit ring.
 */
static INLINE unsigned int
Vmxnet2_IncNextTx(Vmxnet2_TxRingInfo *ri)
{
   unsigned int prev = ri->nicNext;
   Vmxnet2_TxRingEntry *txre = ri->base + ri->nicNext;
   
   txre->ownership = VMXNET2_OWNERSHIP_NIC_PENDING;

   VMXNET_INC(ri->nicNext, ri->ringLength);
   return prev;
}

/*
 * Get the indicated entry from transmit ring.
 */
static INLINE Vmxnet2_TxRingEntry *
Vmxnet2_GetTxEntry(Vmxnet2_TxRingInfo *ri, unsigned int idx)
{
   return ri->base + idx;
}

/*
 * Get the indicated entry from the given rx ring
 */
static INLINE Vmxnet2_RxRingEntry *
Vmxnet2_GetRxEntry(Vmxnet2_RxRingInfo *ri, unsigned int idx)
{
   return ri->base + idx;
}

#endif /* defined VMX86_VMX || defined VMKERNEL */

#endif

