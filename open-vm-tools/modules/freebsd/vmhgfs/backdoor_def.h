/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * backdoor_def.h --
 *
 * This contains backdoor defines that can be included from
 * an assembly language file.
 */



#ifndef _BACKDOOR_DEF_H_
#define _BACKDOOR_DEF_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

/*
 * If you want to add a new low-level backdoor call for a guest userland
 * application, please consider using the GuestRpc mechanism instead. --hpreg
 */

#define BDOOR_MAGIC 0x564D5868

/* Low-bandwidth backdoor port. --hpreg */

#define BDOOR_PORT 0x5658

#define BDOOR_CMD_GETMHZ      		   1
/*
 * BDOOR_CMD_APMFUNCTION is used by:
 *
 * o The FrobOS code, which instead should either program the virtual chipset
 *   (like the new BIOS code does, matthias offered to implement that), or not
 *   use any VM-specific code (which requires that we correctly implement
 *   "power off on CLI HLT" for SMP VMs, boris offered to implement that)
 *
 * o The old BIOS code, which will soon be jettisoned
 *
 *  --hpreg
 */
#define BDOOR_CMD_APMFUNCTION 		   2
#define BDOOR_CMD_GETDISKGEO  		   3
#define BDOOR_CMD_GETPTRLOCATION	      4
#define BDOOR_CMD_SETPTRLOCATION	      5
#define BDOOR_CMD_GETSELLENGTH		   6
#define BDOOR_CMD_GETNEXTPIECE		   7
#define BDOOR_CMD_SETSELLENGTH		   8
#define BDOOR_CMD_SETNEXTPIECE		   9
#define BDOOR_CMD_GETVERSION		      10
#define BDOOR_CMD_GETDEVICELISTELEMENT	11
#define BDOOR_CMD_TOGGLEDEVICE		   12
#define BDOOR_CMD_GETGUIOPTIONS		   13
#define BDOOR_CMD_SETGUIOPTIONS		   14
#define BDOOR_CMD_GETSCREENSIZE		   15
#define BDOOR_CMD_MONITOR_CONTROL       16
#define BDOOR_CMD_GETHWVERSION          17
#define BDOOR_CMD_OSNOTFOUND            18
#define BDOOR_CMD_GETUUID               19
#define BDOOR_CMD_GETMEMSIZE            20
#define BDOOR_CMD_HOSTCOPY              21 /* Devel only */
#define BDOOR_CMD_SERVICE_VM            22 /* prototype only */         
#define BDOOR_CMD_GETTIME               23 /* Deprecated. Use GETTIMEFULL. */
#define BDOOR_CMD_STOPCATCHUP           24
#define BDOOR_CMD_PUTCHR	        25 /* Devel only */
#define BDOOR_CMD_ENABLE_MSG	        26 /* Devel only */
#define BDOOR_CMD_GOTO_TCL	        27 /* Devel only */
#define BDOOR_CMD_INITPCIOPROM		28
#define BDOOR_CMD_INT13			29
#define BDOOR_CMD_MESSAGE               30
#define BDOOR_CMD_RSVD0                 31
#define BDOOR_CMD_RSVD1                 32
#define BDOOR_CMD_RSVD2                 33
#define BDOOR_CMD_ISACPIDISABLED	34
#define BDOOR_CMD_TOE			35 /* Not in use */
#define BDOOR_CMD_ISMOUSEABSOLUTE       36
#define BDOOR_CMD_PATCH_SMBIOS_STRUCTS  37
#define BDOOR_CMD_MAPMEM                38 /* Devel only */
#define BDOOR_CMD_ABSPOINTER_DATA	39
#define BDOOR_CMD_ABSPOINTER_STATUS	40
#define BDOOR_CMD_ABSPOINTER_COMMAND	41
#define BDOOR_CMD_TIMER_SPONGE          42
#define BDOOR_CMD_PATCH_ACPI_TABLES	43
/* Catch-all to allow synchronous tests */
#define BDOOR_CMD_DEVEL_FAKEHARDWARE	44 /* Debug only - needed in beta */
#define BDOOR_CMD_GETHZ      		45
#define BDOOR_CMD_GETTIMEFULL           46
#define BDOOR_CMD_STATELOGGER           47
#define BDOOR_CMD_CHECKFORCEBIOSSETUP	48
#define BDOOR_CMD_LAZYTIMEREMULATION    49
#define BDOOR_CMD_BIOSBBS               50
#define BDOOR_CMD_VASSERT               51
#define BDOOR_CMD_ISGOSDARWIN           52
#define BDOOR_CMD_DEBUGEVENT            53
#define BDOOR_CMD_OSNOTMACOSXSERVER     54
#define BDOOR_CMD_GETTIMEFULL_WITH_LAG  55
#define BDOOR_CMD_ACPI_HOTPLUG_DEVICE   56
#define BDOOR_CMD_ACPI_HOTPLUG_MEMORY   57
#define BDOOR_CMD_ACPI_HOTPLUG_CBRET    58
#define BDOOR_CMD_GET_HOST_VIDEO_MODES  59 /* Not in use */
#define BDOOR_CMD_ACPI_HOTPLUG_CPU      60
#define BDOOR_CMD_USB_HOTPLUG_MOUSE     61 /* Not in use */
#define BDOOR_CMD_XPMODE                62
#define BDOOR_CMD_NESTING_CONTROL       63
#define BDOOR_CMD_FIRMWARE_INIT         64
#define BDOOR_CMD_MAX                   65


/* 
 * IMPORTANT NOTE: When modifying the behavior of an existing backdoor command,
 * you must adhere to the semantics expected by the oldest Tools who use that
 * command. Specifically, do not alter the way in which the command modifies 
 * the registers. Otherwise backwards compatibility will suffer.
 */

/* High-bandwidth backdoor port. --hpreg */

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

#define IS_BDOOR_PMC(index)  (((index) | 3) == 0x10003)
#define BDOOR_CMD(ecx)       ((ecx) & 0xffff)


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
   return cmd == BDOOR_CMD_RSVD0 ||
          cmd == BDOOR_CMD_RSVD1 ||
          cmd == BDOOR_CMD_RSVD2;
}
#endif

#endif
