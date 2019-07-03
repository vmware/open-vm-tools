/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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
 * backdoor_def.h --
 *
 * This contains backdoor defines that can be included from
 * an assembly language file.
 */

#ifndef _BACKDOOR_DEF_H_
#define _BACKDOOR_DEF_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#if defined __cplusplus
extern "C" {
#endif

/*
 * If you want to add a new low-level backdoor call for a guest userland
 * application, please consider using the GuestRpc mechanism instead.
 */

#define BDOOR_MAGIC 0x564D5868

/* Low-bandwidth backdoor port number for the IN/OUT interface. */

#define BDOOR_PORT        0x5658

/* Flags used by the hypercall interface. */

#define BDOOR_FLAGS_HB    (1<<0)
#define BDOOR_FLAGS_WRITE (1<<1)

#define BDOOR_IS_LB(_flags)    (((_flags) & BDOOR_FLAGS_HB) == 0)
#define BDOOR_IS_HB(_flags)    !BDOOR_IS_LB(_flags)
#define BDOOR_IS_READ(_flags)  (((_flags) & BDOOR_FLAGS_WRITE) == 0)
#define BDOOR_IS_WRITE(_flags) !BDOOR_IS_READ(_flags)

/*
 * Max number of BPNs that can be passed in a single call from monitor->VMX with
 * a HB backdoor request.  This should be kept in parity with
 * IOSPACE_MAX_REP_BPNS to keep performance between the two HB backdoor
 * interfaces comparable.
 */
#define BDOOR_HB_MAX_BPNS  513

#define   BDOOR_CMD_GETMHZ                    1
/*
 * BDOOR_CMD_APMFUNCTION is used by:
 *
 * o The FrobOS code, which instead should either program the virtual chipset
 *   (like the new BIOS code does, Matthias Hausner offered to implement that),
 *   or not use any VM-specific code (which requires that we correctly
 *   implement "power off on CLI HLT" for SMP VMs, Boris Weissman offered to
 *   implement that)
 *
 * o The old BIOS code, which will soon be jettisoned
 */
#define   BDOOR_CMD_APMFUNCTION               2 /* CPL0 only. */
#define   BDOOR_CMD_GETDISKGEO                3
#define   BDOOR_CMD_GETPTRLOCATION            4
#define   BDOOR_CMD_SETPTRLOCATION            5
#define   BDOOR_CMD_GETSELLENGTH              6
#define   BDOOR_CMD_GETNEXTPIECE              7
#define   BDOOR_CMD_SETSELLENGTH              8
#define   BDOOR_CMD_SETNEXTPIECE              9
#define   BDOOR_CMD_GETVERSION               10
#define   BDOOR_CMD_GETDEVICELISTELEMENT     11
#define   BDOOR_CMD_TOGGLEDEVICE             12
#define   BDOOR_CMD_GETGUIOPTIONS            13
#define   BDOOR_CMD_SETGUIOPTIONS            14
#define   BDOOR_CMD_GETSCREENSIZE            15
#define   BDOOR_CMD_MONITOR_CONTROL          16 /* Disabled by default. */
#define   BDOOR_CMD_GETHWVERSION             17
#define   BDOOR_CMD_OSNOTFOUND               18 /* CPL0 only. */
#define   BDOOR_CMD_GETUUID                  19
#define   BDOOR_CMD_GETMEMSIZE               20
//#define BDOOR_CMD_HOSTCOPY                 21 /* Not in use. Was devel only. */
//#define BDOOR_CMD_SERVICE_VM               22 /* Not in use. Never shipped. */
#define   BDOOR_CMD_GETTIME                  23 /* Deprecated -> GETTIMEFULL. */
#define   BDOOR_CMD_STOPCATCHUP              24
#define   BDOOR_CMD_PUTCHR                   25 /* Disabled by default. */
#define   BDOOR_CMD_ENABLE_MSG               26 /* Devel only. */
//#define BDOOR_CMD_GOTO_TCL                 27 /* Not in use. Was devel only */
#define   BDOOR_CMD_INITPCIOPROM             28 /* CPL 0 only. */
//#define BDOOR_CMD_INT13                    29 /* Not in use. */
#define   BDOOR_CMD_MESSAGE                  30
#define   BDOOR_CMD_SIDT                     31
#define   BDOOR_CMD_SGDT                     32
#define   BDOOR_CMD_SLDT_STR                 33
#define   BDOOR_CMD_ISACPIDISABLED           34
//#define BDOOR_CMD_TOE                      35 /* Not in use. */
#define   BDOOR_CMD_ISMOUSEABSOLUTE          36
#define   BDOOR_CMD_PATCH_SMBIOS_STRUCTS     37 /* CPL 0 only. */
#define   BDOOR_CMD_MAPMEM                   38 /* Devel only */
#define   BDOOR_CMD_ABSPOINTER_DATA          39
#define   BDOOR_CMD_ABSPOINTER_STATUS        40
#define   BDOOR_CMD_ABSPOINTER_COMMAND       41
//#define BDOOR_CMD_TIMER_SPONGE             42 /* Not in use. */
#define   BDOOR_CMD_PATCH_ACPI_TABLES        43 /* CPL 0 only. */
//#define BDOOR_CMD_DEVEL_FAKEHARDWARE       44 /* Not in use. */
#define   BDOOR_CMD_GETHZ                    45
#define   BDOOR_CMD_GETTIMEFULL              46
//#define BDOOR_CMD_STATELOGGER              47 /* Not in use. */
#define   BDOOR_CMD_CHECKFORCEBIOSSETUP      48 /* CPL 0 only. */
#define   BDOOR_CMD_LAZYTIMEREMULATION       49 /* CPL 0 only. */
#define   BDOOR_CMD_BIOSBBS                  50 /* CPL 0 only. */
//#define BDOOR_CMD_VASSERT                  51 /* Not in use. */
#define   BDOOR_CMD_ISGOSDARWIN              52
#define   BDOOR_CMD_DEBUGEVENT               53
#define   BDOOR_CMD_OSNOTMACOSXSERVER        54 /* CPL 0 only. */
#define   BDOOR_CMD_GETTIMEFULL_WITH_LAG     55
#define   BDOOR_CMD_ACPI_HOTPLUG_DEVICE      56 /* Devel only. */
#define   BDOOR_CMD_ACPI_HOTPLUG_MEMORY      57 /* Devel only. */
#define   BDOOR_CMD_ACPI_HOTPLUG_CBRET       58 /* Devel only. */
//#define BDOOR_CMD_GET_HOST_VIDEO_MODES     59 /* Not in use. */
#define   BDOOR_CMD_ACPI_HOTPLUG_CPU         60 /* Devel only. */
//#define BDOOR_CMD_USB_HOTPLUG_MOUSE        61 /* Not in use. Never shipped. */
#define   BDOOR_CMD_XPMODE                   62 /* CPL 0 only. */
#define   BDOOR_CMD_NESTING_CONTROL          63
#define   BDOOR_CMD_FIRMWARE_INIT            64 /* CPL 0 only. */
#define   BDOOR_CMD_FIRMWARE_ACPI_SERVICES   65 /* CPL 0 only. */
#  define BDOOR_CMD_FAS_GET_TABLE_SIZE        0
#  define BDOOR_CMD_FAS_GET_TABLE_DATA        1
#  define BDOOR_CMD_FAS_GET_PLATFORM_NAME     2
#  define BDOOR_CMD_FAS_GET_PCIE_OSC_MASK     3
#  define BDOOR_CMD_FAS_GET_APIC_ROUTING      4
#  define BDOOR_CMD_FAS_GET_TABLE_SKIP        5
#  define BDOOR_CMD_FAS_GET_SLEEP_ENABLES     6
#  define BDOOR_CMD_FAS_GET_HARD_RESET_ENABLE 7
#  define BDOOR_CMD_FAS_GET_MOUSE_HID         8
#  define BDOOR_CMD_FAS_GET_SMBIOS_VERSION    9
#  define BDOOR_CMD_FAS_GET_64BIT_PCI_HOLE_SIZE 10
//#define BDOOR_CMD_FAS_GET_NVDIMM_FMT_CODE  11 /* Not in use. Never shipped. */
#  define BDOOR_CMD_FAS_SRP_ENABLED          12
#  define BDOOR_CMD_FAS_EXIT_BOOT_SERVICES   13
#define   BDOOR_CMD_SENDPSHAREHINTS          66 /* Not in use. Deprecated. */
#define   BDOOR_CMD_ENABLE_USB_MOUSE         67
#define   BDOOR_CMD_GET_VCPU_INFO            68
#  define BDOOR_CMD_VCPU_SLC64                0
#  define BDOOR_CMD_VCPU_SYNC_VTSCS           1
#  define BDOOR_CMD_VCPU_HV_REPLAY_OK         2
#  define BDOOR_CMD_VCPU_LEGACY_X2APIC_OK     3
#  define BDOOR_CMD_VCPU_MMIO_HONORS_PAT      4
#  define BDOOR_CMD_VCPU_RESERVED            31
#define   BDOOR_CMD_EFI_SERIALCON_CONFIG     69 /* CPL 0 only. */
#define   BDOOR_CMD_BUG328986                70 /* CPL 0 only. */
#define   BDOOR_CMD_FIRMWARE_ERROR           71 /* CPL 0 only. */
#  define BDOOR_CMD_FE_INSUFFICIENT_MEM       0
#  define BDOOR_CMD_FE_EXCEPTION              1
#  define BDOOR_CMD_FE_SGX                    2
#  define BDOOR_CMD_FE_PCI_MMIO               3
#  define BDOOR_CMD_FE_GMM                    4
#define   BDOOR_CMD_VMK_INFO                 72
#define   BDOOR_CMD_EFI_BOOT_CONFIG          73 /* CPL 0 only. */
#  define BDOOR_CMD_EBC_LEGACYBOOT_ENABLED        0
#  define BDOOR_CMD_EBC_GET_ORDER                 1
#  define BDOOR_CMD_EBC_SHELL_ACTIVE              2
#  define BDOOR_CMD_EBC_GET_NETWORK_BOOT_PROTOCOL 3
#  define BDOOR_CMD_EBC_QUICKBOOT_ENABLED         4
#  define BDOOR_CMD_EBC_GET_PXE_ARCH              5
#  define BDOOR_CMD_EBC_SKIP_DELAYS               6
#define   BDOOR_CMD_GET_HW_MODEL             74 /* CPL 0 only. */
#define   BDOOR_CMD_GET_SVGA_CAPABILITIES    75 /* CPL 0 only. */
#define   BDOOR_CMD_GET_FORCE_X2APIC         76 /* CPL 0 only  */
#define   BDOOR_CMD_SET_PCI_HOLE             77 /* CPL 0 only  */
#define   BDOOR_CMD_GET_PCI_HOLE             78 /* CPL 0 only  */
#define   BDOOR_CMD_GET_PCI_BAR              79 /* CPL 0 only  */
#define   BDOOR_CMD_SHOULD_GENERATE_SYSTEMID 80 /* CPL 0 only  */
#define   BDOOR_CMD_READ_DEBUG_FILE          81 /* Devel only. */
#define   BDOOR_CMD_SCREENSHOT               82 /* Devel only. */
#define   BDOOR_CMD_INJECT_KEY               83 /* Devel only. */
#define   BDOOR_CMD_INJECT_MOUSE             84 /* Devel only. */
#define   BDOOR_CMD_MKS_GUEST_STATS          85 /* CPL 0 only. */
#  define BDOOR_CMD_MKSGS_RESET               0
#  define BDOOR_CMD_MKSGS_ADD_PPN             1
#  define BDOOR_CMD_MKSGS_REMOVE_PPN          2
#define   BDOOR_CMD_ABSPOINTER_RESTRICT      86
#define   BDOOR_CMD_GUEST_INTEGRITY          87
#  define BDOOR_CMD_GI_GET_CAPABILITIES       0
#  define BDOOR_CMD_GI_SETUP_ENTRY_POINT      1
#  define BDOOR_CMD_GI_SETUP_ALERTS           2
#  define BDOOR_CMD_GI_SETUP_STORE            3
#  define BDOOR_CMD_GI_SETUP_EVENT_RING       4
#  define BDOOR_CMD_GI_SETUP_NON_FAULT_READ   5
#  define BDOOR_CMD_GI_ENTER_INTEGRITY_MODE   6
#  define BDOOR_CMD_GI_EXIT_INTEGRITY_MODE    7
#  define BDOOR_CMD_GI_RESET_INTEGRITY_MODE   8
#  define BDOOR_CMD_GI_GET_EVENT_RING_STATE   9
#  define BDOOR_CMD_GI_CONSUME_RING_EVENTS   10
#  define BDOOR_CMD_GI_WATCH_MAPPINGS_START  11
#  define BDOOR_CMD_GI_WATCH_MAPPINGS_STOP   12
#  define BDOOR_CMD_GI_CHECK_MAPPINGS_NOW    13
#  define BDOOR_CMD_GI_WATCH_PPNS_START      14
#  define BDOOR_CMD_GI_WATCH_PPNS_STOP       15
#  define BDOOR_CMD_GI_SEND_MSG              16
#  define BDOOR_CMD_GI_TEST_READ_MOB        128
#  define BDOOR_CMD_GI_TEST_ADD_EVENT       129
#  define BDOOR_CMD_GI_TEST_MAPPING         130
#  define BDOOR_CMD_GI_TEST_PPN             131
#  define BDOOR_CMD_GI_MAX                  131
#define   BDOOR_CMD_MKSSTATS_SNAPSHOT        88 /* Devel only. */
#  define BDOOR_CMD_MKSSTATS_START            0
#  define BDOOR_CMD_MKSSTATS_STOP             1
#define   BDOOR_CMD_SECUREBOOT               89
#define   BDOOR_CMD_COPY_PHYSMEM             90 /* Devel only. */
#define   BDOOR_CMD_STEALCLOCK               91 /* CPL 0 only. */
#  define BDOOR_STEALCLOCK_STATUS_DISABLED    0
#  define BDOOR_STEALCLOCK_STATUS_ENABLED     1
#define   BDOOR_CMD_GUEST_PAGE_HINTS         92 /* CPL 0 only  */
#define   BDOOR_CMD_FIRMWARE_UPDATE          93 /* CPL 0 only. */
#  define BDOOR_CMD_FU_GET_HOST_VERSION       0
#  define BDOOR_CMD_FU_UPDATE_FROM_HOST       1
#  define BDOOR_CMD_FU_LOCK                   2
#define   BDOOR_CMD_FUZZER_HELPER            94 /* Devel only. */
#  define BDOOR_CMD_FUZZER_INIT               0
#  define BDOOR_CMD_FUZZER_NEXT               1
#define   BDOOR_CMD_PUTCHR12                 95
#define   BDOOR_CMD_GMM                      96
#  define BDOOR_CMD_GMM_GET_SIZE              0 /* Depends on firmware. */
#  define BDOOR_CMD_GMM_MAP_MEMORY            1 /* Depends on firmware. */
#  define BDOOR_CMD_GMM_ENTER                 2
#  define BDOOR_CMD_GMM_ONESHOT_TIMER         3
#  define BDOOR_CMD_GMM_WATCH_PPNS_START      4
#  define BDOOR_CMD_GMM_WATCH_PPNS_STOP       5
#  define BDOOR_CMD_GMM_RESYNC_RUNTIME_INFO   6
#  define BDOOR_CMD_GMM_INVS_BRK_POINT        7
#  define BDOOR_CMD_GMM_GET_CAPABILITY        8
#define   BDOOR_CMD_PRECISIONCLOCK           97
#  define BDOOR_CMD_PRECISIONCLOCK_GETTIME    0
#define   BDOOR_CMD_COREDUMP_UNSYNC          98 /* Devel only. For VMM cores */
#define   BDOOR_CMD_MAX                      99


/*
 * IMPORTANT NOTE: When modifying the behavior of an existing backdoor command,
 * you must adhere to the semantics expected by the oldest Tools who use that
 * command. Specifically, do not alter the way in which the command modifies
 * the registers. Otherwise backwards compatibility will suffer.
 */

/* Nesting control operations */

#define NESTING_CONTROL_RESTRICT_BACKDOOR 0
#define NESTING_CONTROL_OPEN_BACKDOOR     1
#define NESTING_CONTROL_QUERY             2
#define NESTING_CONTROL_MAX               2

/* EFI Boot Order options, nibble-sized. */
#define EFI_BOOT_ORDER_TYPE_EFI           0x0
#define EFI_BOOT_ORDER_TYPE_LEGACY        0x1
#define EFI_BOOT_ORDER_TYPE_NONE          0xf

#define BDOOR_NETWORK_BOOT_PROTOCOL_NONE  0x0
#define BDOOR_NETWORK_BOOT_PROTOCOL_IPV4  0x1
#define BDOOR_NETWORK_BOOT_PROTOCOL_IPV6  0x2

#define BDOOR_SECUREBOOT_STATUS_DISABLED  0xFFFFFFFFUL
#define BDOOR_SECUREBOOT_STATUS_APPROVED  1
#define BDOOR_SECUREBOOT_STATUS_DENIED    2

/* High-bandwidth backdoor port. */

#define BDOORHB_PORT 0x5659

#define BDOORHB_CMD_MESSAGE 0
#define BDOORHB_CMD_VASSERT 1
#define BDOORHB_CMD_MAX 2

/*
 * There is another backdoor which allows access to certain TSC-related
 * values using otherwise illegal PMC indices when the pseudo_perfctr
 * control flag is set.
 */

#define BDOOR_PMC_HW_TSC      0x10000
#define BDOOR_PMC_REAL_NS     0x10001
#define BDOOR_PMC_APPARENT_NS 0x10002
#define BDOOR_PMC_PSEUDO_TSC  0x10003

#define IS_BDOOR_PMC(index)  (((index) | 3) == 0x10003)
#define BDOOR_CMD(ecx)       ((ecx) & 0xffff)

/* Sub commands for BDOOR_CMD_VMK_INFO */
#define BDOOR_CMD_VMK_INFO_ENTRY   1

/*
 * Current format for the guest page hints is:
 *
 * Arg0: BDOOR_MAGIC, Arg3: BDOOR_PORT
 *
 * Arg1: (rbx on x86)
 *
 *  0         64
 *  |   PPN   |
 *
 * Arg2: (rcx on x86)
 *
 *  0         16        32         64
 *  | Command |  Type   | Reserved |
 *
 * Arg4: (rsi on x86)
 *
 *  0          16         64
 *  | numPages | Reserved |
 *
 */
#define BDOOR_GUEST_PAGE_HINTS_NOT_SUPPORTED ((unsigned)-1)
#define BDOOR_GUEST_PAGE_HINTS_MAX_PAGES     (0xffff)
#define BDOOR_GUEST_PAGE_HINTS_TYPE_PSHARE   (0)
#define BDOOR_GUEST_PAGE_HINTS_TYPE(reg)     (((reg) >> 16) & 0xffff)

#ifdef VMM
/*
 *----------------------------------------------------------------------
 *
 * Backdoor_CmdRequiresFullyValidVCPU --
 *
 *    A few backdoor commands require the full VCPU to be valid
 *    (including GDTR, IDTR, TR and LDTR). The rest get read/write
 *    access to GPRs and read access to Segment registers (selectors).
 *
 * Result:
 *    True iff VECX contains a command that require the full VCPU to
 *    be valid.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
Backdoor_CmdRequiresFullyValidVCPU(unsigned cmd)
{
   return cmd == BDOOR_CMD_SIDT ||
          cmd == BDOOR_CMD_SGDT ||
          cmd == BDOOR_CMD_SLDT_STR ||
          cmd == BDOOR_CMD_GMM;
}


/*
 *----------------------------------------------------------------------
 *
 * Backdoor_CmdRequiresValidSegments --
 *
 *    Returns TRUE if a backdoor command requires access to segment selectors.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
Backdoor_CmdRequiresValidSegments(unsigned cmd)
{
   return cmd == BDOOR_CMD_INITPCIOPROM ||
          cmd == BDOOR_CMD_GETMHZ;
}
#endif

#ifdef VM_ARM_64

/*
 * VMware x86 I/O space virtualization on arm.
 *
 * Implementation goal
 * ---
 * The goal of this implementation is to precisely mimic the semantics of the
 * "VMware x86 I/O space virtualization on x86", in particular:
 *
 * o A vCPU can perform an N-byte access to an I/O port address that is not
 *   N-byte aligned.
 *
 * o A vCPU can perform an N-byte access to I/O port address A without
 *   impacting I/O port addresses [ A + 1; A + N ).
 *
 * o A vCPU can access the I/O space when running 32-bit or 64-bit code.
 *
 * o A vCPU running in unprivileged mode can use the backdoor.
 *
 * As a result, VMware virtual device drivers that were initially developed for
 * x86 can trivially be ported to arm.
 *
 * Mechanism
 * ---
 * In this section, we call W<n> the 32-bit register which aliases the low 32
 * bits of the 64-bit register X<n>.
 *
 * A vCPU which wishes to use the "VMware x86 I/O space virtualization on arm"
 * must follow these 4 steps:
 *
 * 1) Write to general-purpose registers specific to the x86 I/O space
 *    instruction.
 *
 * The vCPU writes to the arm equivalent of general-purpose x86 registers (see
 * the BDOOR_ARG* mapping below) that are used by the x86 I/O space instruction
 * it is about to perform.
 *
 * Examples:
 * o For an IN instruction without DX register, there is nothing to do.
 * o For an OUT instruction with DX register, the vCPU places the I/O port
 *   address in bits W3<15:0> and the value to write in W0<7:0> (1 byte access)
 *   or W0<15:0> (2 bytes access) or W0 (4 bytes access).
 * o For an REP OUTS instruction, the vCPU places the I/O port address in bits
 *   W3<15:0>, the source virtual address in W4 (32-bit code) or X4 (64-bit
 *   code) and the number of repetitions in W2 (32-bit code) or X2 (64-bit
 *   code).
 *
 * 2) Write the x86 I/O space instruction to perform.
 *
 * The vCPU sets a value in W7, as described below:
 *
 * Transfer size, bits [1:0]
 *    00: 1 byte
 *    01: 2 bytes
 *    10: 4 bytes
 *    11: Invalid value
 *
 * Transfer direction, bit [2]
 *    0: Write (OUT/OUTS/REP OUTS instructions)
 *    1: Read (IN/INS/REP INS instructions)
 *
 * Instruction type, bits [4:3]
 *    00: Non-string instruction (IN/OUT) without DX register
 *        The port address (8-bit immediate) is set in W7<12:5>.
 *
 *    01: Non-string instruction (IN/OUT) with DX register
 *
 *    10: String instruction without REP prefix (INS/OUTS)
 *        The direction flag (EFLAGS.DF) is set in W7<5>.
 *
 *    11: String instruction with REP prefix (REP INS/REP OUTS)
 *        The direction flag (EFLAGS.DF) is set in W7<5>.
 *
 * All other bits not described above are reserved for future use and must be
 * set to 0.
 *
 * 3) Perform the x86 I/O space instruction.
 *
 * Several mechanisms are available:
 *
 * o From EL1
 * The vCPU executes the HVC (64-bit code) instruction with the immediate
 * X86_IO_MAGIC. This is the mechanism to favor from EL1 because it is
 * architectural.
 *
 * o From EL1 and EL0
 * 64-bit code: The vCPU sets X7<63:32> to X86_IO_MAGIC and executes the
 *              MRS XZR, MDCCSR_EL0 instruction.
 * 32-bit code: To be defined...
 * This is the mechanism to favor from EL0 because it has a negligible impact
 * on vCPU performance.
 *
 * o From EL1 and EL0
 * The vCPU executes the BRK (64-bit code) or BKPT (32-bit code) instruction
 * with the immediate X86_IO_MAGIC. Note that T32 code requires an 8-bit
 * immediate.
 *
 * 4) Read from general-purpose registers specific to the x86 I/O space
 *    instruction.
 *
 * The vCPU reads from the arm equivalent of general-purpose x86 registers (see
 * the BDOOR_ARG* mapping below) that are used by the x86 I/O space instruction
 * it has just performed.
 *
 * Examples:
 * o For an OUT instruction, there is nothing to do.
 * o For an IN instruction, retrieve the value that was read from W0<7:0> (1
 *   byte access) or W0<15:0> (2 bytes access) or W0 (4 bytes access).
 */

#define X86_IO_MAGIC          0x86

#define X86_IO_W7_SIZE_SHIFT  0
#define X86_IO_W7_SIZE_MASK   (0x3 << X86_IO_W7_SIZE_SHIFT)
#define X86_IO_W7_DIR         (1 << 2)
#define X86_IO_W7_WITH        (1 << 3)
#define X86_IO_W7_STR         (1 << 4)
#define X86_IO_W7_DF          (1 << 5)
#define X86_IO_W7_IMM_SHIFT   5
#define X86_IO_W7_IMM_MASK    (0xff << X86_IO_W7_IMM_SHIFT)

#define BDOOR_ARG0 REG_X0
#define BDOOR_ARG1 REG_X1
#define BDOOR_ARG2 REG_X2
#define BDOOR_ARG3 REG_X3
#define BDOOR_ARG4 REG_X4
#define BDOOR_ARG5 REG_X5
#define BDOOR_ARG6 REG_X6

#else

#define BDOOR_ARG0 REG_RAX
#define BDOOR_ARG1 REG_RBX
#define BDOOR_ARG2 REG_RCX
#define BDOOR_ARG3 REG_RDX
#define BDOOR_ARG4 REG_RSI
#define BDOOR_ARG5 REG_RDI
#define BDOOR_ARG6 REG_RBP

#endif


#if defined __cplusplus
}
#endif

#endif // _BACKDOOR_DEF_H_
