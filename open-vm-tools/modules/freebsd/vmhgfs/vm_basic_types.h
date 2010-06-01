/*********************************************************
 * Copyright (C) 1998-2008 VMware, Inc. All rights reserved.
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
 *
 * vm_basic_types.h --
 *
 *    basic data types.
 */


#ifndef _VM_BASIC_TYPES_H_
#define _VM_BASIC_TYPES_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMNIXMOD
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMKDRIVERS
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMIROM
#include "includeCheck.h"

/* STRICT ANSI means the Xserver build and X defines Bool differently. */
#if !defined(__STRICT_ANSI__) || defined(__FreeBSD__)
typedef char           Bool;
#endif

#ifndef FALSE
#define FALSE          0
#endif

#ifndef TRUE
#define TRUE           1
#endif

#define IsBool(x)      (((x) & ~1) == 0)
#define IsBool2(x, y)  ((((x) | (y)) & ~1) == 0)

/*
 * Macros __i386__ and __ia64 are intrinsically defined by GCC
 */
#if defined _MSC_VER && defined _M_X64
#  define __x86_64__
#elif defined _MSC_VER && defined _M_IX86
#  define __i386__
#endif

#ifdef __i386__
#define VM_I386
#endif

#ifdef __arm__
#define VM_ARM
#endif

#ifdef __x86_64__
#define VM_X86_64
#define VM_I386
#define vm_x86_64 (1)
#else
#define vm_x86_64 (0)
#endif


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

#if defined(__APPLE__) || defined(HAVE_STDINT_H)

/*
 * TODO: This is a C99 standard header.  We should be able to test for
 * #if __STDC_VERSION__ >= 199901L, but that breaks the Netware build
 * (which doesn't have stdint.h).
 */

#include <stdint.h>

typedef uint64_t uint64;
typedef int64_t int64;
typedef uint32_t uint32;
typedef int32_t int32;
typedef uint16_t uint16;
typedef int16_t int16;
typedef uint8_t uint8;

/*
 * XXX: int8_t is defined to be 'signed char' on Mac hosts.
 *
 * Unfortunately, GCC 4.0.1 warns when doing pointer assignment or
 * comparison between signed char * and char * (even if char is
 * signed).
 *
 * If we want to use int8_t to define int8, we need to go through and
 * replace uses of char * with signed char * to prevent warnings.
 */
typedef char int8;

#else /* !HAVE_STDINT_H */

#ifdef _MSC_VER

typedef unsigned __int64 uint64;
typedef signed __int64 int64;

#elif __GNUC__
/* The Xserver source compiles with -ansi -pendantic */
#ifndef __STRICT_ANSI__
#if defined(VM_X86_64)
typedef unsigned long uint64;
typedef long int64;
#else
typedef unsigned long long uint64;
typedef long long int64;
#endif
#elif defined __FreeBSD__
typedef unsigned long long uint64;
typedef long long int64;
#endif
#else
#error - Need compiler define for int64/uint64
#endif /* _MSC_VER */

typedef unsigned int       uint32;
typedef unsigned short     uint16;
typedef unsigned char      uint8;

typedef int       int32;
typedef short     int16;
typedef char      int8;

#endif /* HAVE_STDINT_H */

/*
 * FreeBSD (for the tools build) unconditionally defines these in
 * sys/inttypes.h so don't redefine them if this file has already
 * been included. [greg]
 *
 * This applies to Solaris as well.
 */

/*
 * Before trying to do the includes based on OS defines, see if we can use
 * feature-based defines to get as much functionality as possible
 */

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
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
#      ifdef KLD_MODULE
#         include <sys/types.h>
#      else
#         if __FreeBSD_version >= 500043
#            if !defined(VMKERNEL)
#               include <inttypes.h>
#            endif
#            include <sys/types.h>
#         else
#            include <sys/inttypes.h>
#         endif
#      endif
#   elif defined __APPLE__
#      if KERNEL
#         include <sys/unistd.h>
#         include <sys/types.h> /* mostly for size_t */
#         include <stdint.h>
#      else
#         include <unistd.h>
#         include <inttypes.h>
#         include <stdlib.h>
#         include <stdint.h>
#      endif
#   else
#      if !defined(__intptr_t_defined) && !defined(intptr_t)
#         define __intptr_t_defined
#         define intptr_t  intptr_t
#         ifdef VM_X86_64
             typedef int64     intptr_t;
#         elif defined VM_I386 || defined VM_ARM
             typedef int32     intptr_t;
#         endif
#      endif

#      ifndef _STDINT_H
#         ifdef VM_X86_64
             typedef uint64    uintptr_t;
#         elif defined VM_I386 || defined VM_ARM
             typedef uint32    uintptr_t;
#         endif
#      endif
#   endif
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
 *    printf("%"FMT64"d\n", big);
 *
 * FMTH is for handles/fds.
 */

#ifdef _MSC_VER
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
#elif defined __APPLE__
   /* Mac OS hosts use the same formatters for 32- and 64-bit. */
   #define FMT64 "ll"
   #define FMTSZ "z"
   #define FMTPD "l"
   #define FMTH ""
#elif __GNUC__
   #define FMTH ""
   #if defined(N_PLAT_NLM) || defined(sun) || \
       (defined(__FreeBSD__) && (__FreeBSD__ + 0) && ((__FreeBSD__ + 0) < 5))
      /*
       * Why (__FreeBSD__ + 0)?  See bug 141008.
       * Yes, we really need to test both (__FreeBSD__ + 0) and
       * ((__FreeBSD__ + 0) < 5).  No, we can't remove "+ 0" from
       * ((__FreeBSD__ + 0) < 5).
       */
      #ifdef VM_X86_64
         #define FMTSZ  "l"
         #define FMTPD  "l"
      #else
         #define FMTSZ  ""
         #define FMTPD  ""
      #endif
   #elif defined(__linux__) \
      || (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L) \
      || (defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L) \
      || (defined(_POSIX2_VERSION) && _POSIX2_VERSION >= 200112L)
      /* BSD, Linux */
      #define FMTSZ     "z"

      #if defined(VM_X86_64)
         #define FMTPD  "l"
      #else
         #define FMTPD  ""
      #endif
   #else
      /* Systems with a pre-C99 libc */
      #define FMTSZ     "Z"
      #ifdef VM_X86_64
         #define FMTPD  "l"
      #else
         #define FMTPD  ""
      #endif
   #endif
   #ifdef VM_X86_64
      #define FMT64     "l"
   #elif defined(sun) || defined(__FreeBSD__)
      #define FMT64     "ll"
   #else
      #define FMT64     "L"
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
#ifdef VM_X86_64
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

#ifdef VM_X86_64
    #define CONST3264(a) CONST64(a)
    #define CONST3264U(a) CONST64U(a)
#else
    #define CONST3264(a) (a)
    #define CONST3264U(a) (a)
#endif

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
#define AsPercent(v)	((Percent)(v))
#define CHOOSE_PERCENT  AsPercent(101)


typedef uintptr_t VA;
typedef uintptr_t VPN;

typedef uint64    PA;
typedef uint32    PPN;

typedef uint64    PhysMemOff;
typedef uint64    PhysMemSize;

/* The Xserver source compiles with -ansi -pendantic */
#ifndef __STRICT_ANSI__
typedef uint64    BA;
#endif
typedef uint32    BPN;
typedef uint32    PageNum;
typedef unsigned  MemHandle;
typedef int32     World_ID;

#define INVALID_WORLD_ID ((World_ID)-1)

typedef World_ID User_CartelID;
#define INVALID_CARTEL_ID INVALID_WORLD_ID

typedef User_CartelID User_SessionID;
#define INVALID_SESSION_ID INVALID_CARTEL_ID

typedef User_CartelID User_CartelGroupID;
#define INVALID_CARTELGROUP_ID INVALID_CARTEL_ID

typedef uint32 Worldlet_ID;
#define INVALID_WORLDLET_ID ((Worldlet_ID)-1)

/* world page number */
typedef uint32    WPN;

/* The Xserver source compiles with -ansi -pendantic */
#ifndef __STRICT_ANSI__
typedef uint64     MA;
typedef uint32     MPN;
#endif

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
typedef uint32 MA32;
typedef uint32 MPN32;

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
typedef uint64 MA64;
typedef uint64 MPN64;

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


/*
 * Maximal possible PPN value (errors too) that PhysMem can handle.
 * Must be at least as large as MAX_PPN which is the maximum PPN
 * for any region other than buserror.
 */
#define PHYSMEM_MAX_PPN ((PPN)0xffffffff)
#define MAX_PPN         ((PPN)0x1fffffff)   /* Maximal observable PPN value. */
#define INVALID_PPN     ((PPN)0xffffffff)

#define INVALID_BPN  ((BPN) 0x1fffffff)

#define INVALID_MPN  ((MPN)-1)
#define MEMREF_MPN   ((MPN)-2)
#define RESERVED_MPN ((MPN) 0)
/* Support 43 bits of address space. */
#define MAX_MPN      ((MPN)0x7fffffff)

#define INVALID_LPN ((LPN)-1)
#define INVALID_VPN ((VPN)-1)
#define INVALID_LPN64 ((LPN64)-1)
#define INVALID_PAGENUM ((PageNum)-1)
#define INVALID_WPN ((WPN) -1)


/*
 * Format modifier for printing VA, LA, and VPN.
 * Use them like this: Log("%#"FMTLA"x\n", laddr)
 */

#if defined(VMM64) || defined(FROBOS64) || vm_x86_64 || defined __APPLE__
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
#      define INLINE        __inline
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

#if defined(_WIN32) && !defined(VMX86_NO_THREADS)
#define THREADSPECIFIC __declspec(thread)
#else
#define THREADSPECIFIC
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

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)
/*
 * Starting at version 3.3, gcc does not always inline functions marked
 * 'inline' (it depends on their size). To force gcc to do so, one must use the
 * extra __always_inline__ attribute.
 */
#   define INLINE_SINGLE_CALLER INLINE __attribute__((__always_inline__))
#   if    defined(VMM) \
       && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 1))
#      warning Verify INLINE_SINGLE_CALLER '__always_inline__' attribute (did \
             monitor size change?)
#   endif
#else
#   define INLINE_SINGLE_CALLER INLINE
#endif

/*
 * Used when a hard guaranteed of no inlining is needed. Very few
 * instances need this since the absence of INLINE is a good hint
 * that gcc will not do inlining.
 */

#if defined(__GNUC__) && defined(VMM)
#define ABSOLUTELY_NOINLINE __attribute__((__noinline__))
#endif

/*
 * Attributes placed on function declarations to tell the compiler
 * that the function never returns.
 */

#ifdef _MSC_VER
#define NORETURN __declspec(noreturn)
#elif __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 9)
#define NORETURN __attribute__((__noreturn__))
#else
#define NORETURN
#endif

/*
 * GCC 3.2 inline asm needs the + constraint for input/ouput memory operands.
 * Older GCCs don't know about it --hpreg
 */

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 2)
#   define VM_ASM_PLUS 1
#else
#   define VM_ASM_PLUS 0
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

#if (__GNUC__ >= 3)
/*
 * gcc3 uses __builtin_expect() to inform the compiler of an expected value.
 * We use this to inform the static branch predictor. The '!!' in LIKELY
 * will convert any !=0 to a 1.
 */
#define LIKELY(_exp)     __builtin_expect(!!(_exp), 1)
#define UNLIKELY(_exp)   __builtin_expect((_exp), 0)
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
# else
#  define UNUSED_PARAM(_parm) _parm
# endif
#endif

/*
 * REGPARM defaults to REGPARM3, i.e., a requent that gcc
 * puts the first three arguments in registers.  (It is fine
 * if the function has fewer than three args.)  Gcc only.
 * Syntactically, put REGPARM where you'd put INLINE or NORETURN.
 */

#if defined(__GNUC__)
# define REGPARM0 __attribute__((regparm(0)))
# define REGPARM1 __attribute__((regparm(1)))
# define REGPARM2 __attribute__((regparm(2)))
# define REGPARM3 __attribute__((regparm(3)))
# define REGPARM REGPARM3
#else
# define REGPARM0
# define REGPARM1
# define REGPARM2
# define REGPARM3
# define REGPARM
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
 ***********************************************************************
 * STRUCT_OFFSET_CHECK --                                    */ /**
 *
 * \brief Check if the actual offsef of a member in a structure
 *        is what is expected
 *
 *
 * \param[in]  STRUCT       Structure the member is a part of.
 * \param[in]  MEMBER       Member to check the offset of.
 * \param[in]  OFFSET       Expected offset of MEMBER in STRUCTURE.
 * \param[in]  DEBUG_EXTRA  Additional bytes to be added to OFFSET to
 *                          compensate for extra info in debug builds.
 *
 ***********************************************************************
 */
#ifdef VMX86_DEBUG
#define STRUCT_OFFSET_CHECK(STRUCT, MEMBER, OFFSET, DEBUG_EXTRA) \
  ASSERT_ON_COMPILE(vmk_offsetof(STRUCT, MEMBER) == (OFFSET + DEBUG_EXTRA))
#else
#define STRUCT_OFFSET_CHECK(STRUCT, MEMBER, OFFSET, DEBUG_EXTRA) \
  ASSERT_ON_COMPILE(vmk_offsetof(STRUCT, MEMBER) == OFFSET)
#endif

/*
 ***********************************************************************
 * STRUCT_SIZE_CHECK --                                      */ /**
 *
 * \brief Check if the actual size of a structure is what is expected
 *
 *
 * \param[in]  STRUCT       Structure whose size is to be checked.
 * \param[in]  SIZE         Expected size of STRUCT.
 * \param[in]  DEBUG_EXTRA  Additional bytes to be added to SIZE to
 *                          compensate for extra info in debug builds.
 *
 ***********************************************************************
 */
#ifdef VMX86_DEBUG
#define STRUCT_SIZE_CHECK(STRUCT, SIZE, DEBUG_EXTRA) \
  ASSERT_ON_COMPILE(sizeof(STRUCT) == (SIZE + DEBUG_EXTRA))
#else
#define STRUCT_SIZE_CHECK(STRUCT, SIZE, DEBUG_EXTRA) \
  ASSERT_ON_COMPILE(sizeof(STRUCT) == SIZE)
#endif

/*
 * __func__ is a stringified function name that is part of the C99 standard. The block
 * below defines __func__ on older systems where the compiler does not support that
 * macro.
 */
#if defined(__GNUC__) \
   && ((__GNUC__ == 2 && __GNUC_MINOR < 96) \
       || (__GNUC__ < 2))
#   define __func__ __FUNCTION__
#endif

/*
 * Once upon a time, this was used to silence compiler warnings that
 * get generated when the compiler thinks that a function returns
 * when it is marked noreturn.  Don't do it.  Use NOT_REACHED().
 */

#define INFINITE_LOOP()           do { } while (1)

/*
 * On FreeBSD (for the tools build), size_t is typedef'd if _BSD_SIZE_T_
 * is defined. Use the same logic here so we don't define it twice. [greg]
 */
#ifdef __FreeBSD__
#   ifdef _BSD_SIZE_T_
#      undef _BSD_SIZE_T_
#      ifdef VM_I386
#         ifdef VM_X86_64
             typedef uint64 size_t;
#         else
             typedef uint32 size_t;
#         endif
#      endif /* VM_I386 */
#   endif

#   ifdef _BSD_SSIZE_T_
#      undef _BSD_SSIZE_T_
#      define _SSIZE_T
#      define __ssize_t_defined
#      define _SSIZE_T_DECLARED
#      ifdef VM_I386
#         ifdef VM_X86_64
             typedef int64 ssize_t;
#         else
             typedef int32 ssize_t;
#         endif
#      endif /* VM_I386 */
#   endif

#else
#   ifndef _SIZE_T
#      define _SIZE_T
#      ifdef VM_X86_64
          typedef uint64 size_t;
#      elif defined VM_I386 || defined VM_ARM
          typedef uint32 size_t;
#      endif
#   endif

#   if !defined(FROBOS) && !defined(_SSIZE_T) && !defined(ssize_t) && \
       !defined(__ssize_t_defined) && !defined(_SSIZE_T_DECLARED)
#      define _SSIZE_T
#      define __ssize_t_defined
#      define _SSIZE_T_DECLARED
#      ifdef VM_X86_64
          typedef int64 ssize_t;
#      elif defined VM_I386 || defined VM_ARM
          typedef int32 ssize_t;
#      endif
#   endif

#endif

/*
 * Format modifier for printing pid_t.  On sun the pid_t is a ulong, but on
 * Linux it's an int.
 * Use this like this: printf("The pid is %"FMTPID".\n", pid);
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
 * Use this like this: printf("The uid is %"FMTUID".\n", uid);
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
 * Use this like this: printf("The mode is %"FMTMODE".\n", mode);
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

/*
 * Format modifier for printing time_t. Most platforms define a time_t to be
 * a long int, but on FreeBSD (as of 5.0, it seems), the time_t is a signed
 * size quantity. Refer to the definition of FMTSZ to see why we need silly
 * preprocessor arithmetic.
 * Use this like this: printf("The mode is %"FMTTIME".\n", time);
 */
#if defined(__FreeBSD__) && (__FreeBSD__ + 0) && ((__FreeBSD__ + 0) >= 5)
#   define FMTTIME FMTSZ"d"
#else
#   define FMTTIME "ld"
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
 * Define MXSemaHandle here so both vmmon and vmx see this definition.
 */

#ifdef _WIN32
typedef uintptr_t MXSemaHandle;
#else
typedef int MXSemaHandle;
#endif

/*
 * Define type for poll device handles.
 */

#ifdef _WIN32
typedef uintptr_t PollDevHandle;
#else
typedef int PollDevHandle;
#endif

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

#endif  /* _VM_BASIC_TYPES_H_ */
