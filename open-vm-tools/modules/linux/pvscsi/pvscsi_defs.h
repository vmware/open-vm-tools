/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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

#ifndef __PVSCSI_DEFS_H__
#define __PVSCSI_DEFS_H__

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vm_basic_types.h"


/*
 * Memory mapped i/o register offsets.
 */

enum PVSCSIRegOffset {
   PVSCSI_REG_OFFSET_COMMAND        =    0x0,
   PVSCSI_REG_OFFSET_COMMAND_DATA   =    0x4,
   PVSCSI_REG_OFFSET_COMMAND_STATUS =    0x8,
   PVSCSI_REG_OFFSET_LAST_STS_0     =  0x100,
   PVSCSI_REG_OFFSET_LAST_STS_1     =  0x104,
   PVSCSI_REG_OFFSET_LAST_STS_2     =  0x108,
   PVSCSI_REG_OFFSET_LAST_STS_3     =  0x10c,
   PVSCSI_REG_OFFSET_INTR_STATUS    = 0x100c,
   PVSCSI_REG_OFFSET_INTR_MASK      = 0x2010,
   PVSCSI_REG_OFFSET_KICK_NON_RW_IO = 0x3014,
   PVSCSI_REG_OFFSET_KICK_RW_IO     = 0x4018,
};


/*
 * I/O space register offsets.
 */

enum PVSCSIIoRegOffset {
   PVSCSI_IO_REG_OFFSET_OFFSET = 0,
   PVSCSI_IO_REG_OFFSET_DATA   = 4,
};

/*
 * Configuration pages. Structure sizes are 4 byte multiples.
 */

enum ConfigPageType {
   PVSCSI_CONFIG_PAGE_CONTROLLER = 0x1958,
   PVSCSI_CONFIG_PAGE_PHY        = 0x1959,
   PVSCSI_CONFIG_PAGE_DEVICE     = 0x195a,
};

/*
 * For controller address,
 *  63                            31                           0
 * |-----------------------------|------------------------------|
 *  <--- controller constant----> <--------- All zeros --------->
 *
 * For phy address,
 *  63                            31                           0
 * |-----------------------------|------------------------------|
 *  <-- phy type constant  -----> <--------- phy num ----------->
 *
 * For device address,
 *  63                            31             15            0
 * |-----------------------------|--------------|---------------|
 * <--bus/target type constant--> <--- bus  ---> <--- target --->
 */

#define PVSCSI_CONFIG_ADDR_TYPE(addr)   HIDWORD(addr)
#define PVSCSI_CONFIG_ADDR_PHYNUM(addr) LODWORD(addr)
#define PVSCSI_CONFIG_ADDR_BUS(addr)    HIWORD(addr)
#define PVSCSI_CONFIG_ADDR_TARGET(addr) LOWORD(addr)

enum ConfigPageAddressType {
   PVSCSI_CONFIG_CONTROLLER_ADDRESS = 0x2120,
   PVSCSI_CONFIG_BUSTARGET_ADDRESS  = 0x2121,
   PVSCSI_CONFIG_PHY_ADDRESS        = 0x2122,
};

typedef
#include "vmware_pack_begin.h"
struct PVSCSIConfigPageHeader {
   uint32 pageNum;
   uint16 numDwords; /* Including the header. */
   uint16 hostStatus;
   uint16 scsiStatus;
   uint16 reserved[3];
}
#include "vmware_pack_end.h"
PVSCSIConfigPageHeader;

typedef
#include "vmware_pack_begin.h"
struct PVSCSIConfigPageController {
   PVSCSIConfigPageHeader header;
   uint64                 nodeWWN; /* Device name as defined in the SAS spec. */
   uint16                 manufacturer[64];
   uint16                 serialNumber[64];
   uint16                 opromVersion[32];
   uint16                 hwVersion[32];
   uint16                 firmwareVersion[32];
   uint32                 numPhys;
   uint8                  useConsecutivePhyWWNs;
   uint8                  reserved[3];
}
#include "vmware_pack_end.h"
PVSCSIConfigPageController;

enum AttachedDeviceType {
   PVSCSI_SAS_DEVICE    = 1,
   PVSCSI_SATA_DEVICE   = 2,
};

typedef
#include "vmware_pack_begin.h"
struct PVSCSIConfigPagePhy {
   PVSCSIConfigPageHeader header;
   uint64                 phyWWN;
   uint64                 attachedDeviceWWN; /* 0 => no attached device. */
   uint8                  attachedDeviceType;
   uint8                  reserved[7];
}
#include "vmware_pack_end.h"
PVSCSIConfigPagePhy;

typedef
#include "vmware_pack_begin.h"
struct PVSCSIConfigPageDevice {
   PVSCSIConfigPageHeader header;
   uint64                 deviceWWN;
   uint64                 phyWWN;
   uint32                 phyNum;
   uint8                  target;
   uint8                  bus;
   uint8                  reserved[2];
}
#include "vmware_pack_end.h"
PVSCSIConfigPageDevice;

/*
 * Virtual h/w commands.
 */

enum PVSCSICommands {
   PVSCSI_CMD_FIRST             = 0, /* NB: has to be first */

   PVSCSI_CMD_ADAPTER_RESET     = 1,
   PVSCSI_CMD_ISSUE_SCSI        = 2,
   PVSCSI_CMD_SETUP_RINGS       = 3,
   PVSCSI_CMD_RESET_BUS         = 4,
   PVSCSI_CMD_RESET_DEVICE      = 5,
   PVSCSI_CMD_ABORT_CMD         = 6,
   PVSCSI_CMD_CONFIG            = 7,

   PVSCSI_CMD_LAST              = 8  /* NB: has to be last */
};


/*
 * Command descriptors.
 */

struct CmdDescIssueSCSI {
   PA     reqAddr;
   PA     cmpAddr;
};

struct CmdDescResetDevice {
   uint32 target;
   uint8  lun[8];
};

struct CmdDescAbortCmd {
   uint64 context;
   uint32 target;
   uint32 _pad;
};

/*
 * Note:
 * - reqRingNumPages and cmpRingNumPages need to be power of two.
 * - reqRingNumPages and cmpRingNumPages need to be different from 0,
 * - reqRingNumPages and cmpRingNumPages need to be inferior to
 *   PVSCSI_SETUP_RINGS_MAX_NUM_PAGES.
 */

#define PVSCSI_SETUP_RINGS_MAX_NUM_PAGES        32
struct CmdDescSetupRings {
   uint32 reqRingNumPages;
   uint32 cmpRingNumPages;
   PPN64  ringsStatePPN;
   PPN64  reqRingPPNs[PVSCSI_SETUP_RINGS_MAX_NUM_PAGES];
   PPN64  cmpRingPPNs[PVSCSI_SETUP_RINGS_MAX_NUM_PAGES];
};

#include "vmware_pack_begin.h"
struct CmdDescConfigCmd {
   PA     cmpAddr;
   uint64 configPageAddress;
   uint32 configPageNum;
   uint32 _pad;
}
#include "vmware_pack_end.h"
;

/*
 * Rings state.
 */

typedef struct RingsState {
   uint32       reqProdIdx;
   uint32       reqConsIdx;
   uint32       reqNumEntriesLog2;

   uint32       cmpProdIdx;
   uint32       cmpConsIdx;
   uint32       cmpNumEntriesLog2;
} RingsState;


/*
 * Request descriptor.
 *
 * sizeof(RingReqDesc) = 128
 *
 * - context: is a unique identifier of a command. It could normally be any
 *   64bit value, however we currently store it in the serialNumber variable
 *   of struct SCSI_Command, so we have the following restrictions due to the
 *   way this field is handled in the vmkernel storage stack:
 *    * this value can't be 0,
 *    * the upper 32bit need to be 0 since serialNumber is as a uint32.
 *   Currently tracked as PR 292060.
 * - dataLen: contains the total number of bytes that need to be transferred.
 * - dataAddr:
 *   * if PVSCSI_FLAG_CMD_WITH_SG_LIST is set: dataAddr is the PA of the first
 *     s/g table segment, each s/g segment is entirely contained on a single
 *     page of physical memory,
 *   * if PVSCSI_FLAG_CMD_WITH_SG_LIST is NOT set, then dataAddr is the PA of
 *     the buffer used for the DMA transfer,
 * - flags:
 *   * PVSCSI_FLAG_CMD_WITH_SG_LIST: see dataAddr above,
 *   * PVSCSI_FLAG_CMD_DIR_NONE: no DMA involved,
 *   * PVSCSI_FLAG_CMD_DIR_TOHOST: transfer from device to main memory,
 *   * PVSCSI_FLAG_CMD_DIR_TODEVICE: transfer from main memory to device,
 *   * PVSCSI_FLAG_CMD_OUT_OF_BAND_CDB: reserved to handle CDBs larger than
 *     16bytes. To be specified.
 * - vcpuHint: vcpuId of the processor that will be most likely waiting for the
 *   completion of the i/o. For guest OSes that use lowest priority message
 *   delivery mode (such as windows), we use this "hint" to deliver the
 *   completion action to the proper vcpu. For now, we can use the vcpuId of
 *   the processor that initiated the i/o as a likely candidate for the vcpu
 *   that will be waiting for the completion..
 */

#define PVSCSI_FLAG_CMD_WITH_SG_LIST        (1 << 0)
#define PVSCSI_FLAG_CMD_OUT_OF_BAND_CDB     (1 << 1)
#define PVSCSI_FLAG_CMD_DIR_NONE            (1 << 2)
#define PVSCSI_FLAG_CMD_DIR_TOHOST          (1 << 3)
#define PVSCSI_FLAG_CMD_DIR_TODEVICE        (1 << 4)

typedef
#include "vmware_pack_begin.h"
struct RingReqDesc {
   uint64       context;
   PA           dataAddr;
   uint64       dataLen;
   PA           senseAddr;
   uint32       senseLen;
   uint32       flags;
   uint8        cdb[16];
   uint8        cdbLen;
   uint8        lun[8];
   uint8        tag;
   uint8        bus;
   uint8        target;
   uint8        vcpuHint;
   uint8        unused[59];
}
#include "vmware_pack_end.h"
RingReqDesc;


/*
 * Scatter-gather list management.
 *
 * As described above, when PVSCSI_FLAG_CMD_WITH_SG_LIST is set in the
 * RingReqDesc.flags, then RingReqDesc.dataAddr is the PA of the first s/g
 * table segment.
 *
 * - each segment of the s/g table contain a succession of struct
 *   PVSCSISGElement.
 * - each segment is entirely contained on a single physical page of memory.
 * - a "chain" s/g element has the flag PVSCSI_SGE_FLAG_CHAIN_ELEMENT set in
 *   PVSCSISGElement.flags and in this case:
 *     * addr is the PA of the next s/g segment,
 *     * length is undefined, assumed to be 0.
 */

#define PVSCSI_MAX_NUM_SG_ENTRIES_PER_SEGMENT       128

/*
 * MAX_CHAIN_SEGMENTS could probably be much smaller, but if the guest takes
 * more than 128 pages to give us the SG list, then the guest is pretty clearly
 * broken.
 */

#define PVSCSI_MAX_NUM_SG_SEGMENTS       128
#define PVSCSI_SGE_FLAG_CHAIN_ELEMENT   (1 << 0)

typedef struct PVSCSISGElement {
   PA           addr;
   uint32       length;
   uint32       flags;
} PVSCSISGElement;


/*
 * Completion descriptor.
 *
 * sizeof(RingCmpDesc) = 32
 *
 * - context: identifier of the command. The same thing that was specified
 *   under "context" as part of struct RingReqDesc at initiation time,
 * - dataLen: number of bytes transferred for the actual i/o operation,
 * - senseLen: number of bytes written into the sense buffer,
 * - hostStatus: adapter status,
 * - scsiStatus: device status,
 */

typedef struct RingCmpDesc {
   uint64     context;
   uint64     dataLen;
   uint32     senseLen;
   uint16     hostStatus;
   uint16     scsiStatus;
   uint32     _pad[2];
} RingCmpDesc;


/*
 * Interrupt status / IRQ bits.
 */

#define PVSCSI_INTR_CMPL_0      (1 << 0)
#define PVSCSI_INTR_CMPL_1      (1 << 1)
#define PVSCSI_INTR_CMPL_MASK   MASK(2)

#define PVSCSI_INTR_ALL         PVSCSI_INTR_CMPL_MASK
#define PVSCSI_MAX_INTRS        24


/*
 * Enumeration of supported MSI-X vectors
 */
#define PVSCSI_VECTOR_COMPLETION   0


/*
 * Misc constants for the rings.
 */

#define PVSCSI_MAX_NUM_PAGES_REQ_RING   PVSCSI_SETUP_RINGS_MAX_NUM_PAGES
#define PVSCSI_MAX_NUM_PAGES_CMP_RING   PVSCSI_SETUP_RINGS_MAX_NUM_PAGES

#define PVSCSI_MAX_NUM_REQ_ENTRIES_PER_PAGE   (PAGE_SIZE / sizeof(RingReqDesc))
#define PVSCSI_MAX_NUM_CMP_ENTRIES_PER_PAGE   (PAGE_SIZE / sizeof(RingCmpDesc))

#define PVSCSI_MAX_REQ_QUEUE_DEPTH \
   (PVSCSI_MAX_NUM_PAGES_REQ_RING * PVSCSI_MAX_NUM_REQ_ENTRIES_PER_PAGE)
#define PVSCSI_MAX_CMP_QUEUE_DEPTH \
   (PVSCSI_MAX_NUM_PAGES_CMP_RING * PVSCSI_MAX_NUM_CMP_ENTRIES_PER_PAGE)
#define PVSCSI_MAX_QUEUE_DEPTH \
   MAX(PVSCSI_MAX_REQ_QUEUE_DEPTH, PVSCSI_MAX_CMP_QUEUE_DEPTH)

/*
 * Misc constants related to the BARs.
 */

#define PVSCSI_NUM_REGS              7
#define PVSCSI_NUM_IO_REGS           2

/*
 * The following only gives a functional number if the result is a power of
 * two.
 */
#define PVSCSI_IO_SPACE_MASK              (PVSCSI_NUM_IO_REGS * sizeof(uint32) - 1)

#define PVSCSI_MEM_SPACE_COMMAND_NUM_PAGES     1
#define PVSCSI_MEM_SPACE_INTR_STATUS_NUM_PAGES 1
#define PVSCSI_MEM_SPACE_MISC_NUM_PAGES        2
#define PVSCSI_MEM_SPACE_KICK_IO_NUM_PAGES     2
#define PVSCSI_MEM_SPACE_MSIX_NUM_PAGES        2

#define PVSCSI_MEM_SPACE_COMMAND_PAGE          0
#define PVSCSI_MEM_SPACE_INTR_STATUS_PAGE      1
#define PVSCSI_MEM_SPACE_MISC_PAGE             2
#define PVSCSI_MEM_SPACE_KICK_IO_PAGE          4
#define PVSCSI_MEM_SPACE_MSIX_TABLE_PAGE       6
#define PVSCSI_MEM_SPACE_MSIX_PBA_PAGE         7


#define PVSCSI_MEM_SPACE_NUM_PAGES \
   (PVSCSI_MEM_SPACE_COMMAND_NUM_PAGES +       \
    PVSCSI_MEM_SPACE_INTR_STATUS_NUM_PAGES +   \
    PVSCSI_MEM_SPACE_MISC_NUM_PAGES +          \
    PVSCSI_MEM_SPACE_KICK_IO_NUM_PAGES +       \
    PVSCSI_MEM_SPACE_MSIX_NUM_PAGES)

#define PVSCSI_MEM_SPACE_SIZE        (PVSCSI_MEM_SPACE_NUM_PAGES * PAGE_SIZE)
#define PVSCSI_MEM_SPACE_MASK        (CONST64U(PVSCSI_MEM_SPACE_SIZE - 1))

/*
 * For simplicity of implementation, the MSI-X array is combined into
 * the single 64-bit memory BAR; these values are used to initialize the
 * MSI-X capability PCIe field.
 */
#define PVSCSI_MSIX_TABLE_OFF      (PVSCSI_MEM_SPACE_MSIX_TABLE_PAGE * PAGE_SIZE)
#define PVSCSI_MSIX_PBA_OFF        (PVSCSI_MEM_SPACE_MSIX_PBA_PAGE * PAGE_SIZE)
#define PVSCSI_MSIX_BIR            1

#endif /* __PVSCSI_DEFS_H__ */
