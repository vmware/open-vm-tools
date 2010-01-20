/*********************************************************
 * Copyright (C) 2000 VMware, Inc. All rights reserved.
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

/*
 * net_sg.h --
 *
 *	Network packet scatter gather structure.
 */


#ifndef _NET_SG_H
#define _NET_SG_H

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#define NET_SG_DEFAULT_LENGTH	16

/*
 * A single scatter-gather element for a network packet.
 * The address is split into low and high to save space.
 * If we make it 64 bits then Windows pads things out such that
 * we lose a lot of space for each scatter gather array.
 * This adds up when you have embedded scatter-gather 
 * arrays for transmit and receive ring buffers.
 */
typedef struct NetSG_Elem {
   uint32 	addrLow;
   uint16	addrHi;
   uint16	length;
} NetSG_Elem;

typedef enum NetSG_AddrType {
   NET_SG_MACH_ADDR,
   NET_SG_PHYS_ADDR,
   NET_SG_VIRT_ADDR,
} NetSG_AddrType;

typedef struct NetSG_Array {
   uint16	addrType;
   uint16	length;
   NetSG_Elem	sg[NET_SG_DEFAULT_LENGTH];
} NetSG_Array;

#define NET_SG_SIZE(len) (sizeof(NetSG_Array) + (len - NET_SG_DEFAULT_LENGTH) * sizeof(NetSG_Elem))

#define NET_SG_MAKE_PA(elem)                 (PA)QWORD(elem.addrHi, elem.addrLow)
#define NET_SG_MAKE_PTR(elem) (char *)(uintptr_t)QWORD(elem.addrHi, elem.addrLow)

#endif
