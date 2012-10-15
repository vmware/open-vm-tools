/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * kernelStubs.h
 *
 * KernelStubs implements some userspace library functions in terms
 * of kernel functions to allow library userspace code to be used in a
 * kernel.
 */

#ifndef __KERNELSTUBS_H__
#define __KERNELSTUBS_H__

#ifdef linux
#   ifndef __KERNEL__
#      error "__KERNEL__ is not defined"
#   endif
#   include "driver-config.h" // Must be included before any other header files
#   include "vm_basic_types.h"
#   include <linux/kernel.h>
#   include <linux/string.h>
#elif defined(_WIN32)
#   include "vm_basic_types.h"
#   include <ntddk.h>   /* kernel memory APIs */
#   include <stdio.h>   /* for _vsnprintf, vsprintf */
#   include <stdarg.h>  /* for va_start stuff */
#   include <stdlib.h>  /* for min macro. */
#   include "vm_assert.h"  /* Our assert macros */
#elif defined(__FreeBSD__)
#   include "vm_basic_types.h"
#   ifndef _KERNEL
#      error "_KERNEL is not defined"
#   endif
#   include <sys/types.h>
#   include <sys/malloc.h>
#   include <sys/param.h>
#   include <sys/kernel.h>
#   include <machine/stdarg.h>
#   include <sys/libkern.h>
#elif defined(__APPLE__)
#   include "vm_basic_types.h"
#   ifndef KERNEL
#      error "KERNEL is not defined"
#   endif
#   include <stdarg.h>
#   include <string.h>
# elif defined(sun)
#   include "vm_basic_types.h"
#   include <sys/types.h>
#   include <sys/varargs.h>
#endif

/*
 * Function Prototypes
 */

#if defined(linux) || defined(__APPLE__) || defined (sun)

#  ifdef linux                               /* if (linux) { */
char *strdup(const char *source);
#  endif

/* Shared between Linux and Apple kernel stubs. */
void *malloc(size_t size);
void free(void *mem);
void *calloc(size_t num, size_t len);
void *realloc(void *ptr, size_t newSize);

#elif defined(_WIN32)                           /* } else if (_WIN32) { */

#if (_WIN32_WINNT == 0x0400)
/* The following declarations are missing on NT4. */
typedef unsigned int UINT_PTR;
typedef unsigned int SIZE_T;

/* No free with tag availaible on NT4 kernel! */
#define KRNL_STUBS_FREE(P,T)     ExFreePool((P))

#else /* _WIN32_WINNT */
#define KRNL_STUBS_FREE(P,T)     ExFreePoolWithTag((P),(T))
/* Win 2K and later useful kernel function, documented but not declared! */
NTKERNELAPI VOID ExFreePoolWithTag(IN PVOID  P, IN ULONG  Tag);
#endif /* _WIN32_WINNT */

#elif defined(__FreeBSD__)                      /* } else if (FreeBSD) { */

/* Kernel memory on FreeBSD is tagged for statistics and sanity checking. */
MALLOC_DECLARE(M_VMWARE_TEMP);

/*
 * On FreeBSD, the general memory allocator for both userland and the kernel is named
 * malloc, but the kernel malloc() takes more arguments.  The following alias & macros
 * work around this, to provide the standard malloc() API for userspace code that is
 * being used in the kernel.
 */

#   undef malloc

static INLINE void *
__compat_malloc(unsigned long size, struct malloc_type *type, int flags) {
   return malloc(size, type, flags);
}

#   define malloc(size)         __compat_malloc(size, M_VMWARE_TEMP, M_NOWAIT)
#   define calloc(count, size)  __compat_malloc((count) * (size),       \
                                                M_VMWARE_TEMP, M_NOWAIT|M_ZERO)
#   define realloc(buf, size)   realloc(buf, size, M_VMWARE_TEMP, M_NOWAIT)
#   define free(buf)            free(buf, M_VMWARE_TEMP)
#   define strchr(s,c)          index(s,c)
#   define strrchr(s,c)         rindex(s,c)

#endif                                          /* } */

/*
 * Stub functions we provide.
 */

void Panic(const char *fmt, ...);

char *Str_Strcpy(char *buf, const char *src, size_t maxSize);
int Str_Vsnprintf(char *str, size_t size, const char *format,
                  va_list arguments);
char *Str_Vasprintf(size_t *length, const char *format,
                    va_list arguments);
char *Str_Asprintf(size_t *length, const char *Format, ...);

/*
 * Functions the driver must implement for the stubs.
 */
EXTERN void Debug(const char *fmt, ...);


#endif /* __KERNELSTUBS_H__ */
