/*********************************************************
 * Copyright (C) 2015-2016 VMware, Inc. All rights reserved.
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
 * kernelStubsSal.h
 *
 * Contains definitions source annotation language definitions for kernel drivers.
 * This solves two issues:
 * 1. Microsoft changed their annotation language from SAL 1.0 (original one
 *    widely distributed by the Windows team) to their more final SAL 2.0
 *    langauge (championed by the VS team).
 * 2. We want these annotations to do nothing during non-Win32 compiles.
 *
 * A longer term goal is to rationalize this into Bora.
 */
#ifndef __KERNELSTUBSSAL_H__
#define __KERNELSTUBSSAL_H__

#if defined(_WIN32)
#  include <DriverSpecs.h>
#  if !defined(_SAL_VERSION)
#     define _SAL_VERSION 10
#  endif
#endif

#if !defined(_SAL_VERSION) || (defined(_SAL_VERSION) && _SAL_VERSION == 10)
#define _In_
#define _In_opt_
#define _In_reads_bytes_(count)
#define _In_reads_bytes_opt_(count)
#define _In_z_
#define _In_opt_z_
#define _Out_
#define _Out_opt_
#define _Out_writes_bytes_(capcount)
#define _Out_writes_bytes_opt_(capcount)
#define _Out_writes_bytes_to_(cap, count)
#define _Out_writes_bytes_to_opt_(cap, count)
#define _Out_bytecap_post_bytecount_(cap, count)
#define _Out_writes_z_(cap)
#define _Out_writes_opt_z_(cap)
#define _Out_z_cap_(e)
#define _Outptr_result_buffer_(count)
#define _Outptr_result_bytebuffer_(count)
#define _Outptr_result_bytebuffer_maybenull_(count)
#define _Outptr_opt_result_buffer_(count)
#define _Outptr_opt_result_bytebuffer_(count)
#define _Outptr_opt_result_bytebuffer_maybenull_(count)
#define _COM_Outptr_
#define _Inout_
#define _Inout_updates_bytes_(e)
#define _Inout_z_cap_(e)
#define _Post_z_count_(e)
#define _Ret_writes_z_(e)
#define _Ret_writes_maybenull_z_(e)
#define _Ret_maybenull_
#define _Ret_maybenull_z_
#define _Ret_range_(l,h)
#define _Success_(expr)
#define _Check_return_
#define _Must_inspect_result_
#define _Group_(annos)
#define _When_(expr, annos)
#define _Always_(annos)
#define _Printf_format_string_
#define _Use_decl_annotations_
#define _Dispatch_type_(mj)
#define _Function_class_(c)
#define _Requires_lock_held_(cs)
#define _Requires_lock_not_held_(cs)
#define _Acquires_lock_(l)
#define _Releases_lock_(l)
#define _IRQL_requires_max_(i)
#define _IRQL_requires_(i)
#define _IRQL_requires_same_
#define _Analysis_assume_(e)
#define _Pre_notnull_
#define _At_(expr,annos)

#else
// Sal 2.0 path - everything is already defined.
#endif // _SAL_VERSION

// Now define our own annotations
#if !defined(_SAL_VERSION) || (defined(_SAL_VERSION) && _SAL_VERSION == 10)
#define _When_windrv_(annos)
#define _Ret_allocates_malloc_mem_opt_bytecap_(_Size)
#define _Ret_allocates_malloc_mem_opt_bytecount_(_Size)
#define _Ret_allocates_malloc_mem_opt_bytecap_post_bytecount_(_Cap,_Count)
#define _Ret_allocates_malloc_mem_opt_z_bytecount_(_Size)
#define _Ret_allocates_malloc_mem_opt_z_
#define _In_frees_malloc_mem_opt_
#else
#define _When_windrv_(annos)                                               annos
#define _Ret_allocates_malloc_mem_opt_bytecap_(_Cap)                       __drv_allocatesMem("Memory") _Must_inspect_result_ _Ret_opt_bytecap_(_Cap)
#define _Ret_allocates_malloc_mem_opt_bytecount_(_Count)                   __drv_allocatesMem("Memory") _Must_inspect_result_ _Ret_opt_bytecount_(_Count)
#define _Ret_allocates_malloc_mem_opt_bytecap_post_bytecount_(_Cap,_Count) __drv_allocatesMem("Memory") _Must_inspect_result_ _Ret_opt_bytecap_(_Cap) _Ret_opt_bytecount_(_Count)
#define _Ret_allocates_malloc_mem_opt_z_bytecount_(_Count)                 __drv_allocatesMem("Memory") _Must_inspect_result_ _Ret_opt_z_bytecount_(_Count)
#define _Ret_allocates_malloc_mem_opt_z_                                   __drv_allocatesMem("Memory") _Must_inspect_result_ _Ret_opt_z_
#define _In_frees_malloc_mem_opt_                                          __drv_freesMem("Memory") _Pre_maybenull_ _Post_invalid_
#endif // _SAL_VERSION

// Best we can do for reallocate with simple annotations: assume old size was fully initialized.
#define _Ret_reallocates_malloc_mem_opt_newbytecap_oldbytecap_(_NewSize, _OldSize) _Ret_allocates_malloc_mem_opt_bytecap_post_bytecount_(_NewSize, _OldSize <= _NewSize ? _OldSize : _NewSize)
#define _Ret_reallocates_malloc_mem_opt_newbytecap_(_NewSize)                      _Ret_allocates_malloc_mem_opt_z_bytecount_(_NewSize)
#define _In_reallocates_malloc_mem_opt_oldptr_                                     _In_frees_malloc_mem_opt_

#endif // __KERNELSTUBSSAL_H__
