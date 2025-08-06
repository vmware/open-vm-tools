/*********************************************************
 * Copyright (c) 2000-2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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
 * acpi_table_defs.h --
 *
 *      ACPI specification
 */

#ifndef _ACPI_TABLE_DEFS_H_
#define _ACPI_TABLE_DEFS_H_

#include "vm_basic_defs.h"
#include "vm_atomic.h"

/*
 * Root System Description Pointer
 * Floating structure pointing to the actual table
 */
#define RSDP_SIGNATURE        "RSD PTR "
#define RSDP_HI_WINDOW_BASE   0xE0000
#define RSDP_HI_WINDOW_SIZE   0x20000
#define RSDP_PARAGRAPH_SIZE   16
#define RSDP_CHECKSUM_LENGTH  20
#define RSDP_REVISION_ACPI10  0
#define RSDP_REVISION_ACPI20  2

#pragma pack(push, 1)
typedef struct {
   char   signature[8];          // "RSD PTR "
   uint8  checksum;              // checksum of ACPI1.0 part (makes sum 0)
   char   oemId[6];              // OEM ID
   uint8  revision;              // Specification version
   uint32 rsdtAddress;           // Physical address of RSDT
   uint32 length;                // Length of this whole structure
   uint64 xsdtAddress;           // Physical address of XSDT
   uint8  extendedChecksum;      // checksum of this structure (makes sum 0)
   uint8  reserved[3];
} ACPITableRSDP;
#pragma pack(pop)

/*
 * Header common to all Description Tables
 */
#pragma pack(push, 1)
typedef struct {
   char   signature[4];          // identifies type of table
   uint32 length;                // length of table, in bytes, including header
   uint8  revision;              // specification minor version #
   uint8  checksum;              // to make sum of entire table == 0
   char   oemId[6];              // OEM identification
   char   oemTableId[8];         // OEM table identification
   uint32 oemRevision;           // OEM revision number
   char   aslCreatorId[4];       // ASL creator vendor ID
   uint32 aslCreatorRevision;    // ASL creator revision number
} ACPITableHeader;
#pragma pack(pop)

/*
 * Generic Address Structure
 */
#pragma pack(push, 1)
typedef struct {
   uint8  spaceID;
   uint8  bitWidth;
   uint8  bitOffset;
   uint8  accessWidth;
   uint64 address;
} ACPIAddress;
#pragma pack(pop)

enum acpi_space_id {
   ACPI_SPACE_MEMORY     = 0,
   ACPI_SPACE_IO         = 1,
   ACPI_SPACE_PCI        = 2,
   ACPI_SPACE_EMBEDDED   = 3,
   ACPI_SPACE_SMBUS      = 4,
   ACPI_SPACE_PCC        = 10,
   ACPI_SPACE_FUNCTIONAL = 0x7f,
};

enum acpi_access_width {
   ACPI_ACCESS_WIDTH_UNDEF = 0,
   ACPI_ACCESS_WIDTH_BYTE = 1,
   ACPI_ACCESS_WIDTH_WORD = 2,
   ACPI_ACCESS_WIDTH_DWORD = 3,
   ACPI_ACCESS_WIDTH_QWORD = 4,
};

/*
 * Root System Description Table
 */
#define RSDT_SIGNATURE     "RSDT"
#define RSDT_MAX_DTS       20

#pragma pack(push, 1)
typedef struct {
   ACPITableHeader   header;         // Signature is "RSDT"
   uint32            entries[1];     // Array of addresses of descript. tables
} ACPITableRSDT;
#pragma pack(pop)

/*
 * Extended System Description Table
 */
#define XSDT_SIGNATURE     "XSDT"

#pragma pack(push, 1)
typedef struct {
   ACPITableHeader   header;         // Signature is "XSDT"
   uint64            entries[1];     // Array of addresses of descript. tables
} ACPITableXSDT;
#pragma pack(pop)

/*
 * Multiple APIC Description Table
 */
#define MADT_SIGNATURE     "APIC"
#define MADT_PCAT_COMPAT   0x1       // PC-AT dual 8259 PIC present

#pragma pack(push, 1)
typedef struct {
   ACPITableHeader   header;           // Signature is "APIC"
   uint32            localApicAddress; // Address where local APIC is seen
   uint32            flags;
   uint8             entries[1];       // Array of variable-length structures
} ACPITableMADT;
#pragma pack(pop)

/*
 * Header common to all entries in some Description Table
 */
#pragma pack(push, 1)
typedef struct {
   uint8 type;
   uint8 length;
} ACPIDTEntryHeader;
#pragma pack(pop)

/*
 * Possible entries for MADT
 */
#define MADT_ENABLED 1

enum {
   ACPI_MADT_LAPIC = 0,
   ACPI_MADT_IOAPIC,
   ACPI_MADT_INT,
   ACPI_MADT_NMI,
   ACPI_MADT_LAPIC_NMI,
   ACPI_MADT_LAPIC_ADDR_OVR,
   ACPI_MADT_IOSAPIC,
   ACPI_MADT_LSAPIC,
   ACPI_MADT_PLATFORM_INT,
   ACPI_MADT_LX2APIC,
   ACPI_MADT_LX2APIC_NMI,
   ACPI_MADT_GIC,
   ACPI_MADT_GICD,
   ACPI_MADT_GIC_MSI,
   ACPI_MADT_GICR,
   ACPI_MADT_ITS,
   ACPI_MADT_MPWK,
   ACPI_MADT_OEM_80 = 0x80,
};

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint8             acpiId;        // The processor's ACPI ID
   uint8             id;            // The processor's local APIC ID
   uint32            flags;         // MADT_ENABLED
} ACPIMADTLocalApic;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint8             id;            // I/O APIC ID
   uint8             reserved;      // 0
   uint32            address;       // 32-bit physical address of this I/O APIC
   uint32            globalIntBase; // Global interrupt base for this I/O APIC
} ACPIMADTIOApic;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint8             bus;           // 0 Constant, meaning ISA
   uint8             sourceIRQ;     // Bus-relative interrupt source (IRQ)
   uint32            globalInt;     // Global System interrupt for this bus IRQ
   uint16            flags;         // MPS_POLARITY_xx, MPS_TRIGGER_xx
} ACPIMADTIntrSrcOverride;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint16            flags;         // MPS_POLARITY_xx, MPS_TRIGGER_xx
   uint32            globalInt;     // Global system interrupt for this NMI
} ACPIMADTNMISrc;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint8             acpiId;        // The processor's ACPI ID, or 0xff for all
   uint16            flags;         // MPS_POLARITY_xx, MPS_TRIGGER_xx
   uint8             lint;          // Local APIC interrupt for this NMI
} ACPIMADTLocalApicNMI;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint16            reserved;      // Reserved - Must be set to zero
   uint64            address;       // Physical address of Local APIC
} ACPIMADTLocalApicAddrOverride;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint8             id;            // I/O SAPIC ID
   uint8             reserved;      // Reserved - Must be zero
   uint32            globalIntBase; // Global interrupt base for this I/O SAPIC
   uint64            address;       // 64-bit physical address of this I/O SAPIC
} ACPIMADTIOSApic;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint8             acpiId;        // The processor's ACPI ID
   uint8             id;            // The processor’s local SAPIC ID
   uint8             EId;           // The processor’s local SAPIC EID
   uint8             reserved[3];   // Reserved - Must be zero
   uint32            flags;         // MADT_ENABLED
   uint32            uidValue;      // ACPI Processor UID value
   uint8             uidString[1];  // ACPI Processor UID string
} ACPIMADTLocalSApic;
#pragma pack(pop)

#define MADT_PMI                          1
#define MADT_INIT                         2
#define MADT_CORRECTED_PLATFORM_ERROR_INT 3

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint16            flags;         // MPS_POLARITY_xx, MPS_TRIGGER_xx
   uint8             type;          // MADT_PMI, MADT_INIT, MADT_CORR...
   uint8             destId;        // Processor ID of destination
   uint8             destEId;       // Processor EID of destination
   uint8             ioSApicVector; // entry in the I/O SAPIC redirection table
   uint32            globalInt;     // Global System interrupt for this PMI
   uint32            IntrSrcFlags;  // Platform interrupt source flags
} ACPIMADTPlatformIntrSrc;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint16            reserved;      // Reserved - Must be zero
   uint32            id;            // x2APIC ID
   uint32            flags;         // Same as Local APIC
   uint32            acpiId;        // ACPI processor UID for this x2APIC
} ACPIMADTLocalX2Apic;
#pragma pack(pop)

/*
 * arm64 Generic Interrupt Controller (GIC).
 */
#define MADT_GIC_PMU_EDGE  (1 << 1)
#define MADT_GIC_VGIC_EDGE (1 << 2)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint16            reserved;      // Reserved - Must be zero
   uint32            id;            // The local GIC's hardware ID.
   uint32            acpiId;        // ACPI processor UID for this GIC.
   uint32            flags;         // GIC flags.
   uint32            parkingVers;   // Parking protocol implemented.
   uint32            pmuGsiv;       // GSIV for PMU interrupts.
   uint64            mailBox;       // Parking protocol mailbox.
   uint64            base;          // GIC base
   uint64            vgicBase;      // VGIC base
   uint64            gichBase;      // GICH
   uint32            gichGsiv;      // GSIV for GICH maintenance interrupt
   uint64            gicrBase;      // GICv3 and later.
   uint64            mpidr;         // Actual MPIDR value
} ACPIMADTGIC;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint16            reserved;      // Reserved - Must be zero
   uint32            id;            // The GIC distributor's hardware ID.
   uint64            base;          // GIC distributor base.
   uint32            globalIntBase; // System vector base.
   uint8             version;       // GIC version.
   uint8             reserved2[3];  // Must be zero.
} ACPIMADTGICD;
#pragma pack(pop)

/*
 * arm64 Generic Interrupt Controller (GIC) MSI frame.
 */
#define MADT_GIC_MSI_OVERRIDE (1 << 0)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint16            reserved;      // Reserved - Must be zero
   uint32            id;            // MSI frame ID.
   uint64            base;          // MSI frame base.
   uint32            flags;         // MSI frame flags.
   uint16            spiCount;      // SPI count.
   uint16            spiBase;       // SPI base.
} ACPIMADTGICMSI;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint16            reserved;      // Reserved - Must be zero
   uint64            base;          // all GIC re-distributor base.
   uint32            length;        // all GIC re-distributor length.
} ACPIMADTGICR;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint16            reserved;      // Reserved - Must be zero
   uint32            id;            // GIC ITS ID.
   uint64            base;          // GIC ITS base.
   uint32            reserved1;     // Reserved - Must be zero
} ACPIMADTITS;
#pragma pack(pop)

/*
 * Multiprocessor Wakeup (MPWK)
 */
#define MADT_MPWK_MAILBOX_VERSION 0

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint16            mailboxVers;   // Mailbox version (0 in current spec)
   uint32            reserved;      // Reserved - Must be zero
   uint64            mailboxAddr;   // Physical address of mailbox
} ACPIMADTMPWK;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   Atomic_uint16     command;       // Command (see enum below)
   uint16            reserved;      // Reserved - Must be zero
   Atomic_uint32     apicId;        // Target processor's local APIC ID
   Atomic_uint64     wakeupVector;  // Wakeup address for AP
   uint8             osRsvd[2032];  // Reserved for OS use
   uint8             fwRsvd[2048];  // Reserved for firmware use
} ACPIMPWKMailbox;
#pragma pack(pop)

MY_ASSERTS(ACPIMPWK_ASSERTS,
   ASSERT_ON_COMPILE(sizeof (ACPIMPWKMailbox) == 4096);
)

enum {
   ACPI_MPWK_COMMAND_NOOP   = 0,
   ACPI_MPWK_COMMAND_WAKEUP = 1
};

/*
 * Static Resource Affinity Table
 */
#define SRAT_SIGNATURE   "SRAT"

#pragma pack(push, 1)
typedef struct {
   ACPITableHeader   header;        // Signature is "SRAT"
   uint8             unknown[12];
   uint8             entries[1];    // Array of variable-length structures
} ACPITableSRAT;
#pragma pack(pop)

/*
 * Possible entries for SRAT
 */
enum {
   SRAT_PROC = 0,
   SRAT_MEM,
   SRAT_PROC_X2APIC,
   SRAT_PROC_GICC,
   SRAT_GIC_ITS_AFFINITY,
   SRAT_GENERIC_INITIATOR,
};

#define SRAT_ENABLED            (1 << 0)
#define SRAT_HOT_PLUGGABLE      (1 << 1)
#define SRAT_NON_VOLATILE       (1 << 2)


#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint8             nodeIdLo;      // Bit[7:0] Proximity domain for the logical proc
   uint8             apicId;        // The processor local (S)APIC ID
   uint32            flags;         // SRAT_ENABLED
   uint8             localSapicEid; // The processor local (S)APIC EID
   uint8             nodeIdHi[3];   // Bit[31:8] Proximity domain for the logical proc
   uint32            clockId;       // Clock domain for the logical processor
} ACPISRATProcApic;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint16            reserved;      // Reserved - Must be zero
   uint32            nodeId;        // Proximity domain for the logical proc
   uint32            x2ApicId;      // The processor local x2APIC ID
   uint32            flags;         // Flags
   uint32            clockId;       // Clock domain for the logical processor
   uint8             unknown[4];    // Reserved
} ACPISRATProcX2Apic;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint32            nodeId;       // Proximity domain for the logical proc
   uint32            acpiId;       // Processor UID (as per ACPI)
#define SRAT_GICC_ENABLED (1 << 0) // If clear, OSPM ignores this SRAT structure
   uint32            flags;
   uint32            clockId;      // Clock domain for the logical processor
} ACPISRATProcGICC;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint32            nodeId;       // Proximity domain for the GIC ITS
   uint16            unknown;
   uint32            itsId;        // The ITS ID
} ACPISRATGICITSAffinity;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader header;
   uint32            nodeId;       // Proximity domain for the logical proc
   uint8             unknown[2];
   uint64            base;         // Base address of the memory range
   uint64            length;       // Size of the memory range
   uint8             unknown2[4];  // Reserved
   uint32            flags;        // Flags
   uint8             unknown3[8];  // Reserved
} ACPISRATMemory;
#pragma pack(pop)

/*
 * SRAT Generic Initiator Affinity Structure.
 */

// Supported generic initiator device types
enum AcpiGenericInitiatorType {
   SRAT_GENERIC_INITIATOR_TYPE_ACPI = 0,
   SRAT_GENERIC_INITIATOR_TYPE_PCI  = 1,
   SRAT_GENERIC_INITIATOR_TYPE_INVALID = 0xff,
};

// ACPI device handle of generic initiator.
#pragma pack(push, 1)
typedef struct {
   uint64 hid;        // ACPI _HID
   uint32 uid;        // ACPI _UID
   uint8 reserved[4]; // Reserved
} ACPIGiasAcpiHandle;
#pragma pack(pop)

// PCI device handle of generic initiator.
#pragma pack(push, 1)
typedef struct {
   uint16 seg;          // S: segment
   uint16 bus:8;        // B: bus
   uint16 dev:5;        // D: device
   uint16 fun:3;        // F: function
   uint8  reserved[12]; // Reserved
} ACPIGiasPCIHandle;
#pragma pack(pop)

// Common data structure for device handle.
#pragma pack(push, 1)
typedef union {
   ACPIGiasAcpiHandle acpi; // ACPI device handle
   ACPIGiasPCIHandle  pci;  // PCI device handle
} ACPIGiasDeviceHandle;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   ACPIDTEntryHeader       header;
   uint8                   reserved1;    // Reserved
   uint8                   deviceType;   // Device handle type
                                         //    - 0: ACPI device
                                         //    - 1: PCI device
   uint32                  pxmID;        // Proximity domain which generic
                                         // initiator belongs to
   ACPIGiasDeviceHandle deviceHandle;    // ACPI/PCI device handle
#define SRAT_GENERIC_INITIATOR_ENABLED (1 << 0)
   uint32                  flags;        // Flags
   uint8                   reserved2[4]; // Reserved
} ACPISRATGenericInitiator;
#pragma pack(pop)

/*
 * High Precision Event Timers Table
 */
#define HPET_SIGNATURE          "HPET"

#pragma pack(push, 1)
typedef struct {
   ACPITableHeader   header;       // Signature is "HPET"
   uint32            blockId;
   uint32            acpiAddr;
   uint64            baseAddr;
   uint8             number;
   uint16            minTick;
   uint8             attributes;
} ACPITableHPET;
#pragma pack(pop)

/*
 * Fixed ACPI Description Table
 */

#define FADT_SIGNATURE          "FACP"
#define FADT_REVISION_ACPI10    1
#define FADT_REVISION_ACPI20    3
#define FADT_REVISION_ACPI30    4
#define FADT_REVISION_ACPI40    4
#define FADT_REVISION_ACPI50    5
#define FADT_REVISION_ACPI60    6

#pragma pack(push, 1)
typedef struct {
   ACPITableHeader   header;       // Signature is "FACP"
   uint32            facsAddr;
   uint32            dsdtAddr;
   uint8             reserved1;
   uint8             preferredPmProfile;
   uint16            sciInt;
   uint32            smiCmd;
   uint8             acpiEnable;
   uint8             acpiDisable;
   uint8             s4biosReq;
   uint8             pstateCnt;
   uint32            pm1aEvtBlk;
   uint32            pm1bEvtBlk;
   uint32            pm1aCntBlk;
   uint32            pm1bCntBlk;
   uint32            pm2CntBlk;
   uint32            pmTmrBlk;
   uint32            gpe0Blk;
   uint32            gpe1Blk;
   uint8             pm1EvtLen;
   uint8             pm1CntLen;
   uint8             pm2CntLen;
   uint8             pmTmrLen;
   uint8             gpe0BlkLen;
   uint8             gpe1BlkLen;
   uint8             gpe1Base;
   uint8             cstCnt;
   uint16            pLvl2Lat;
   uint16            pLvl3Lat;
   uint16            flushSize;
   uint16            flushStride;
   uint8             dutyOffset;
   uint8             dutyWidth;
   uint8             dayAlrm;
   uint8             monAlrm;
   uint8             century;
   uint16            iapcBootArch;
   uint8             reserved2;
   uint32            flags;
   ACPIAddress       resetReg;
   uint8             resetValue;
   uint16            armBootArch;
   uint8             minorVersion;
   uint64            xFacsAddr;
   uint64            xDsdtAddr;
   ACPIAddress       xPm1aEvtBlk;
   ACPIAddress       xPm1bEvtBlk;
   ACPIAddress       xPm1aCntBlk;
   ACPIAddress       xPm1bCntBlk;
   ACPIAddress       xPm2CntBlk;
   ACPIAddress       xPmTmrBlk;
   ACPIAddress       xGpe0Blk;
   ACPIAddress       xGpe1Blk;
   ACPIAddress       sleepCtlReg;
   ACPIAddress       sleepStsReg;
   uint64            hypervisorVendor;
} ACPITableFADT;
#pragma pack(pop)

enum acpi_fadt_flags {
   FADT_WBINVD                               = 0x00000001,
   FADT_WBINVD_FLUSH                         = 0x00000002,
   FADT_PROC_C1                              = 0x00000004,
   FADT_P_LVL2_UP                            = 0x00000008,
   FADT_PWR_BUTTON                           = 0x00000010,
   FADT_SLP_BUTTON                           = 0x00000020,
   FADT_FIX_RTC                              = 0x00000040,
   FADT_RTC_S4                               = 0x00000080,
   FADT_TMR_VAL_EXT                          = 0x00000100,
   FADT_DCK_CAP                              = 0x00000200,
   FADT_RESET_REG_SUP                        = 0x00000400,
   FADT_SEALED_CASE                          = 0x00000800,
   FADT_HEADLESS                             = 0x00001000,
   FADT_CPU_SW_SLP                           = 0x00002000,
   FADT_PCI_EXP_WAK                          = 0x00004000,
   FADT_USE_PLATFORM_CLOCK                   = 0x00008000,
   FADT_S4_RTS_STS_VALID                     = 0x00010000,
   FADT_REMOTE_POWER_ON_CAPABLE              = 0x00020000,
   FADT_FORCE_APIC_CLUSTER_MODE              = 0x00040000,
   FADT_FORCE_APIC_PHYSICAL_DESTINATION_MODE = 0x00080000,
   FADT_HW_REDUCED                           = 0x00100000,
   FADT_LOW_POWER_S0                         = 0x00200000,
};

/*
 * ACPI FADT register block types
 */
typedef enum {
   ACPI_FADT_REG_BLK_FIRST = 0,
   ACPI_FADT_PM1A_EVT = 0,
   ACPI_FADT_PM1B_EVT,
   ACPI_FADT_PM1A_CNT,
   ACPI_FADT_PM1B_CNT,
   ACPI_FADT_PM2_CNT,
   ACPI_FADT_PM_TMR,
   ACPI_FADT_GPE0,
   ACPI_FADT_GPE1,
   ACPI_FADT_RESET_REG,
   ACPI_FADT_SLEEP_CTL,
   ACPI_FADT_SLEEP_STS,
   ACPI_FADT_REG_BLK_LAST = ACPI_FADT_SLEEP_STS
} AcpiFadtRegBlkType;


#define DSDT_SIGNATURE "DSDT"

#pragma pack(push, 1)
typedef struct {
   ACPITableHeader   header;
   uint8             data[1]; // We don't need the exact format
} ACPITableDSDT;
#pragma pack(pop)

/*
 * Embedded controller boot resources table
 */
#define ECDT_SIGNATURE "ECDT"

#pragma pack(push, 1)
typedef struct {
   ACPITableHeader   header;
   ACPIAddress       ecControl;
   ACPIAddress       ecData;
   uint32            uid;
   uint8             gpeBit;
   uint8             ecId[1];    // variable length, null-terminated string
} ACPITableECDT;
#pragma pack(pop)

/*
 * NVIDIA BlueField TMFIFO Table (TMFF), used to discover the remote shim-based
 * console in place of PL011.
 */
#define TMFF_SIGNATURE "TMFF"

#pragma pack(push, 1)
typedef struct {
   ACPITableHeader header;
   uint64          base;
   uint64          size;
#define TMFIFO_CON_OVERRIDES_SPCR_FOR_EARLY_CONSOLE 0x1
#define TMFIFO_CON_OVERRIDES_DBG2                   0x2
#define TMFIFO_NET_OVERRIDES_DBG2                   0x4
   uint64          flags;
} ACPITableTMFF;
#pragma pack(pop)

/*
 * Serial Port Console Redirection Table (SPCR)
 */ 
#define SPCR_SIGNATURE  "SPCR"

#pragma pack(push, 1)
typedef struct {
   ACPITableHeader header;
   uint8           interfaceType;
   uint8           reserved1[3];
   ACPIAddress     portAddr;
   uint8           interruptType;
   uint8           irq;
   uint32          globalSysInterrupt;
   uint8           baudRate;
   uint8           parity;
   uint8           stopBits;
   uint8           flowControl;
   uint8           terminalType;
   uint8           reserved2;
   uint16          pciVid;
   uint16          pciDid;
   uint8           pciBus;
   uint8           pciDev;
   uint8           pciFunc;
   uint32          pciFlags;
   uint8           pciSeg;
   uint32          reserved3[4];
} ACPITableSPCR;
#pragma pack(pop)

/*
 * interfaceType field definitions
 */
#define ACPI_SPCR_16550             0x0
#define ACPI_SPCR_16550_DBGP_SUBSET 0x1
#define ACPI_SPCR_MAX311xE          0x2
#define ACPI_SPCR_PL011             0x3
#define ACPI_SPCR_MSM8x60           0x4
#define ACPI_SPCR_NVIDIA_16550      0x5
#define ACPI_SPCR_TI_OMAP           0x6
#define ACPI_SPCR_APM88xxxx         0x8
#define ACPI_SPCR_MSM8974           0x9
#define ACPI_SPCR_SAM5250           0xa
#define ACPI_SPCR_INTEL_USIF        0xb
#define ACPI_SPCR_IMX6              0xc
#define ACPI_SPCR_SBSA_32BIT        0xd
#define ACPI_SPCR_SBSA              0xe
#define ACPI_SPCR_ARM_DCC           0xf
#define ACPI_SPCR_BCM2835           0x10
#define ACPI_SPCR_SDM845_18432      0x11
/*
 * This is a DBG2 type inherited by SPCR, but all SPCR-described
 * ports must correctly fill the ACPI Generic Address Structure
 * covering the controller registers.
 */
#define ACPI_SPCR_16550_HONOR_GAS   0x12
#define ACPI_SPCR_SDM845_7372       0x13
#define ACPI_SPCR_INTEL_LPSS        0x14

/*
 * interruptType field definitions
 */
#define ACPI_SPCR_INT_PIC     0x1
#define ACPI_SPCR_INT_IOAPIC  0x2
#define ACPI_SPCR_INT_IOSAPIC 0x4
#define ACPI_SPCR_INT_GIC     0x8

/*
 * Trusted Computing Platform Alliance (TCPA) table for TPM 1.2.
 *
 * See TCG ACPI Specification, Family "1.2" and "2.0", Level 00 Revision
 * 00.37, December 19, 2014.
 */
#define TCPA_SIGNATURE "TCPA"

#pragma pack(push, 1)
typedef struct {
   ACPITableHeader header;
   uint16          platformClass;
   union {
      struct {
         uint32      logAreaMinLength;
         uint64      logAreaAddress;
      } client;
      struct {
         uint16      reserved1;
         uint64      logAreaMinLength;
         uint64      logAreaAddress;
         uint16      specificationRevision;
         uint8       deviceFlags;
         uint8       interruptFlags;
         uint8       gpe;
         uint8       reserved2[3];
         uint32      globalSystemInterrupt;
         ACPIAddress baseAddress;
         uint32      reserved3;
         ACPIAddress configurationAddress;
         uint16      pciSegmentGroupNumber;
         uint8       pciBusNumber;
         uint8       pciDeviceNumber;
         uint8       pciFunctionNumber;
      } server;
   } platform;
} ACPITableTCPA;
#pragma pack(pop)

/* Possible entries for the TCPA platform class. */
enum {
   TCPA_PLATFORM_PC_CLIENT = 0x0,
   TCPA_PLATFORM_SERVER = 0x1,
};

/*
 * ACPI table for TPM 2.
 *
 * There are two versions of this table. Later versions of the spec
 * added the address of a copy of the TCG event log.
 */
#define TPM2_SIGNATURE "TPM2"

/*
 * See TCG ACPI Specification, Family "1.2" and "2.0", Level 00 Revision
 * 00.37, December 19, 2014, Table 7: TCG Hardware Interface Description
 * Table for TPM 2.0.
 */
#pragma pack(push, 1)
typedef struct {
   ACPITableHeader header;
   uint16          platformClass;
   uint16          reserved;
   uint64          controlArea;
   uint32          startMethod;
   uint8           platformSpecific[];
} ACPITableTPM2;
#pragma pack(pop)

/*
 * See TCG ACPI Specification, Family "1.2" and "2.0", Version 1.2,
 * Revision 8, August 18, 2017, Table 7: TCG Hardware Interface
 * Description Table for TPM 2.0.
 */
#pragma pack(push, 1)
typedef struct {
   ACPITableHeader header;
   uint16          platformClass;
   uint16          reserved;
   uint64          controlArea;
   uint32          startMethod;
   uint8           platformSpecific[12];  // Up to 12 bytes
   uint32          logAreaMinLength;      // Optional
   uint64          logAreaStartAddr;      // Optional
} ACPITableTPM2Ex;
#pragma pack(pop)

/* Possible entries of the 'startMethod' field in the TPM2 table. */
typedef enum {
   ACPI_TPM2_START_METHOD_INVALID = 0,
   ACPI_TPM2_START_METHOD_LEGACY1 = 1,
   ACPI_TPM2_START_METHOD_ACPI = 2,
   ACPI_TPM2_START_METHOD_LEGACY3 = 3,
   ACPI_TPM2_START_METHOD_LEGACY4 = 4,
   ACPI_TPM2_START_METHOD_LEGACY5 = 5,
   ACPI_TPM2_START_METHOD_MMIO_TIS = 6,
   ACPI_TPM2_START_METHOD_CRB = 7,
   ACPI_TPM2_START_METHOD_CRB_WITH_ACPI = 8,
   ACPI_TPM2_START_METHOD_LEGACY9 = 9,
   ACPI_TPM2_START_METHOD_LEGACY10 = 10,
   ACPI_TPM2_START_METHOD_CRB_WITH_ARM_SMC = 11,
} AcpiTpm2StartMethod;

/* Possible entries for the 'platformClass' field in the TPM2 table. */
typedef enum {
   ACPI_TPM2_PLATFORM_PC_CLIENT = 0x0,
   ACPI_TPM2_PLATFORM_SERVER = 0x1,
} AcpiTpm2PlatformClass;

#define IBFT_SIGNATURE1 "iBFT"
#define IBFT_SIGNATURE2 "IBFT"
#define MPST_SIGNATURE  "MPST"
#define SLIC_SIGNATURE  "SLIC"
#define DMAR_SIGNATURE  "DMAR"

/*
 * Special values used in the ACPI SLIT table.
 *
 * Based on the ACPI documentation (Revision 4.0, June 16, 2009 ),
 * section 5.2.17.
 */
#define ACPI_SLIT_LOCAL_DISTANCE (10)
#define ACPI_SLIT_UNREACHABLE (255)
#define ACPI_SLIT_LOWEST_VALID_DISTANCE (ACPI_SLIT_LOCAL_DISTANCE)
#define ACPI_SLIT_HIGHEST_VALID_DISTANCE (ACPI_SLIT_UNREACHABLE - 1)
#define ACPI_SLIT_INVALID (ACPI_SLIT_UNREACHABLE + 1)

/*
 * arm64 Generic Timer Description Table (GTDT).
 */
#define GTDT_SIGNATURE "GTDT"

#pragma pack(push, 1)
typedef struct {
   ACPITableHeader header;
   uint64          address;
   uint32          flags;
   uint32          secureEL1Gsiv;
   uint32          secureEL1Flags;
   uint32          nonSecureEL1Gsiv;
   uint32          nonSecureEL1Flags;
   uint32          virtualEL1Gsiv;
   uint32          virtualEL1Flags;
   uint32          nonSecureEL2Gsiv;
   uint32          nonSecureEL2Flags;
   uint64          cntReadBase;
   uint32          platformTimerCount;
   uint32          platformTimerOffset;
   uint32          virtualEL2Gsiv;
   uint32          virtualEL2Flags;
   uint8           platformTimers[0];
} ACPITableGTDT;
#pragma pack(pop)

/* For flags field. */
#define ACPI_GTDT_MAPPED_BLOCK_PRESENT 1

/* For ACPITableGTDT *Flags */
#define ACPI_GTDT_TRIGGER_MASK         1
#define ACPI_GTDT_TRIGGER_LEVEL        0
#define ACPI_GTDT_TRIGGER_EDGE         1
#define ACPI_GTDT_POLARITY_MASK        2
#define ACPI_GTDT_POLARITY_HIGH        0
#define ACPI_GTDT_POLARITY_LOW         2

#define PPTT_SIGNATURE "PPTT"

/*
 * arm64 IO Remapping Table (IORT).
 */
#define IORT_SIGNATURE "IORT"


#endif /* !_ACPI_TABLE_DEFS_H_ */
