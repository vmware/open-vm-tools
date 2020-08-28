/*********************************************************
 * Copyright (C) 2005-2016, 2020 VMware, Inc. All rights reserved.
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
 * backdoorInt.h --
 *
 *      Internal function prototypes for the real backdoor work.
 */

void BackdoorHbIn(Backdoor_proto_hb *bp);
void BackdoorHbOut(Backdoor_proto_hb *bp);
void BackdoorHb(Backdoor_proto_hb *myBp, Bool outbound);

/*
 * Are vmcall/vmmcall hypercall instructions available in the assembler?
 * Use the compiler version as a proxy.
 */
#if defined(__linux__) && defined(__GNUC__)
#define GCC_VERSION (__GNUC__ * 10000 + \
                     __GNUC_MINOR__ * 100 + \
                     __GNUC_PATCHLEVEL__)
#if GCC_VERSION > 40803 && !defined(__aarch64__)
#define USE_HYPERCALL
#endif
#endif

#if defined(USE_HYPERCALL)
void BackdoorHbVmcall(Backdoor_proto_hb *bp);
void BackdoorHbVmmcall(Backdoor_proto_hb *bp);
#endif
