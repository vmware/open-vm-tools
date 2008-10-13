/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * vmxnet3_defs.h --
 *
 *      Definitions shared by device emulation and guest drivers for
 *      VMXNET3 NIC
 */

#ifndef _VMXNET3_DEFS_H_
#define _VMXNET3_DEFS_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#include "upt1_defs.h"

/* all registers are 32 bit wide */
/* BAR 1 */
#define VMXNET3_REG_VRRS  0x0    /* Vmxnet3 Revision Report Selection */
#define VMXNET3_REG_UVRS  0x8    /* UPT Version Report Selection */
#define VMXNET3_REG_DSAL  0x10   /* Driver Shared Address Low */
#define VMXNET3_REG_DSAH  0x18   /* Driver Shared Address High */
#define VMXNET3_REG_CMD   0x20   /* Command */
#define VMXNET3_REG_MACL  0x28   /* MAC Address Low */
#define VMXNET3_REG_MACH  0x30   /* MAC Address High */
#define VMXNET3_REG_ICR   0x38   /* Interrupt Cause Register */
#define VMXNET3_REG_ECR   0x40   /* Event Cause Register */

/* BAR 0 */
#define VMXNET3_REG_IMR      0x0   /* Interrupt Mask Register */
#define VMXNET3_REG_TXPROD   0x600 /* Tx Producer Index */
#define VMXNET3_REG_RXPROD   0x800 /* Rx Producer Index for ring 1 */
#define VMXNET3_REG_RXPROD2  0xA00 /* Rx Producer Index for ring 2 */

#define VMXNET3_PT_REG_SIZE     4096    /* BAR 0 */
#define VMXNET3_VD_REG_SIZE     4096    /* BAR 1 */

#define VMXNET3_REG_ALIGN       8  /* All registers are 8-byte aligned. */
#define VMXNET3_REG_ALIGN_MASK  0x7

/* I/O Mapped access to registers */
#define VMXNET3_IO_TYPE_PT              0
#define VMXNET3_IO_TYPE_VD              1
#define VMXNET3_IO_ADDR(type, reg)      (((type) << 24) | ((reg) & 0xFFFFFF))
#define VMXNET3_IO_TYPE(addr)           ((addr) >> 24)
#define VMXNET3_IO_REG(addr)            ((addr) & 0xFFFFFF)

typedef enum {
   VMXNET3_CMD_FIRST_SET = 0xCAFE0000,
   VMXNET3_CMD_ACTIVATE_DEV = VMXNET3_CMD_FIRST_SET,
   VMXNET3_CMD_QUIESCE_DEV,
   VMXNET3_CMD_RESET_DEV,
   VMXNET3_CMD_UPDATE_RX_MODE,
   VMXNET3_CMD_UPDATE_MAC_FILTERS,
   VMXNET3_CMD_UPDATE_VLAN_FILTERS,
   VMXNET3_CMD_UPDATE_RSSIDT,
   VMXNET3_CMD_UPDATE_IML,
   VMXNET3_CMD_UPDATE_PMCFG,
   VMXNET3_CMD_UPDATE_FEATURE,

   VMXNET3_CMD_FIRST_GET = 0xF00D0000,
   VMXNET3_CMD_GET_QUEUE_STATUS = VMXNET3_CMD_FIRST_GET,
   VMXNET3_CMD_GET_STATS,
   VMXNET3_CMD_GET_LINK,
   VMXNET3_CMD_GET_PERM_MAC_LO,
   VMXNET3_CMD_GET_PERM_MAC_HI,
   VMXNET3_CMD_GET_DID_LO,
   VMXNET3_CMD_GET_DID_HI,
   VMXNET3_CMD_GET_DEV_EXTRA_INFO,
   VMXNET3_CMD_GET_CONF_INTR
} Vmxnet3_Cmd;

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_TxDesc {
   uint64 addr;

   uint32 len:14;
   uint32 gen:1;      /* generation bit */
   uint32 rsvd:1;
   uint32 dtype:1;    /* descriptor type */
   uint32 ext1:1;
   uint32 msscof:14;  /* MSS, checksum offset, flags */

   uint32 hlen:10;    /* header len */
   uint32 om:2;       /* offload mode */
   uint32 eop:1;      /* End Of Packet */
   uint32 cq:1;       /* completion request */
   uint32 ext2:1;
   uint32 ti:1;       /* VLAN Tag Insertion */
   uint32 tci:16;     /* Tag to Insert */
}
#include "vmware_pack_end.h"
Vmxnet3_TxDesc;

/* TxDesc.OM values */
#define VMXNET3_OM_NONE  0
#define VMXNET3_OM_CSUM  2
#define VMXNET3_OM_TSO   3

/* fields in TxDesc we access w/o using bit fields */
#define VMXNET3_TXD_EOP_SHIFT 12
#define VMXNET3_TXD_CQ_SHIFT  13
#define VMXNET3_TXD_GEN_SHIFT 14

#define VMXNET3_TXD_CQ  (1 << VMXNET3_TXD_CQ_SHIFT)
#define VMXNET3_TXD_EOP (1 << VMXNET3_TXD_EOP_SHIFT)
#define VMXNET3_TXD_GEN (1 << VMXNET3_TXD_GEN_SHIFT)

#define VMXNET3_HDR_COPY_SIZE   128

typedef
#include "vmware_pack_begin.h"
struct vmxnet3_TxDataDesc {
   uint8 data[VMXNET3_HDR_COPY_SIZE];
}
#include "vmware_pack_end.h"
Vmxnet3_TxDataDesc;

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_TxCompDesc {
   uint32 txdIdx:12;    /* Index of the EOP TxDesc */
   uint32 ext1:20;

   uint32 ext2;
   uint32 ext3;

   uint32 rsvd:24;
   uint32 type:7;       /* completion type */
   uint32 gen:1;        /* generation bit */
}
#include "vmware_pack_end.h"
Vmxnet3_TxCompDesc;

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_RxDesc {
   uint64 addr;

   uint32 len:14;
   uint32 btype:1;      /* Buffer Type */
   uint32 dtype:1;      /* Descriptor type */
   uint32 rsvd:15;
   uint32 gen:1;        /* Generation bit */

   uint32 ext1;
}
#include "vmware_pack_end.h"
Vmxnet3_RxDesc;

/* values of RXD.BTYPE */
#define VMXNET3_RXD_BTYPE_HEAD   0    /* head only */
#define VMXNET3_RXD_BTYPE_BODY   1    /* body only */

/* fields in RxDesc we access w/o using bit fields */
#define VMXNET3_RXD_BTYPE_SHIFT  14
#define VMXNET3_RXD_GEN_SHIFT    31

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_RxCompDesc {
   uint32 rxdIdx:12;    /* Index of the RxDesc */
   uint32 ext1:2;
   uint32 eop:1;        /* End of Packet */
   uint32 sop:1;        /* Start of Packet */
   uint32 rqID:10;      /* rx queue/ring ID */
   uint32 rssType:4;    /* RSS hash type used */
   uint32 cnc:1;        /* Checksum Not Calculated */
   uint32 ext2:1;

   uint32 rssHash;      /* RSS hash value */

   uint32 len:14;       /* data length */
   uint32 err:1;        /* Error */
   uint32 ts:1;         /* Tag is stripped */
   uint32 tci:16;       /* Tag stripped */

   uint32 csum:16;
   uint32 tuc:1;        /* TCP/UDP Checksum Correct */
   uint32 udp:1;        /* UDP packet */
   uint32 tcp:1;        /* TCP packet */
   uint32 ipc:1;        /* IP Checksum Correct */
   uint32 v6:1;         /* IPv6 */
   uint32 v4:1;         /* IPv4 */
   uint32 frg:1;        /* IP Fragment */
   uint32 fcs:1;        /* Frame CRC correct */
   uint32 type:7;       /* completion type */
   uint32 gen:1;        /* generation bit */
}
#include "vmware_pack_end.h"
Vmxnet3_RxCompDesc;

/* fields in RxCompDesc we access via Vmxnet3_GenericDesc.dword[3] */
#define VMXNET3_RCD_TUC_SHIFT  16
#define VMXNET3_RCD_IPC_SHIFT  19

/* fields in RxCompDesc we access via Vmxnet3_GenericDesc.qword[1] */
#define VMXNET3_RCD_TYPE_SHIFT 56
#define VMXNET3_RCD_GEN_SHIFT  63

/* csum OK for TCP/UDP pkts over IP */
#define VMXNET3_RCD_CSUM_OK (1 << VMXNET3_RCD_TUC_SHIFT | 1 << VMXNET3_RCD_IPC_SHIFT)

/* value of RxCompDesc.rssType */
#define VMXNET3_RCD_RSS_TYPE_NONE     0
#define VMXNET3_RCD_RSS_TYPE_IPV4     1
#define VMXNET3_RCD_RSS_TYPE_TCPIPV4  2
#define VMXNET3_RCD_RSS_TYPE_IPV6     3
#define VMXNET3_RCD_RSS_TYPE_TCPIPV6  4

/* a union for accessing all cmd/completion descriptors */
typedef union Vmxnet3_GenericDesc {
   uint64             qword[2];
   uint32             dword[4];
   uint16             word[8];
   Vmxnet3_TxDesc     txd;
   Vmxnet3_RxDesc     rxd;
   Vmxnet3_TxCompDesc tcd;
   Vmxnet3_RxCompDesc rcd;
} Vmxnet3_GenericDesc;

#define VMXNET3_INIT_GEN       1

/* Max size of a single tx buffer */
#define VMXNET3_MAX_TX_BUF_SIZE  (1 << 14)

/* # of tx desc needed for a tx buffer size */
#define VMXNET3_TXD_NEEDED(size) (((size) + VMXNET3_MAX_TX_BUF_SIZE - 1) / VMXNET3_MAX_TX_BUF_SIZE)

/* max # of tx descs for a non-tso pkt */
#define VMXNET3_MAX_TXD_PER_PKT 16

/* Max size of a single rx buffer */
#define VMXNET3_MAX_RX_BUF_SIZE  ((1 << 14) - 1)
/* Minimum size of a type 0 buffer */
#define VMXNET3_MIN_T0_BUF_SIZE  128
#define VMXNET3_MAX_CSUM_OFFSET  1024

/* Ring base address alignment */
#define VMXNET3_RING_BA_ALIGN   512
#define VMXNET3_RING_BA_MASK    (VMXNET3_RING_BA_ALIGN - 1)

/* Ring size must be a multiple of 32 */
#define VMXNET3_RING_SIZE_ALIGN 32
#define VMXNET3_RING_SIZE_MASK  (VMXNET3_RING_SIZE_ALIGN - 1)

/* Max ring size */
#define VMXNET3_TX_RING_MAX_SIZE   4096
#define VMXNET3_TC_RING_MAX_SIZE   4096
#define VMXNET3_RX_RING_MAX_SIZE   4096
#define VMXNET3_RC_RING_MAX_SIZE   8192

/* a list of reasons for queue stop */

#define VMXNET3_ERR_NOEOP        0x80000000  /* cannot find the EOP desc of a pkt */
#define VMXNET3_ERR_TXD_REUSE    0x80000001  /* reuse a TxDesc before tx completion */
#define VMXNET3_ERR_BIG_PKT      0x80000002  /* too many TxDesc for a pkt */
#define VMXNET3_ERR_DESC_NOT_SPT 0x80000003  /* descriptor type not supported */
#define VMXNET3_ERR_SMALL_BUF    0x80000004  /* type 0 buffer too small */
#define VMXNET3_ERR_STRESS       0x80000005  /* stress option firing in vmkernel */
#define VMXNET3_ERR_SWITCH       0x80000006  /* mode switch failure */
#define VMXNET3_ERR_TXD_INVALID  0x80000007  /* invalid TxDesc */

/* completion descriptor types */
#define VMXNET3_CDTYPE_TXCOMP      0    /* Tx Completion Descriptor */
#define VMXNET3_CDTYPE_RXCOMP      3    /* Rx Completion Descriptor */

#define VMXNET3_GOS_BITS_UNK    0   /* unknown */
#define VMXNET3_GOS_BITS_32     1
#define VMXNET3_GOS_BITS_64     2

#define VMXNET3_GOS_TYPE_UNK        0 /* unknown */
#define VMXNET3_GOS_TYPE_LINUX      1
#define VMXNET3_GOS_TYPE_WIN        2
#define VMXNET3_GOS_TYPE_SOLARIS    3
#define VMXNET3_GOS_TYPE_FREEBSD    4
#define VMXNET3_GOS_TYPE_PXE        5

/* All structures in DriverShared are padded to multiples of 8 bytes */

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_GOSInfo {
   uint32   gosBits: 2;     /* 32-bit or 64-bit? */
   uint32   gosType: 4;     /* which guest */
   uint32   gosVer:  16;    /* gos version */
   uint32   gosMisc: 10;    /* other info about gos */
} 
#include "vmware_pack_end.h"
Vmxnet3_GOSInfo;

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_DriverInfo {
   uint32          version;        /* driver version */
   Vmxnet3_GOSInfo gos;
   uint32          vmxnet3RevSpt;  /* vmxnet3 revision supported */
   uint32          uptVerSpt;      /* upt version supported */
}
#include "vmware_pack_end.h"
Vmxnet3_DriverInfo;

#define VMXNET3_REV1_MAGIC  0xbabefee1

/* 
 * QueueDescPA must be 128 bytes aligned. It points to an array of
 * Vmxnet3_TxQueueDesc followed by an array of Vmxnet3_RxQueueDesc.
 * The number of Vmxnet3_TxQueueDesc/Vmxnet3_RxQueueDesc are specified by
 * Vmxnet3_MiscConf.numTxQueues/numRxQueues, respectively.
 */
#define VMXNET3_QUEUE_DESC_ALIGN  128

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_MiscConf {
   Vmxnet3_DriverInfo driverInfo;
   uint64             uptFeatures;
   uint64             ddPA;         /* driver data PA */
   uint64             queueDescPA;  /* queue descriptor table PA */
   uint32             ddLen;        /* driver data len */
   uint32             queueDescLen; /* queue descriptor table len, in bytes */
   uint32             mtu;
   uint16             maxNumRxSG;
   uint8              numTxQueues;
   uint8              numRxQueues;
   uint32             reserved[4];
}
#include "vmware_pack_end.h"
Vmxnet3_MiscConf;

typedef
#include "vmware_pack_begin.h"
struct vmxnet3_TxQueueConf {
   uint64    txRingBasePA;
   uint64    dataRingBasePA;
   uint64    compRingBasePA;
   uint64    ddPA;         /* driver data */
   uint64    reserved;
   uint32    txRingSize;   /* # of tx desc */
   uint32    dataRingSize; /* # of data desc */
   uint32    compRingSize; /* # of comp desc */
   uint32    ddLen;        /* size of driver data */
   uint8     intrIdx;
   uint8     _pad[7];
}
#include "vmware_pack_end.h"
Vmxnet3_TxQueueConf;

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_RxQueueConf {
   uint64    rxRingBasePA[2];
   uint64    compRingBasePA;
   uint64    ddPA;            /* driver data */
   uint64    reserved;
   uint32    rxRingSize[2];   /* # of rx desc */
   uint32    compRingSize;    /* # of rx comp desc */
   uint32    ddLen;           /* size of driver data */
   uint8     intrIdx;
   uint8     _pad[7];
}
#include "vmware_pack_end.h"
Vmxnet3_RxQueueConf;

enum vmxnet3_intr_mask_mode {
   VMXNET3_IMM_AUTO   = 0,
   VMXNET3_IMM_ACTIVE = 1,
   VMXNET3_IMM_LAZY   = 2
};

enum vmxnet3_intr_type {
   VMXNET3_IT_AUTO = 0,
   VMXNET3_IT_INTX = 1,
   VMXNET3_IT_MSI  = 2,
   VMXNET3_IT_MSIX = 3
};

#define VMXNET3_MAX_TX_QUEUES  8
#define VMXNET3_MAX_RX_QUEUES  16
/* addition 1 for events */
#define VMXNET3_MAX_INTRS      25

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_IntrConf {
   Bool   autoMask;
   uint8  numIntrs;      /* # of interrupts */
   uint8  eventIntrIdx;
   uint8  modLevels[VMXNET3_MAX_INTRS]; /* moderation level for each intr */
   uint32 reserved[3];
}
#include "vmware_pack_end.h"
Vmxnet3_IntrConf;

/* one bit per VLAN ID, the size is in the units of uint32 */
#define VMXNET3_VFT_SIZE  (4096 / (sizeof(uint32) * 8))

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_QueueStatus {
   Bool    stopped;
   uint8   _pad[3];
   uint32  error;
}
#include "vmware_pack_end.h"
Vmxnet3_QueueStatus;

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_TxQueueCtrl {
   uint32  txNumDeferred;
   uint32  txThreshold;
   uint64  reserved;
}
#include "vmware_pack_end.h"
Vmxnet3_TxQueueCtrl;

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_RxQueueCtrl {
   Bool    updateRxProd;
   uint8   _pad[7];
   uint64  reserved;
}
#include "vmware_pack_end.h"
Vmxnet3_RxQueueCtrl;

#define VMXNET3_RXM_UCAST     0x01  /* unicast only */
#define VMXNET3_RXM_MCAST     0x02  /* multicast passing the filters */
#define VMXNET3_RXM_BCAST     0x04  /* broadcast only */
#define VMXNET3_RXM_ALL_MULTI 0x08  /* all multicast */
#define VMXNET3_RXM_PROMISC   0x10  /* promiscuous */

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_RxFilterConf {
   uint32   rxMode;       /* VMXNET3_RXM_xxx */
   uint16   mfTableLen;   /* size of the multicast filter table */
   uint16   _pad1;
   uint64   mfTablePA;    /* PA of the multicast filters table */
   uint32   vfTable[VMXNET3_VFT_SIZE]; /* vlan filter */
}
#include "vmware_pack_end.h"
Vmxnet3_RxFilterConf;

#define VMXNET3_PM_MAX_FILTERS        6
#define VMXNET3_PM_MAX_PATTERN_SIZE   128
#define VMXNET3_PM_MAX_MASK_SIZE      (VMXNET3_PM_MAX_PATTERN_SIZE / 8)

#define VMXNET3_PM_WAKEUP_MAGIC       0x01  /* wake up on magic pkts */
#define VMXNET3_PM_WAKEUP_FILTER      0x02  /* wake up on pkts matching filters */

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_PM_PktFilter {
   uint8 maskSize;
   uint8 patternSize;
   uint8 mask[VMXNET3_PM_MAX_MASK_SIZE];
   uint8 pattern[VMXNET3_PM_MAX_PATTERN_SIZE];
   uint8 pad[6];
}
#include "vmware_pack_end.h"
Vmxnet3_PM_PktFilter;

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_PMConf {
   uint16               wakeUpEvents;  /* VMXNET3_PM_WAKEUP_xxx */
   uint8                numFilters;
   uint8                pad[5];
   Vmxnet3_PM_PktFilter filters[VMXNET3_PM_MAX_FILTERS];
}
#include "vmware_pack_end.h"
Vmxnet3_PMConf;

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_VariableLenConfDesc {
   uint32              confVer;
   uint32              confLen;
   uint64              confPA;
}
#include "vmware_pack_end.h"
Vmxnet3_VariableLenConfDesc;

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_DSDevRead {
   /* read-only region for device, read by dev in response to a SET cmd */
   Vmxnet3_MiscConf     misc;
   Vmxnet3_IntrConf     intrConf;
   Vmxnet3_RxFilterConf rxFilterConf;
   Vmxnet3_VariableLenConfDesc  rssConfDesc;
   Vmxnet3_VariableLenConfDesc  pmConfDesc;
   uint64               reserved[2];
}
#include "vmware_pack_end.h"
Vmxnet3_DSDevRead;

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_TxQueueDesc {
   Vmxnet3_TxQueueCtrl ctrl;
   Vmxnet3_TxQueueConf conf;
   /* Driver read after a GET command */
   Vmxnet3_QueueStatus status;
   UPT1_TxStats        stats;
   uint8               _pad[88]; /* 128 aligned */
}
#include "vmware_pack_end.h"
Vmxnet3_TxQueueDesc;

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_RxQueueDesc {
   Vmxnet3_RxQueueCtrl ctrl;
   Vmxnet3_RxQueueConf conf;
   /* Driver read after a GET command */
   Vmxnet3_QueueStatus status;
   UPT1_RxStats        stats;
   uint8               _pad[88]; /* 128 aligned */
}
#include "vmware_pack_end.h"
Vmxnet3_RxQueueDesc;

typedef
#include "vmware_pack_begin.h"
struct Vmxnet3_DriverShared {
   uint32               magic;
   uint32               pad; /* make devRead start at 64-bit boundaries */
   Vmxnet3_DSDevRead    devRead;
   uint32               ecr;
   uint32               reserved[5];
}
#include "vmware_pack_end.h"
Vmxnet3_DriverShared;

#define VMXNET3_ECR_RQERR       (1 << 0)
#define VMXNET3_ECR_TQERR       (1 << 1)
#define VMXNET3_ECR_LINK        (1 << 2)
#define VMXNET3_ECR_DIC         (1 << 3)
#define VMXNET3_ECR_DEBUG       (1 << 4)

/* flip the gen bit of a ring */
#define VMXNET3_FLIP_RING_GEN(gen) ((gen) = (gen) ^ 0x1)

/* only use this if moving the idx won't affect the gen bit */
#define VMXNET3_INC_RING_IDX_ONLY(idx, ring_size) \
do {\
   (idx)++;\
   if (UNLIKELY((idx) == (ring_size))) {\
      (idx) = 0;\
   }\
} while (0)

#define VMXNET3_SET_VFTABLE_ENTRY(vfTable, vid) \
   vfTable[vid >> 5] |= (1 << (vid & 31))
#define VMXNET3_CLEAR_VFTABLE_ENTRY(vfTable, vid) \
   vfTable[vid >> 5] &= ~(1 << (vid & 31))

#define VMXNET3_VFTABLE_ENTRY_IS_SET(vfTable, vid) \
   ((vfTable[vid >> 5] & (1 << (vid & 31))) != 0)

#define VMXNET3_MAX_MTU     9000
#define VMXNET3_MIN_MTU     60

#define VMXNET3_LINK_UP         (10000 << 16 | 1)    // 10 Gbps, up
#define VMXNET3_LINK_DOWN       0
#endif /* _VMXNET3_DEFS_H_ */
