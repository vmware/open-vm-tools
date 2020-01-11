/*********************************************************
 * Copyright (C) 2012-2019 VMware, Inc. All rights reserved.
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
 * ioplGet.h --
 *
 *   A utility function to retrieve the IOPL level of the current thread
 *   Compiles on x86, x64 of Linux and Windows
 */

#ifndef _IOPL_GET_H_
#define _IOPL_GET_H_

#include "x86_basic_defs.h"
#include "vm_basic_asm.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define Iopl_Get() ((GetCallerEFlags() >> EFLAGS_IOPL_SHIFT) & 0x3)

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif
