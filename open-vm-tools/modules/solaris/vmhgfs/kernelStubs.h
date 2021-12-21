/*********************************************************
 * Copyright (C) 2006-2019,2021 VMware, Inc. All rights reserved.
 *
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

#define KRNL_STUBS_DRIVER_TYPE_POSIX      1
#define KRNL_STUBS_DRIVER_TYPE_GDI        2
#define KRNL_STUBS_DRIVER_TYPE_WDM        3
#define KRNL_STUBS_DRIVER_TYPE_NDIS       4
#define KRNL_STUBS_DRIVER_TYPE_STORPORT   5

// For now (vsphere-2015), choose a good default. Later we'll modify all the
// build files using KernelStubs to set this.
#ifndef KRNL_STUBS_DRIVER_TYPE
#  if defined(_WIN32)
#     define KRNL_STUBS_DRIVER_TYPE KRNL_STUBS_DRIVER_TYPE_WDM
#  else
#     define KRNL_STUBS_DRIVER_TYPE KRNL_STUBS_DRIVER_TYPE_POSIX
#  endif
#endif

#ifdef __linux__
#   ifndef __KERNEL__
#      error "__KERNEL__ is not defined"
#   endif
#   include "driver-config.h" // Must be included before any other header files
#   include "vm_basic_types.h"
#   include <linux/kernel.h>
#   include <linux/string.h>
#elif defined(_WIN32)
#   define _CRT_ALLOCATION_DEFINED // prevent malloc.h from defining malloc et. all
#   if KRNL_STUBS_DRIVER_TYPE == KRNL_STUBS_DRIVER_TYPE_GDI
#      include <d3d9.h>
#      include <winddi.h>
#      include <stdio.h>
#      include "vm_basic_types.h"
#      include "vm_basic_defs.h"
#      include "vm_assert.h"
#   elif KRNL_STUBS_DRIVER_TYPE == KRNL_STUBS_DRIVER_TYPE_NDIS
#      include <ntddk.h>
#      include <stdio.h>    /* for _vsnprintf, vsprintf */
#      include <stdarg.h>   /* for va_start stuff */
#      include "vm_basic_defs.h"
#      include "vm_assert.h"
#      include "kernelStubsFloorFixes.h"
#pragma warning(disable:4201) // unnamed struct/union
#      include <ndis.h>
#   elif KRNL_STUBS_DRIVER_TYPE == KRNL_STUBS_DRIVER_TYPE_STORPORT
#      include "vm_basic_types.h"
#      include <wdm.h>   /* kernel memory APIs, DbgPrintEx */
#      include <stdio.h>    /* for _vsnprintf, vsprintf */
#      include <stdarg.h>   /* for va_start stuff */
#      include <stdlib.h>   /* for min macro. */
#      include <Storport.h> /* for Storport functions */
#      include "vm_basic_defs.h"
#      include "vm_assert.h"  /* Our assert macros */
#      include "kernelStubsFloorFixes.h"
#   elif KRNL_STUBS_DRIVER_TYPE == KRNL_STUBS_DRIVER_TYPE_WDM
#      include "vm_basic_types.h"
#      if defined(NTDDI_WINXP) && (NTDDI_VERSION >= NTDDI_WINXP)
#         include <wdm.h>   /* kernel memory APIs, DbgPrintEx */
#      else
#         include <ntddk.h> /* kernel memory APIs */
#      endif
#      include <stdio.h>    /* for _vsnprintf, vsprintf */
#      include <stdarg.h>   /* for va_start stuff */
#      include <stdlib.h>   /* for min macro. */
#      include "vm_basic_defs.h"
#      include "vm_assert.h"  /* Our assert macros */
#      include "kernelStubsFloorFixes.h"
#   else
#      error Type KRNL_STUBS_DRIVER_TYPE must be defined.
#   endif
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
#include "kernelStubsSal.h"

/*
 * Function Prototypes
 */

#if defined(__linux__) || defined(__APPLE__) || defined (sun)

#  ifdef __linux__                           /* if (__linux__) { */
#  define atoi(s) simple_strtol(((s != NULL) ? s : ""), NULL, 10)
int strcasecmp(const char *s1, const char *s2);
char *strdup(const char *source);
#  endif

#  ifdef __APPLE__                           /* if (__APPLE__) { */
int atoi(const char *);
char *STRDUP(const char *, int);
#  define strdup(s) STRDUP(s, 80)
#  endif

#  if defined(__linux__) || defined(__APPLE__) /* if (__linux__ || __APPLE__) { */
#  define Str_Strcasecmp(s1, s2) strcasecmp(s1, s2)
#  endif

/* Shared between Linux and Apple kernel stubs. */
void *malloc(size_t size);
void free(void *mem);
void *calloc(size_t num, size_t len);
void *realloc(void *ptr, size_t newSize);

#elif defined(_WIN32)                           /* } else if (_WIN32) { */

_Ret_allocates_malloc_mem_opt_bytecap_(_Size)
_When_windrv_(_IRQL_requires_max_(DISPATCH_LEVEL))
_CRTNOALIAS _CRTRESTRICT
void * __cdecl malloc(
   _In_ size_t _Size);

_Ret_allocates_malloc_mem_opt_bytecount_(_Count*_Size)
_When_windrv_(_IRQL_requires_max_(DISPATCH_LEVEL))
_CRTNOALIAS _CRTRESTRICT
void * __cdecl calloc(
   _In_ size_t _Count,
   _In_ size_t _Size);

_When_windrv_(_IRQL_requires_max_(DISPATCH_LEVEL))
_CRTNOALIAS
void __cdecl free(
   _In_frees_malloc_mem_opt_ void * _Memory);

_Success_(return != 0)
_When_(_Memory != 0, _Ret_reallocates_malloc_mem_opt_newbytecap_oldbytecap_(_NewSize, ((uintptr_t*)_Memory)[-1]))
_When_(_Memory == 0, _Ret_reallocates_malloc_mem_opt_newbytecap_(_NewSize))
_When_windrv_(_IRQL_requires_max_(DISPATCH_LEVEL))
_CRTNOALIAS _CRTRESTRICT
void * __cdecl realloc(
   _In_reallocates_malloc_mem_opt_oldptr_ void * _Memory,
   _In_ size_t _NewSize);

_Success_(return != 0)
_Ret_allocates_malloc_mem_opt_z_
_When_windrv_(_IRQL_requires_max_(DISPATCH_LEVEL))
_CRTIMP
char * __cdecl _strdup_impl(
   _In_opt_z_ const char * _Src);

#define strdup _strdup_impl

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

_Ret_writes_z_(maxSize)
char *Str_Strcpy(
   _Out_z_cap_(maxSize) char *buf,
   _In_z_ const char *src,
   _In_ size_t maxSize);

_Ret_writes_z_(maxSize)
char *Str_Strcat(
   _Inout_z_cap_(maxSize) char *buf,
   _In_z_ const char *src,
   _In_ size_t maxSize);

_Success_(return >= 0)
int Str_Sprintf(
   _Out_z_cap_(maxSize) _Post_z_count_(return+1) char *buf,
   _In_ size_t maxSize,
   _In_z_ _Printf_format_string_ const char *fmt,
   ...) PRINTF_DECL(3, 4);

_Success_(return != -1)
int Str_Vsnprintf(
   _Out_z_cap_(size) _Post_z_count_(return+1) char *str,
   _In_ size_t size,
   _In_z_ _Printf_format_string_ const char *format,
   _In_ va_list ap) PRINTF_DECL(3, 0);

_Success_(return != 0)
_When_(length != 0, _Ret_allocates_malloc_mem_opt_z_bytecount_(*length))
_When_(length == 0, _Ret_allocates_malloc_mem_opt_z_)
_When_windrv_(_IRQL_requires_max_(DISPATCH_LEVEL))
char *Str_Vasprintf(
   _Out_opt_ size_t *length,
   _In_z_ _Printf_format_string_ const char *format,
   _In_ va_list arguments) PRINTF_DECL(2, 0);

_Success_(return != 0)
_When_(length != 0, _Ret_allocates_malloc_mem_opt_z_bytecount_(*length))
_When_(length == 0, _Ret_allocates_malloc_mem_opt_z_)
_When_windrv_(_IRQL_requires_max_(DISPATCH_LEVEL))
char *Str_Asprintf(
   _Out_opt_ size_t *length,
   _In_z_ _Printf_format_string_ const char *format,
   ...) PRINTF_DECL(2, 3);

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 28301) // Suppress complaint that first declaration lacked annotations
#endif

// For now (vsphere-2015), we don't implement Panic, Warning, or Debug in the
// GDI case.
#if (KRNL_STUBS_DRIVER_TYPE != KRNL_STUBS_DRIVER_TYPE_GDI) &&\
    (KRNL_STUBS_DRIVER_TYPE != KRNL_STUBS_DRIVER_TYPE_NDIS)

/*
 * Stub functions we provide.
 */
#ifdef _WIN32
NORETURN
#endif
void Panic(
   _In_z_ _Printf_format_string_ const char *fmt,
   ...) PRINTF_DECL(1, 2);

void Warning(
   _In_z_ _Printf_format_string_ const char *fmt,
   ...) PRINTF_DECL(1, 2);

/*
 * Functions the driver must implement for the stubs.
 */
EXTERN void Debug(
   _In_z_ _Printf_format_string_ const char *fmt,
   ...) PRINTF_DECL(1, 2);

#endif // KRNL_STUBS_DRIVER_TYPE != KRNL_STUBS_DRIVER_TYPE_GDI

#ifdef _WIN32
#pragma warning(pop)
#endif

#endif /* __KERNELSTUBS_H__ */
