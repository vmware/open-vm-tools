/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of VMware Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission of VMware Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
 * includeCheck.h --
 *
 *	Restrict include file use.
 *
 * In every .h file, define one or more of these
 *
 *	INCLUDE_ALLOW_VMX 
 *	INCLUDE_ALLOW_USERLEVEL 
 *	INCLUDE_ALLOW_VMCORE
 *	INCLUDE_ALLOW_MODULE
 *	INCLUDE_ALLOW_VMKERNEL 
 *	INCLUDE_ALLOW_DISTRIBUTE
 *	INCLUDE_ALLOW_VMK_MODULE
 *      INCLUDE_ALLOW_VMKDRIVERS
 *      INCLUDE_ALLOW_VMIROM
 *
 * Then include this file.
 *
 * Any file that has INCLUDE_ALLOW_DISTRIBUTE defined will potentially
 * be distributed in source form along with GPLed code.  Ensure
 * that this is acceptable.
 */


/*
 * Declare a VMCORE-only variable to help classify object
 * files.  The variable goes in the common block and does
 * not create multiple definition link-time conflicts.
 */

#if defined VMCORE && defined VMX86_DEVEL && defined VMX86_DEBUG && \
    defined linux && !defined MODULE && \
    !defined COMPILED_WITH_VMCORE
#define COMPILED_WITH_VMCORE compiled_with_vmcore
#ifdef ASM
        .comm   compiled_with_vmcore, 0
#else
        asm(".comm compiled_with_vmcore, 0");
#endif /* ASM */
#endif


#if defined VMCORE && \
    !(defined VMX86_VMX || defined VMM || \
      defined MONITOR_APP || defined VMMON)
#error "Makefile problem: VMCORE without VMX86_VMX or \
        VMM or MONITOR_APP or MODULE."
#endif

#if defined VMCORE && !defined INCLUDE_ALLOW_VMCORE
#error "The surrounding include file is not allowed in vmcore."
#endif
#undef INCLUDE_ALLOW_VMCORE

#if defined VMX86_VMX && !defined VMCORE && \
    !(defined INCLUDE_ALLOW_VMX || defined INCLUDE_ALLOW_USERLEVEL)
#error "The surrounding include file is not allowed in the VMX."
#endif
#undef INCLUDE_ALLOW_VMX

#if defined USERLEVEL && !defined VMX86_VMX && !defined VMCORE && \
    !defined INCLUDE_ALLOW_USERLEVEL
#error "The surrounding include file is not allowed at userlevel."
#endif
#undef INCLUDE_ALLOW_USERLEVEL

#if defined MODULE && !defined VMKERNEL_MODULE && \
    !defined VMMON && !defined INCLUDE_ALLOW_MODULE
#error "The surrounding include file is not allowed in driver modules."
#endif
#undef INCLUDE_ALLOW_MODULE

#if defined VMMON && !defined INCLUDE_ALLOW_VMMON
#error "The surrounding include file is not allowed in vmmon."
#endif
#undef INCLUDE_ALLOW_VMMON

#if defined VMKERNEL && !defined INCLUDE_ALLOW_VMKERNEL
#error "The surrounding include file is not allowed in the vmkernel."
#endif
#undef INCLUDE_ALLOW_VMKERNEL

#if defined GPLED_CODE && !defined INCLUDE_ALLOW_DISTRIBUTE
#error "The surrounding include file is not allowed in GPL code."
#endif
#undef INCLUDE_ALLOW_DISTRIBUTE

#if defined VMKERNEL_MODULE && !defined VMKERNEL && \
    !defined INCLUDE_ALLOW_VMK_MODULE && !defined INCLUDE_ALLOW_VMKDRIVERS
#error "The surrounding include file is not allowed in vmkernel modules."
#endif
#undef INCLUDE_ALLOW_VMK_MODULE
#undef INCLUDE_ALLOW_VMKDRIVERS

#if defined VMIROM && ! defined INCLUDE_ALLOW_VMIROM
#error "The surrounding include file is not allowed in vmirom."
#endif
#undef INCLUDE_ALLOW_VMIROM
