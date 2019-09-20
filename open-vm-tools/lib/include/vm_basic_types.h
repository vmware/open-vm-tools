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
 *
 * vm_basic_types.h --
 *
 *    basic data types.
 */


#ifndef _VM_BASIC_TYPES_H_
#define _VM_BASIC_TYPES_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMKDRIVERS
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

/*
 * Macros __i386__ and __ia64 are intrinsically defined by GCC
 */
#if defined _MSC_VER && defined _M_X64
#  define __x86_64__
#elif defined _MSC_VER && defined _M_IX86
#  define __i386__
#endif

/*
 * Setup a bunch of defines for instruction set architecture (ISA) related
 * properties.
 *
 * For compiler types/size:
 *
 * - VM_32BIT for a 32-bit ISA (with the same C compiler types/sizes as 32-bit
 *   x86/ARM).
 * - VM_64BIT for a 64-bit ISA (with the same C compiler types/sizes as 64-bit
 *   x86/ARM).
 *
 * For a given <arch> in {X86, ARM}:
 *
 * - VM_<arch>_32 for the 32-bit variant.
 * - VM_<arch>_64 for the 64-bit variant.
 * - VM_<arch>_ANY for any variant of <arch>.
 */

#ifdef __i386__
#define VM_X86_32
#define VM_X86_ANY
#define VM_32BIT
#endif

#ifdef __x86_64__
#define VM_X86_64
#define vm_x86_64 1
#define VM_X86_ANY
#define VM_64BIT
#else
#define vm_x86_64 0
#endif

#ifdef __arm__
#define VM_ARM_32
#define VM_ARM_ANY
#define VM_32BIT
#endif

#ifdef __aarch64__
#define VM_ARM_64
#define vm_arm_64 1
#define VM_ARM_ANY
#define VM_64BIT
#else
#define vm_arm_64 0
#endif

#define vm_64bit (sizeof (void *) == 8)

#ifdef _MSC_VER

#pragma warning (3 :4505) // unreferenced local function
#pragma warning (disable :4018) // signed/unsigned mismatch
#pragma warning (disable :4761) // integral size mismatch in argument; conversion supplied
#pragma warning (disable :4305) // truncation from 'const int' to 'short'
#pragma warning (disable :4244) // conversion from 'unsigned short' to 'unsigned char'
#pragma warning (disable :4267) // truncation of 'size_t'
#pragma warning (disable :4146) // unary minus operator applied to unsigned type, result still unsigned
#pragma warning (disable :4142) // benign redefinition of type

#endif

#if defined(__FreeBSD__) && (__FreeBSD__ + 0 < 5)
#  error FreeBSD detected without major version (PR 2116887)
#endif

/*
 * C99 <stdint.h> or equivalent
 * Special cases:
 * - Linux kernel lacks <stdint.h>, preferring <linux/types.h>
 *   (and defines uintptr_t since 2.6.24, but not intptr_t)
 * - Solaris collides with gcc <stdint.h>, but has <sys/stdint.h>
 * - VMKernel + FreeBSD collides with gcc <stdint.h>, but has <sys/stdint.h>
 * - VMKernel (+DECODERLIB) share macros with Linux kernel
 * - Windows only added <stdint.h> in vc10/vs2010 (MSC ver 1600),
 *   and WDKs lack it.
 *
 * NB about LLP64 in LP64 environments:
 * - Apple uses 'long long' uint64_t
 * - Linux kernel uses 'long long' uint64_t
 * - Linux userlevel uses 'long' uint64_t
 */
#if !defined(VMKERNEL) && !defined(DECODERLIB) && \
    defined(__linux__) && defined(__KERNEL__)
#  include <linux/types.h>
#  include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
   typedef unsigned long uintptr_t;
#endif
   typedef   signed long  intptr_t;
#elif (defined(__sun__) && defined(_KERNEL)) || \
      (defined(VMKERNEL) && defined(__FreeBSD__)) || \
      defined(_SYS_STDINT_H_)
#  include <sys/stdint.h>
#elif !defined(_MSC_VER)
   /* Common case */
#  include <stdint.h>
#else
   /* COMPAT: until pre-vc10 is retired */
#  include <crtdefs.h>  // uintptr_t
   typedef unsigned __int64   uint64_t;
   typedef unsigned int       uint32_t;
   typedef unsigned short     uint16_t;
   typedef unsigned char      uint8_t;

   typedef __int64            int64_t;
   typedef int                int32_t;
   typedef short              int16_t;
   typedef signed char        int8_t;
#endif

/*
 * size_t and ssize_t, or equivalent
 * Options:
 * C90+ <stddef.h> has size_t, but is incompatible with many kernels.
 * POSIX <sys/types.h> has size_t and ssize_t, is always available at
 *    userlevel but is missing from some kernels.
 *
 * Special cases:
 * - Linux kernel (again) does everything via <linux/types.h>
 * - VMKernel may or may not have POSIX headers (tcpip only)
 * - VMM does not have POSIX headers
 * - Windows <sys/types.h> does not define ssize_t
 */
#if defined(VMKERNEL) || defined(VMM) || defined(DECODERLIB)
   /* Guard against FreeBSD <sys/types.h> collison. */
#  if !defined(_SIZE_T_DEFINED) && !defined(_SIZE_T)
#     define _SIZE_T_DEFINED
#     define _SIZE_T
      typedef __SIZE_TYPE__ size_t;
#endif
#  if !defined(_SSIZE_T_DEFINED) && !defined(_SSIZE_T)
#     define _SSIZE_T_DEFINED
#     define _SSIZE_T
      typedef int64_t ssize_t;
#  endif
#elif defined(__linux__) && defined(__KERNEL__)
   /* <linux/types.h> provided size_t, ssize_t. */
#else
#  include <sys/types.h>
#  if defined(_WIN64)
      typedef int64_t ssize_t;
#  elif defined(_WIN32)
      typedef int32_t ssize_t;
#  endif
#endif


typedef uint64_t    uint64;
typedef  int64_t     int64;
typedef uint32_t    uint32;
typedef  int32_t     int32;
typedef uint16_t    uint16;
typedef  int16_t     int16;
typedef  uint8_t     uint8;
typedef   int8_t      int8;


/*
 * The _XTYPEDEF_BOOL guard prevents colliding with:
 * <X11/Xlib.h> #define Bool int
 * <X11/Xdefs.h> typedef int Bool;
 * If using this header AND X11 headers, be sure to #undef Bool and
 * be careful about the different size.
 */
#if !defined(_XTYPEDEF_BOOL)
#define _XTYPEDEF_BOOL
/*
 * C does not specify whether char is signed or unsigned, and
 * both gcc and msvc implement it as a non-signed, non-unsigned type.
 * Thus, (uint8_t *)&Bool and (int8_t *)&Bool are possible compile errors.
 * This is intentional.
 */
typedef char           Bool;
#endif

#ifndef FALSE
#define FALSE          0
#endif

#ifndef TRUE
#define TRUE           1
#endif

#define IS_BOOL(x)     (((x) & ~1) == 0)


/*
 * Before trying to do the includes based on OS defines, see if we can use
 * feature-based defines to get as much functionality as possible
 */

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#ifdef HAVE_SYS_INTTYPES_H
#include <sys/inttypes.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef __FreeBSD__
#include <sys/param.h> /* For __FreeBSD_version */
#endif

#if !defined(USING_AUTOCONF)
#   if defined(__FreeBSD__) || defined(sun)
#      ifndef KLD_MODULE
#         if __FreeBSD_version >= 500043
#            if !defined(VMKERNEL)
#               include <inttypes.h>
#            endif
#         else
#            include <sys/inttypes.h>
#         endif
#      endif
#   elif defined __APPLE__
#      if KERNEL
#         include <sys/unistd.h>
#      else
#         include <unistd.h>
#         include <inttypes.h>
#         include <stdlib.h>
#      endif
#   endif
#endif


#if defined(__GNUC__) && defined(__SIZEOF_INT128__)

typedef unsigned __int128 uint128;
typedef          __int128  int128;

#define MIN_INT128   ((int128)1 << 127)
#define MAX_INT128   (~MIN_INT128)
#define MIN_UINT128  ((uint128)0)
#define MAX_UINT128  (~MIN_UINT128)

#endif


/*
 * Time
 * XXX These should be cleaned up.  -- edward
 */

typedef int64 VmTimeType;          /* Time in microseconds */
typedef int64 VmTimeRealClock;     /* Real clock kept in microseconds */
typedef int64 VmTimeVirtualClock;  /* Virtual Clock kept in CPU cycles */

/*
 * Printf format specifiers for size_t and 64-bit number.
 * Use them like this:
 *    printf("%" FMT64 "d\n", big);
 * The spaces are important for C++11 compatibility.
 *
 * FMTH is for handles/fds.
 */

#ifdef _MSC_VER
   /* MSVC added C99-compatible formatting in vs2015. */
   #define FMT64      "I64"
   #ifdef VM_X86_64
      #define FMTSZ      "I64"
      #define FMTPD      "I64"
      #define FMTH       "I64"
   #else
      #define FMTSZ      "I"
      #define FMTPD      "I"
      #define FMTH       "I"
   #endif
#elif defined __APPLE__ || (!defined VMKERNEL && !defined DECODERLIB && \
                            defined __linux__ && defined __KERNEL__)
   /* semi-LLP64 targets; 'long' is 64-bit, but uint64_t is 'long long' */
   #define FMT64         "ll"
   #if defined(__APPLE__) && KERNEL
      /* macOS osfmk/kern added 'z' length specifier in 10.13 */
      #define FMTSZ      "l"
   #else
      #define FMTSZ      "z"
   #endif
   #define FMTPD         "l"
   #define FMTH          ""
#elif defined __GNUC__
   /*
    * Every POSIX system we target has C99-compatible printf
    * (supports 'z' for size_t and 'll' for long long).
    */
   #define FMTH          ""
   #define FMTSZ         "z"
   #if defined(VM_X86_64) || defined(VM_ARM_64)
      #define FMT64      "l"
      #define FMTPD      "l"
   #else
      #define FMT64      "ll"
      #define FMTPD      ""
   #endif
#else
   #error - Need compiler define for FMT64 and FMTSZ
#endif

/*
 * Suffix for 64-bit constants.  Use it like this:
 *    CONST64(0x7fffffffffffffff) for signed or
 *    CONST64U(0x7fffffffffffffff) for unsigned.
 *
 * 2004.08.30(thutt):
 *   The vmcore/asm64/gen* programs are compiled as 32-bit
 *   applications, but must handle 64 bit constants.  If the
 *   64-bit-constant defining macros are already defined, the
 *   definition will not be overwritten.
 */

#if !defined(CONST64) || !defined(CONST64U)
#ifdef _MSC_VER
#define CONST64(c) c##I64
#define CONST64U(c) c##uI64
#elif defined __APPLE__
#define CONST64(c) c##LL
#define CONST64U(c) c##uLL
#elif __GNUC__
#if defined(VM_X86_64) || defined(VM_ARM_64)
#define CONST64(c) c##L
#define CONST64U(c) c##uL
#else
#define CONST64(c) c##LL
#define CONST64U(c) c##uLL
#endif
#else
#error - Need compiler define for CONST64
#endif
#endif

/*
 * Use CONST3264/CONST3264U if you want a constant to be
 * treated as a 32-bit number on 32-bit compiles and
 * a 64-bit number on 64-bit compiles. Useful in the case
 * of shifts, like (CONST3264U(1) << x), where x could be
 * more than 31 on a 64-bit compile.
 */

#if defined(VM_X86_64) || defined(VM_ARM_64)
    #define CONST3264(a) CONST64(a)
    #define CONST3264U(a) CONST64U(a)
#else
    #define CONST3264(a) (a)
    #define CONST3264U(a) (a)
#endif

#define MIN_INT8   ((int8)0x80)
#define MAX_INT8   ((int8)0x7f)

#define MIN_UINT8  ((uint8)0)
#define MAX_UINT8  ((uint8)0xff)

#define MIN_INT16  ((int16)0x8000)
#define MAX_INT16  ((int16)0x7fff)

#define MIN_UINT16 ((uint16)0)
#define MAX_UINT16 ((uint16)0xffff)

#define MIN_INT32  ((int32)0x80000000)
#define MAX_INT32  ((int32)0x7fffffff)

#define MIN_UINT32 ((uint32)0)
#define MAX_UINT32 ((uint32)0xffffffff)

#define MIN_INT64  (CONST64(0x8000000000000000))
#define MAX_INT64  (CONST64(0x7fffffffffffffff))

#define MIN_UINT64 (CONST64U(0))
#define MAX_UINT64 (CONST64U(0xffffffffffffffff))

typedef uint8 *TCA;  /* Pointer into TC (usually). */

/*
 * Type big enough to hold an integer between 0..100
 */
typedef uint8 Percent;
#define AsPercent(v) ((Percent)(v))


typedef uintptr_t VA;
typedef uintptr_t VPN;

typedef uint64    PA;
typedef uint64    PPN;

typedef uint64    TPA;
typedef uint64    TPPN;

typedef uint64    PhysMemOff;
typedef uint64    PhysMemSize;

typedef uint64    BA;
#ifdef VMKERNEL
typedef void     *BPN;
#else
typedef uint64    BPN;
#endif

#define UINT64_2_BPN(u) ((BPN)(u))
#define BPN_2_UINT64(b) ((uint64)(b))

typedef uint64    PageCnt;
typedef uint64    PageNum;
typedef unsigned  MemHandle;
typedef unsigned  IoHandle;
typedef int32     World_ID;

/* !! do not alter the definition of INVALID_WORLD_ID without ensuring
 * that the values defined in both bora/public/vm_basic_types.h and
 * lib/vprobe/vm_basic_types.h are the same.  Additionally, the definition
 * of VMK_INVALID_WORLD_ID in vmkapi_world.h also must be defined with
 * the same value
 */

#define INVALID_WORLD_ID ((World_ID)0)

typedef World_ID User_CartelID;
#define INVALID_CARTEL_ID INVALID_WORLD_ID

typedef User_CartelID User_SessionID;
#define INVALID_SESSION_ID INVALID_CARTEL_ID

typedef User_CartelID User_CartelGroupID;
#define INVALID_CARTELGROUP_ID INVALID_CARTEL_ID

typedef uint32 Worldlet_ID;
#define INVALID_WORLDLET_ID ((Worldlet_ID)-1)

typedef  int8    Reg8;
typedef  int16   Reg16;
typedef  int32   Reg32;
typedef  int64   Reg64;

typedef uint8   UReg8;
typedef uint16  UReg16;
typedef uint32  UReg32;
typedef uint64  UReg64;

#if defined(__GNUC__) && defined(__SIZEOF_INT128__)
typedef  int128  Reg128;
typedef uint128 UReg128;
#endif

#if defined(VMM) || defined(COREQUERY) || defined(EXTDECODER) ||  \
    defined (VMKERNEL) || defined (VMKBOOT) || defined (ULM)
typedef  Reg64  Reg;
typedef UReg64 UReg;
#endif
typedef uint64 MA;
typedef uint32 MPN32;

/*
 * This type should be used for variables that contain sector
 * position/quantity.
 */
typedef uint64 SectorType;

/*
 * Linear address
 */

typedef uintptr_t LA;
typedef uintptr_t LPN;
#define LA_2_LPN(_la)     ((_la) >> PAGE_SHIFT)
#define LPN_2_LA(_lpn)    ((_lpn) << PAGE_SHIFT)

#define LAST_LPN   ((((LA)  1) << (8 * sizeof(LA)   - PAGE_SHIFT)) - 1)
#define LAST_LPN32 ((((LA32)1) << (8 * sizeof(LA32) - PAGE_SHIFT)) - 1)
#define LAST_LPN64 ((((LA64)1) << (8 * sizeof(LA64) - PAGE_SHIFT)) - 1)

/* Valid bits in a LPN. */
#define LPN_MASK   LAST_LPN
#define LPN_MASK32 LAST_LPN32
#define LPN_MASK64 LAST_LPN64

/*
 * On 64 bit platform, address and page number types default
 * to 64 bit. When we need to represent a 32 bit address, we use
 * types defined below.
 *
 * On 32 bit platform, the following types are the same as the
 * default types.
 */
typedef uint32 VA32;
typedef uint32 VPN32;
typedef uint32 LA32;
typedef uint32 LPN32;
typedef uint32 PA32;
typedef uint32 PPN32;

/*
 * On 64 bit platform, the following types are the same as the
 * default types.
 */
typedef uint64 VA64;
typedef uint64 VPN64;
typedef uint64 LA64;
typedef uint64 LPN64;
typedef uint64 PA64;
typedef uint64 PPN64;
typedef uint64 TPPN64;
typedef uint64 MA64;
typedef uint64 MPN;

/*
 * IO device DMA virtual address and page number (translated by IOMMU to
 * MA/MPN). IOPN can be in the inclusive range 0 -> MAX_IOPN.
 */
typedef uint64 IOA;
typedef uint64 IOPN;

/*
 * VA typedefs for user world apps.
 */
typedef VA32 UserVA32;
typedef VA64 UserVA64;
typedef UserVA64 UserVAConst; /* Userspace ptr to data that we may only read. */
typedef UserVA32 UserVA32Const; /* Userspace ptr to data that we may only read. */
typedef UserVA64 UserVA64Const; /* Used by 64-bit syscalls until conversion is finished. */
#ifdef VMKERNEL
typedef UserVA64 UserVA;
#else
typedef void * UserVA;
#endif


/* Maximal observable PPN value. */
#define MAX_PPN_BITS      33
#define MAX_PPN           (((PPN)1 << MAX_PPN_BITS) - 1)

#define INVALID_PPN       ((PPN)0x000fffffffffffffull)
#define INVALID_PPN32     ((PPN32)0xffffffff)
#define APIC_INVALID_PPN  ((PPN)0x000ffffffffffffeull)

#define INVALID_BPN       ((BPN)0x0000ffffffffffffull)

#define MPN38_MASK        ((1ull << 38) - 1)

#define RESERVED_MPN      ((MPN)0)
#define INVALID_MPN       ((MPN)MPN38_MASK)
#define MEMREF_MPN        ((MPN)MPN38_MASK - 1)
#define RELEASED_MPN      ((MPN)MPN38_MASK - 2)

/* account for special MPNs defined above */
#define MAX_MPN           ((MPN)MPN38_MASK - 3) /* 50 bits of address space */

#define INVALID_IOPN      ((IOPN)-1)
#define MAX_IOPN          (INVALID_IOPN - 1)

#define INVALID_LPN       ((LPN)-1)
#define INVALID_VPN       ((VPN)-1)
#define INVALID_LPN64     ((LPN64)-1)
#define INVALID_PAGENUM   ((PageNum)0x000000ffffffffffull)
#define INVALID_PAGENUM32 ((uint32)-1)

/*
 * Format modifier for printing VA, LA, and VPN.
 * Use them like this: Log("%#" FMTLA "x\n", laddr)
 */

#if defined(VMM) || defined(FROBOS64) || vm_x86_64 || vm_arm_64 || defined __APPLE__
#   define FMTLA "l"
#   define FMTVA "l"
#   define FMTVPN "l"
#else
#   define FMTLA ""
#   define FMTVA ""
#   define FMTVPN ""
#endif

#ifndef EXTERN
#define EXTERN        extern
#endif
#define CONST         const

#ifndef INLINE
#   ifdef _MSC_VER
       /*
        * On UWP(Universal Windows Platform),
        * Only X86 32bit support '__inline'
        */
#      if defined(VM_WIN_UWP) && !defined(VM_X86_32)
#            define INLINE
#      else
#            define INLINE        __inline
#      endif
#   else
#      define INLINE        inline
#   endif
#endif


/*
 * Annotation for data that may be exported into a DLL and used by other
 * apps that load that DLL and import the data.
 */
#if defined(_WIN32) && defined(VMX86_IMPORT_DLLDATA)
#  define VMX86_EXTERN_DATA       extern __declspec(dllimport)
#else // !_WIN32
#  define VMX86_EXTERN_DATA       extern
#endif

#ifdef _WIN32

/* under windows, __declspec(thread) is supported since VS 2003 */
#define __thread __declspec(thread)

#else

/*
 * under other platforms instead, __thread is supported by gcc since
 * version 3.3.1 and by clang since version 3.x
 */

#endif


/*
 * Due to the wonderful "registry redirection" feature introduced in
 * 64-bit Windows, if you access any key under HKLM\Software in 64-bit
 * code, you need to open/create/delete that key with
 * VMKEY_WOW64_32KEY if you want a consistent view with 32-bit code.
 */

#ifdef _WIN32
#ifdef _WIN64
#define VMW_KEY_WOW64_32KEY KEY_WOW64_32KEY
#else
#define VMW_KEY_WOW64_32KEY 0x0
#endif
#endif


/*
 * At present, we effectively require a compiler that is at least
 * gcc-3.3 (circa 2003).  Enforce this here, various things below
 * this line depend upon it.
 *
 * In practice, most things presently compile with gcc-4.1 or gcc-4.4.
 * The various linux kernel modules may use older (gcc-3.3) compilers.
 */
#if defined __GNUC__ && (__GNUC__ < 3 || (__GNUC__ == 3 && __GNUC_MINOR__ < 3))
#error "gcc version is too old to compile assembly, need gcc-3.3 or better"
#endif

/*
 * Similarly, we require a compiler that is at least vc80 (vs2005).
 * Enforce this here.
 */
#if defined _MSC_VER && _MSC_VER < 1400
#error "cl.exe version is too old, need vc80 or better"
#endif


/*
 * Consider the following reasons functions are inlined:
 *
 *  1) inlined for performance reasons
 *  2) inlined because it's a single-use function
 *
 * Functions which meet only condition 2 should be marked with this
 * inline macro; It is not critical to be inlined (but there is a
 * code-space & runtime savings by doing so), so when other callers
 * are added the inline-ness should be removed.
 */

#if defined __GNUC__
/*
 * Starting at version 3.3, gcc does not always inline functions marked
 * 'inline' (it depends on their size and other factors). To force gcc
 * to inline a function, one must use the __always_inline__ attribute.
 * This attribute should be used sparingly and with care.  It is usually
 * preferable to let gcc make its own inlining decisions
 */
#   define INLINE_ALWAYS INLINE __attribute__((__always_inline__))
#else
#   define INLINE_ALWAYS INLINE
#endif
#define INLINE_SINGLE_CALLER INLINE_ALWAYS

/*
 * Used when a hard guarantee of no inlining is needed. Very few
 * instances need this since the absence of INLINE is a good hint
 * that gcc will not do inlining.
 */

#if defined(__GNUC__)
#define ABSOLUTELY_NOINLINE __attribute__((__noinline__))
#elif defined(_MSC_VER)
#define ABSOLUTELY_NOINLINE __declspec(noinline)
#endif

/*
 * Used when a function has no effects except the return value and the
 * return value depends only on the parameters and/or global variables
 * Such a function can be subject to common subexpression elimination
 * and loop optimization just as an arithmetic operator would be.
 */

#if defined(__GNUC__) && (defined(VMM) || defined (VMKERNEL))
#define SIDE_EFFECT_FREE __attribute__((__pure__))
#else
#define SIDE_EFFECT_FREE
#endif

/*
 * Used when a function exmaines no input other than its arguments and
 * has no side effects other than its return value.  Stronger than
 * SIDE_EFFECT_FREE as the function is not allowed to read from global
 * memory.
 */

#if defined(__GNUC__) && (defined(VMM) || defined (VMKERNEL))
#define CONST_FUNCTION __attribute__((__const__))
#else
#define CONST_FUNCTION
#endif

/*
 * Attributes placed on function declarations to tell the compiler
 * that the function never returns.
 */

#ifdef _MSC_VER
#define NORETURN __declspec(noreturn)
#elif defined __GNUC__
#define NORETURN __attribute__((__noreturn__))
#else
#define NORETURN
#endif

/*
 * Static profiling hints for functions.
 *    A function can be either hot, cold, or neither.
 *    It is an error to specify both hot and cold for the same function.
 *    Note that there is no annotation for "neither."
 */

#if defined __GNUC__ && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#define HOT __attribute__((hot))
#define COLD __attribute__((cold))
#else
#define HOT
#define COLD
#endif

/*
 * Branch prediction hints:
 *     LIKELY(exp)   - Expression exp is likely TRUE.
 *     UNLIKELY(exp) - Expression exp is likely FALSE.
 *   Usage example:
 *        if (LIKELY(excCode == EXC_NONE)) {
 *               or
 *        if (UNLIKELY(REAL_MODE(vc))) {
 *
 * We know how to predict branches on gcc3 and later (hopefully),
 * all others we don't so we do nothing.
 */

#if defined __GNUC__
/*
 * gcc3 uses __builtin_expect() to inform the compiler of an expected value.
 * We use this to inform the static branch predictor. The '!!' in LIKELY
 * will convert any !=0 to a 1.
 */
#define LIKELY(_exp)     __builtin_expect(!!(_exp), 1)
#define UNLIKELY(_exp)   __builtin_expect(!!(_exp), 0)
#else
#define LIKELY(_exp)      (_exp)
#define UNLIKELY(_exp)    (_exp)
#endif

/*
 * GCC's argument checking for printf-like functions
 * This is conditional until we have replaced all `"%x", void *'
 * with `"0x%08x", (uint32) void *'. Note that %p prints different things
 * on different platforms.  Argument checking is enabled for the
 * vmkernel, which has already been cleansed.
 *
 * fmtPos is the position of the format string argument, beginning at 1
 * varPos is the position of the variable argument, beginning at 1
 */

#if defined(__GNUC__)
# define PRINTF_DECL(fmtPos, varPos) __attribute__((__format__(__printf__, fmtPos, varPos)))
#else
# define PRINTF_DECL(fmtPos, varPos)
#endif

#if defined(__GNUC__)
# define SCANF_DECL(fmtPos, varPos) __attribute__((__format__(__scanf__, fmtPos, varPos)))
#else
# define SCANF_DECL(fmtPos, varPos)
#endif

/*
 * UNUSED_PARAM should surround the parameter name and type declaration,
 * e.g. "int MyFunction(int var1, UNUSED_PARAM(int var2))"
 *
 */

#ifndef UNUSED_PARAM
# if defined(__GNUC__)
#  define UNUSED_PARAM(_parm) _parm  __attribute__((__unused__))
# elif defined _MSC_VER
#  define UNUSED_PARAM(_parm) __pragma(warning(suppress:4100)) _parm
# else
#  define UNUSED_PARAM(_parm) _parm
# endif
#endif

#ifndef UNUSED_TYPE
// XXX _Pragma would better but doesn't always work right now.
#  define UNUSED_TYPE(_parm) UNUSED_PARAM(_parm)
#endif

#ifndef UNUSED_VARIABLE
// XXX is there a better way?
#  define UNUSED_VARIABLE(_var) (void)_var
#endif

/*
 * gcc can warn us if we're ignoring returns
 */
#if defined(__GNUC__)
# define MUST_CHECK_RETURN __attribute__((warn_unused_result))
#else
# define MUST_CHECK_RETURN
#endif

/*
 * ALIGNED specifies minimum alignment in "n" bytes.
 */

#ifdef __GNUC__
#define ALIGNED(n) __attribute__((__aligned__(n)))
#else
#define ALIGNED(n)
#endif


/*
 * Encapsulate the syntactic differences between gcc and msvc alignment control.
 * BOUNDARY must match in the prefix and suffix.
 */

#ifdef _WIN32
#define ALIGN_PREFIX(BOUNDRY) __declspec(align(BOUNDRY))
#define ALIGN_SUFFIX(BOUNDRY)
#else
#define ALIGN_PREFIX(BOUNDRY)
#define ALIGN_SUFFIX(BOUNDRY) __attribute__((__aligned__(BOUNDRY)))
#endif


/*
 * Once upon a time, this was used to silence compiler warnings that
 * get generated when the compiler thinks that a function returns
 * when it is marked noreturn.  Don't do it.  Use NOT_REACHED().
 */

#define INFINITE_LOOP()           do { } while (1)

/*
 * Format modifier for printing pid_t.  On sun the pid_t is a ulong, but on
 * Linux it's an int.
 * Use this like this: printf("The pid is %" FMTPID ".\n", pid);
 */
#ifdef sun
#   ifdef VM_X86_64
#      define FMTPID "d"
#   else
#      define FMTPID "lu"
#   endif
#else
# define FMTPID "d"
#endif

/*
 * Format modifier for printing uid_t.  On Solaris 10 and earlier, uid_t
 * is a ulong, but on other platforms it's an unsigned int.
 * Use this like this: printf("The uid is %" FMTUID ".\n", uid);
 */
#if defined(sun) && !defined(SOL11)
#   ifdef VM_X86_64
#      define FMTUID "u"
#   else
#      define FMTUID "lu"
#   endif
#else
# define FMTUID "u"
#endif

/*
 * Format modifier for printing mode_t.  On sun the mode_t is a ulong, but on
 * Linux it's an int.
 * Use this like this: printf("The mode is %" FMTMODE ".\n", mode);
 */
#ifdef sun
#   ifdef VM_X86_64
#      define FMTMODE "o"
#   else
#      define FMTMODE "lo"
#   endif
#else
# define FMTMODE "o"
#endif

#ifdef __APPLE__
/*
 * Format specifier for all these annoying types such as {S,U}Int32
 * which are 'long' in 32-bit builds
 *       and  'int' in 64-bit builds.
 */
#   ifdef __LP64__
#      define FMTLI ""
#   else
#      define FMTLI "l"
#   endif

/*
 * Format specifier for all these annoying types such as NS[U]Integer
 * which are  'int' in 32-bit builds
 *       and 'long' in 64-bit builds.
 */
#   ifdef __LP64__
#      define FMTIL "l"
#   else
#      define FMTIL ""
#   endif
#endif


/*
 * Define type for poll device handles.
 */

typedef int64 PollDevHandle;

/*
 * Define the utf16_t type.
 */

#if defined(_WIN32) && defined(_NATIVE_WCHAR_T_DEFINED)
typedef wchar_t utf16_t;
#else
typedef uint16 utf16_t;
#endif

/*
 * Define for point and rectangle types.  Defined here so they
 * can be used by other externally facing headers in bora/public.
 */

typedef struct VMPoint {
   int x, y;
} VMPoint;

#if defined _WIN32 && defined USERLEVEL
struct tagRECT;
typedef struct tagRECT VMRect;
#else
typedef struct VMRect {
   int left;
   int top;
   int right;
   int bottom;
} VMRect;
#endif

/*
 * ranked locks "everywhere"
 */

typedef uint32 MX_Rank;

#endif  /* _VM_BASIC_TYPES_H_ */
