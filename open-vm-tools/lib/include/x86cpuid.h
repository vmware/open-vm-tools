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

#ifndef _X86CPUID_H_
#define _X86CPUID_H_

/* http://www.sandpile.org/ia32/cpuid.htm */

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMX

#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "community_source.h"
#include "x86vendor.h"
#include "vm_assert.h"

#if defined __cplusplus
extern "C" {
#endif


/*
 * The linux kernel's ptrace.h stupidly defines the bare
 * EAX/EBX/ECX/EDX, which wrecks havoc with our preprocessor tricks.
 */
#undef EAX
#undef EBX
#undef ECX
#undef EDX

typedef struct CPUIDRegs {
   uint32 eax, ebx, ecx, edx;
} CPUIDRegs;

typedef union CPUIDRegsUnion {
   uint32 array[4];
   CPUIDRegs regs;
} CPUIDRegsUnion;

/*
 * Results of calling cpuid(eax, ecx) on all host logical CPU.
 */
#ifdef _MSC_VER
// TODO: Move this under the push
#pragma warning (disable :4200) // non-std extension: zero-sized array in struct
#pragma warning (push)
#pragma warning (disable :4100) // unreferenced parameters
#endif

typedef
#include "vmware_pack_begin.h"
struct CPUIDReply {
   /*
    * Unique host logical CPU identifier. It does not change across queries, so
    * we use it to correlate the replies of multiple queries.
    */
   uint64 tag;                // OUT

   CPUIDRegs regs;            // OUT
}
#include "vmware_pack_end.h"
CPUIDReply;

typedef
#include "vmware_pack_begin.h"
struct CPUIDQuery {
   uint32 eax;                // IN
   uint32 ecx;                // IN
   uint32 numLogicalCPUs;     // IN/OUT
   CPUIDReply logicalCPUs[0]; // OUT
}
#include "vmware_pack_end.h"
CPUIDQuery;

/*
 * CPUID levels the monitor caches.
 *
 * The first parameter defines whether the level has its default masks
 * generated from the values in this file. Any level which is marked as FALSE
 * here *must* have all monitor support types set to NA. A static assert in
 * lib/cpuidcompat/cpuidcompat.c will check this.
 *
 * The second parameter is the "short name" of the level. It's mainly used for
 * token concatenation in various macros.
 *
 * The third parameter is the actual numeric value of that level (the EAX input
 * value).
 *
 * The fourth parameter is a "subleaf count", where 0 means that ecx is
 * ignored, otherwise is the count of sub-leaves.
 *
 * The fifth parameter is the first hardware version that is *aware* of the
 * CPUID level (0 = existed since dawn of time), even though we may not expose
 * this level or parts of it to guest.
 */

#define CPUID_CACHED_LEVELS                         \
   CPUIDLEVEL(TRUE,  0,   0,          0,  0)        \
   CPUIDLEVEL(TRUE,  1,   1,          0,  0)        \
   CPUIDLEVEL(FALSE, 2,   2,          0,  0)        \
   CPUIDLEVEL(FALSE, 4,   4,          7,  0)        \
   CPUIDLEVEL(FALSE, 5,   5,          0,  0)        \
   CPUIDLEVEL(TRUE,  6,   6,          0,  0)        \
   CPUIDLEVEL(TRUE,  7,   7,          1,  0)        \
   CPUIDLEVEL(FALSE, A,   0xA,        0,  0)        \
   CPUIDLEVEL(FALSE, B,   0xB,        2,  0)        \
   CPUIDLEVEL(TRUE,  D,   0xD,       10,  0)        \
   CPUIDLEVEL(TRUE,  F,   0xF,        2, 13)        \
   CPUIDLEVEL(TRUE,  10,  0x10,       2, 13)        \
   CPUIDLEVEL(TRUE,  12,  0x12,       4, 13)        \
   CPUIDLEVEL(TRUE,  14,  0x14,       2, 13)        \
   CPUIDLEVEL(TRUE,  15,  0x15,       0, 13)        \
   CPUIDLEVEL(TRUE,  16,  0x16,       0, 13)        \
   CPUIDLEVEL(TRUE,  17,  0x17,       4, 14)        \
   CPUIDLEVEL(FALSE, 400, 0x40000000, 0,  0)        \
   CPUIDLEVEL(FALSE, 401, 0x40000001, 0,  0)        \
   CPUIDLEVEL(FALSE, 402, 0x40000002, 0,  0)        \
   CPUIDLEVEL(FALSE, 403, 0x40000003, 0,  0)        \
   CPUIDLEVEL(FALSE, 404, 0x40000004, 0,  0)        \
   CPUIDLEVEL(FALSE, 405, 0x40000005, 0,  0)        \
   CPUIDLEVEL(FALSE, 406, 0x40000006, 0,  0)        \
   CPUIDLEVEL(FALSE, 410, 0x40000010, 0,  0)        \
   CPUIDLEVEL(FALSE, 80,  0x80000000, 0,  0)        \
   CPUIDLEVEL(TRUE,  81,  0x80000001, 0,  0)        \
   CPUIDLEVEL(FALSE, 82,  0x80000002, 0,  0)        \
   CPUIDLEVEL(FALSE, 83,  0x80000003, 0,  0)        \
   CPUIDLEVEL(FALSE, 84,  0x80000004, 0,  0)        \
   CPUIDLEVEL(FALSE, 85,  0x80000005, 0,  0)        \
   CPUIDLEVEL(FALSE, 86,  0x80000006, 0,  0)        \
   CPUIDLEVEL(FALSE, 87,  0x80000007, 0,  0)        \
   CPUIDLEVEL(TRUE,  88,  0x80000008, 0,  0)        \
   CPUIDLEVEL(TRUE,  8A,  0x8000000A, 0,  0)        \
   CPUIDLEVEL(FALSE, 819, 0x80000019, 0,  0)        \
   CPUIDLEVEL(FALSE, 81A, 0x8000001A, 0,  0)        \
   CPUIDLEVEL(FALSE, 81B, 0x8000001B, 0,  0)        \
   CPUIDLEVEL(FALSE, 81C, 0x8000001C, 0,  0)        \
   CPUIDLEVEL(FALSE, 81D, 0x8000001D, 5,  0)        \
   CPUIDLEVEL(FALSE, 81E, 0x8000001E, 0,  0)        \
   CPUIDLEVEL(TRUE,  81F, 0x8000001F, 0, 14)

#define CPUID_ALL_LEVELS CPUID_CACHED_LEVELS

/* Define cached CPUID levels in the form: CPUID_LEVEL_<ShortName> */
typedef enum {
#define CPUIDLEVEL(t, s, v, c, h) CPUID_LEVEL_##s,
   CPUID_CACHED_LEVELS
#undef CPUIDLEVEL
   CPUID_NUM_CACHED_LEVELS
} CpuidCachedLevel;

/* Enum to translate between shorthand name and actual CPUID level value. */
enum {
#define CPUIDLEVEL(t, s, v, c, h) CPUID_LEVEL_VAL_##s = v,
   CPUID_ALL_LEVELS
#undef CPUIDLEVEL
};


/* Named feature leaves */
#define CPUID_FEATURE_INFORMATION  0x01
#define CPUID_PROCESSOR_TOPOLOGY   4
#define CPUID_MWAIT_FEATURES       5
#define CPUID_XSAVE_FEATURES       0xd
#define CPUID_SGX_FEATURES         0x12
#define CPUID_PT_FEATURES          0x14
#define CPUID_HYPERVISOR_LEVEL_0   0x40000000
#define CPUID_SVM_FEATURES         0x8000000a
#define CPUID_SEV_INFO             0x8000001f

/*
 * CPUID result registers
 */

#define CPUID_REGS                              \
   CPUIDREG(EAX, eax)                           \
   CPUIDREG(EBX, ebx)                           \
   CPUIDREG(ECX, ecx)                           \
   CPUIDREG(EDX, edx)

typedef enum {
#define CPUIDREG(uc, lc) CPUID_REG_##uc,
   CPUID_REGS
#undef CPUIDREG
   CPUID_NUM_REGS
} CpuidReg;

#define CPUID_INTEL_VENDOR_STRING       "GenuntelineI"
#define CPUID_AMD_VENDOR_STRING         "AuthcAMDenti"
#define CPUID_CYRIX_VENDOR_STRING       "CyriteadxIns"
#define CPUID_VIA_VENDOR_STRING         "CentaulsaurH"
#define CPUID_HYGON_VENDOR_STRING       "HygouinenGen"

#define CPUID_HYPERV_HYPERVISOR_VENDOR_STRING  "Microsoft Hv"
#define CPUID_KVM_HYPERVISOR_VENDOR_STRING     "KVMKVMKVM\0\0\0"
#define CPUID_VMWARE_HYPERVISOR_VENDOR_STRING  "VMwareVMware"
#define CPUID_XEN_HYPERVISOR_VENDOR_STRING     "XenVMMXenVMM"

#define CPUID_INTEL_VENDOR_STRING_FIXED "GenuineIntel"
#define CPUID_AMD_VENDOR_STRING_FIXED   "AuthenticAMD"
#define CPUID_CYRIX_VENDOR_STRING_FIXED "CyrixInstead"
#define CPUID_VIA_VENDOR_STRING_FIXED   "CentaurHauls"
#define CPUID_HYGON_VENDOR_STRING_FIXED "HygonGenuine"

/*
 * FIELD can be defined to process the CPUID information provided in the
 * following CPUID_FIELD_DATA macro.
 *
 * The first parameter is the CPUID level of the feature (must be defined in
 * CPUID_ALL_LEVELS, above).
 *
 * The second parameter is the CPUID sub-level (subleaf) of the feature. Please
 * make sure here the number is consistent with the "subleaf count" in
 * CPUIDLEVEL macro. I.e., if a feature is being added to a _new_ subleaf,
 * update the subleaf count above as well.
 *
 * The third parameter is the result register.
 *
 * The fourth and fifth parameters are the bit position of the field and the
 * width, respectively.
 *
 * The sixth is the name of the field.
 *
 * The seventh parameter specifies the monitor support characteristics for
 * this field. The value must be a valid CpuidFieldSupported value (omitting
 * CPUID_FIELD_SUPPORT_ for convenience). The meaning of those values are
 * described below.
 *
 * The eighth parameter specifies the first virtual hardware version that
 * implements the field (if 7th field is YES or ANY), or 0 (if 7th field is
 * NO or NA).  The value FUT means HW_VERSION_FUTURE.  The field's hardware
 * version must match the version in defaultMasks (cpuidcompat.c) if defined
 * there, and must be less than or equal to the version of the cpuid leaf
 * it's in.
 *
 * FLAG is defined identically to FIELD, but its accessors are more appropriate
 * for 1-bit flags, and compile-time asserts enforce that the size is 1 bit
 * wide.
 */


/*
 * CpuidFieldSupported is made up of the following values:
 *
 *     NO: A feature/field that IS NOT SUPPORTED by the monitor.  Even
 *     if the host supports this feature, we will never expose it to
 *     the guest.
 *
 *     YES: A feature/field that IS SUPPORTED by the monitor.  If the
 *     host supports this feature, we will expose it to the guest.  If
 *     not, then we will not set the feature.
 *
 *     ANY: A feature/field that IS ALWAYS SUPPORTED by the monitor.
 *     Even if the host does not support the feature, the monitor can
 *     expose the feature to the guest. As with "YES", the guest cpuid
 *     value defaults to the host/evc cpuid value.  But usually the
 *     guest cpuid value is recomputed at power on, ignoring the default
 *     value.
 *
 *     NA: Only legal for levels not masked/tested by default (see
 *     above for this definition).  Such fields must always be marked
 *     as NA.
 *
 * These distinctions can be translated into a common CPUID mask string as
 * follows:
 *
 *     YES --> "H" (Host).  We support the feature, so show it to the
 *     guest if the host has the feature.
 *
 *     ANY/NA --> "X" (Ignore).  By default, don't perform checks for
 *     this feature bit.  Per-GOS masks may choose to set this bit in
 *     the guest.  (e.g. the APIC feature bit is always set to 1.)
 *
 *     See lib/cpuidcompat/cpuidcompat.c for any possible overrides to
 *     these defaults.
 */

/*
 * XSAVEOPT was incorrectly missed until HWv11. See comment for
 * DisableReqHWVersion in vmFeatureCPUID.c for more detail.
 */

typedef enum {
   CPUID_FIELD_SUPPORTED_NO,
   CPUID_FIELD_SUPPORTED_YES,
   CPUID_FIELD_SUPPORTED_ANY,
   CPUID_FIELD_SUPPORTED_NA,
   CPUID_NUM_FIELD_SUPPORTEDS
} CpuidFieldSupported;

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_0                                            \
FIELD(  0,  0, EAX,  0, 32, NUMLEVELS,                           ANY,   4 ) \
FIELD(  0,  0, EBX,  0, 32, VENDOR1,                             YES,   4 ) \
FIELD(  0,  0, ECX,  0, 32, VENDOR3,                             YES,   4 ) \
FIELD(  0,  0, EDX,  0, 32, VENDOR2,                             YES,   4 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_1                                            \
FIELD(  1,  0, EAX,  0,  4, STEPPING,                            ANY,   4 ) \
FIELD(  1,  0, EAX,  4,  4, MODEL,                               ANY,   4 ) \
FIELD(  1,  0, EAX,  8,  4, FAMILY,                              YES,   4 ) \
FIELD(  1,  0, EAX, 12,  2, TYPE,                                ANY,   4 ) \
FIELD(  1,  0, EAX, 16,  4, EXTENDED_MODEL,                      ANY,   4 ) \
FIELD(  1,  0, EAX, 20,  8, EXTENDED_FAMILY,                     YES,   4 ) \
FIELD(  1,  0, EBX,  0,  8, BRAND_ID,                            ANY,   4 ) \
FIELD(  1,  0, EBX,  8,  8, CLFL_SIZE,                           ANY,   4 ) \
FIELD(  1,  0, EBX, 16,  8, LCPU_COUNT,                          ANY,   4 ) \
FIELD(  1,  0, EBX, 24,  8, APICID,                              ANY,   4 ) \
FLAG(   1,  0, ECX,  0,  1, SSE3,                                YES,   4 ) \
FLAG(   1,  0, ECX,  1,  1, PCLMULQDQ,                           YES,   4 ) \
FLAG(   1,  0, ECX,  2,  1, DTES64,                              NO,    0 ) \
FLAG(   1,  0, ECX,  3,  1, MWAIT,                               YES,   4 ) \
FLAG(   1,  0, ECX,  4,  1, DSCPL,                               NO,    0 ) \
FLAG(   1,  0, ECX,  5,  1, VMX,                                 YES,   8 ) \
FLAG(   1,  0, ECX,  6,  1, SMX,                                 YES, FUT ) \
FLAG(   1,  0, ECX,  7,  1, EIST,                                NO,    0 ) \
FLAG(   1,  0, ECX,  8,  1, TM2,                                 NO,    0 ) \
FLAG(   1,  0, ECX,  9,  1, SSSE3,                               YES,   4 ) \
FLAG(   1,  0, ECX, 10,  1, CNXTID,                              NO,    0 ) \
FLAG(   1,  0, ECX, 11,  1, SDBG,                                NO,    0 ) \
FLAG(   1,  0, ECX, 12,  1, FMA,                                 YES,   8 ) \
FLAG(   1,  0, ECX, 13,  1, CMPXCHG16B,                          YES,   4 ) \
FLAG(   1,  0, ECX, 14,  1, xTPR,                                NO,    0 ) \
FLAG(   1,  0, ECX, 15,  1, PDCM,                                NO,    0 ) \
FLAG(   1,  0, ECX, 17,  1, PCID,                                YES,   9 ) \
FLAG(   1,  0, ECX, 18,  1, DCA,                                 NO,    0 ) \
FLAG(   1,  0, ECX, 19,  1, SSE41,                               YES,   4 ) \
FLAG(   1,  0, ECX, 20,  1, SSE42,                               YES,   4 ) \
FLAG(   1,  0, ECX, 21,  1, x2APIC,                              ANY,   9 ) \
FLAG(   1,  0, ECX, 22,  1, MOVBE,                               YES,   4 ) \
FLAG(   1,  0, ECX, 23,  1, POPCNT,                              YES,   4 ) \
FLAG(   1,  0, ECX, 24,  1, TSC_DEADLINE,                        ANY,  11 ) \
FLAG(   1,  0, ECX, 25,  1, AES,                                 YES,   4 ) \
FLAG(   1,  0, ECX, 26,  1, XSAVE,                               YES,   8 ) \
FLAG(   1,  0, ECX, 27,  1, OSXSAVE,                             ANY,   8 ) \
FLAG(   1,  0, ECX, 28,  1, AVX,                                 YES,   8 ) \
FLAG(   1,  0, ECX, 29,  1, F16C,                                YES,   9 ) \
FLAG(   1,  0, ECX, 30,  1, RDRAND,                              YES,   9 ) \
FLAG(   1,  0, ECX, 31,  1, HYPERVISOR,                          ANY,   4 ) \
FLAG(   1,  0, EDX,  0,  1, FPU,                                 YES,   4 ) \
FLAG(   1,  0, EDX,  1,  1, VME,                                 YES,   4 ) \
FLAG(   1,  0, EDX,  2,  1, DE,                                  YES,   4 ) \
FLAG(   1,  0, EDX,  3,  1, PSE,                                 YES,   4 ) \
FLAG(   1,  0, EDX,  4,  1, TSC,                                 YES,   4 ) \
FLAG(   1,  0, EDX,  5,  1, MSR,                                 YES,   4 ) \
FLAG(   1,  0, EDX,  6,  1, PAE,                                 YES,   4 ) \
FLAG(   1,  0, EDX,  7,  1, MCE,                                 YES,   4 ) \
FLAG(   1,  0, EDX,  8,  1, CX8,                                 YES,   4 ) \
FLAG(   1,  0, EDX,  9,  1, APIC,                                ANY,   4 ) \
FLAG(   1,  0, EDX, 11,  1, SEP,                                 YES,   4 ) \
FLAG(   1,  0, EDX, 12,  1, MTRR,                                YES,   4 ) \
FLAG(   1,  0, EDX, 13,  1, PGE,                                 YES,   4 ) \
FLAG(   1,  0, EDX, 14,  1, MCA,                                 YES,   4 ) \
FLAG(   1,  0, EDX, 15,  1, CMOV,                                YES,   4 ) \
FLAG(   1,  0, EDX, 16,  1, PAT,                                 YES,   4 ) \
FLAG(   1,  0, EDX, 17,  1, PSE36,                               YES,   4 ) \
FLAG(   1,  0, EDX, 18,  1, PSN,                                 YES,   4 ) \
FLAG(   1,  0, EDX, 19,  1, CLFSH,                               YES,   4 ) \
FLAG(   1,  0, EDX, 21,  1, DS,                                  YES,   4 ) \
FLAG(   1,  0, EDX, 22,  1, ACPI,                                ANY,   4 ) \
FLAG(   1,  0, EDX, 23,  1, MMX,                                 YES,   4 ) \
FLAG(   1,  0, EDX, 24,  1, FXSR,                                YES,   4 ) \
FLAG(   1,  0, EDX, 25,  1, SSE,                                 YES,   4 ) \
FLAG(   1,  0, EDX, 26,  1, SSE2,                                YES,   4 ) \
FLAG(   1,  0, EDX, 27,  1, SS,                                  YES,   4 ) \
FLAG(   1,  0, EDX, 28,  1, HTT,                                 ANY,   7 ) \
FLAG(   1,  0, EDX, 29,  1, TM,                                  NO,    0 ) \
FLAG(   1,  0, EDX, 30,  1, IA64,                                NO,    0 ) \
FLAG(   1,  0, EDX, 31,  1, PBE,                                 NO,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_2                                            \
FIELD(  2,  0, EAX,  0,  8, LEAF2_COUNT,                         NA,    0 ) \
FIELD(  2,  0, EAX,  8,  8, LEAF2_CACHE1,                        NA,    0 ) \
FIELD(  2,  0, EAX, 16,  8, LEAF2_CACHE2,                        NA,    0 ) \
FIELD(  2,  0, EAX, 24,  8, LEAF2_CACHE3,                        NA,    0 ) \
FIELD(  2,  0, EBX,  0,  8, LEAF2_CACHE4,                        NA,    0 ) \
FIELD(  2,  0, EBX,  8,  8, LEAF2_CACHE5,                        NA,    0 ) \
FIELD(  2,  0, EBX, 16,  8, LEAF2_CACHE6,                        NA,    0 ) \
FIELD(  2,  0, EBX, 24,  8, LEAF2_CACHE7,                        NA,    0 ) \
FIELD(  2,  0, ECX,  0,  8, LEAF2_CACHE8,                        NA,    0 ) \
FIELD(  2,  0, ECX,  8,  8, LEAF2_CACHE9,                        NA,    0 ) \
FIELD(  2,  0, ECX, 16,  8, LEAF2_CACHE10,                       NA,    0 ) \
FIELD(  2,  0, ECX, 24,  8, LEAF2_CACHE11,                       NA,    0 ) \
FIELD(  2,  0, EDX,  0,  8, LEAF2_CACHE12,                       NA,    0 ) \
FIELD(  2,  0, EDX,  8,  8, LEAF2_CACHE13,                       NA,    0 ) \
FIELD(  2,  0, EDX, 16,  8, LEAF2_CACHE14,                       NA,    0 ) \
FIELD(  2,  0, EDX, 24,  8, LEAF2_CACHE15,                       NA,    0 ) \

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_4                                            \
FIELD(  4,  0, EAX,  0,  5, LEAF4_CACHE_TYPE,                    NA,    0 ) \
FIELD(  4,  0, EAX,  5,  3, LEAF4_CACHE_LEVEL,                   NA,    0 ) \
FLAG(   4,  0, EAX,  8,  1, LEAF4_CACHE_SELF_INIT,               NA,    0 ) \
FLAG(   4,  0, EAX,  9,  1, LEAF4_CACHE_FULLY_ASSOC,             NA,    0 ) \
FIELD(  4,  0, EAX, 14, 12, LEAF4_CACHE_NUMHT_SHARING,           NA,    0 ) \
FIELD(  4,  0, EAX, 26,  6, LEAF4_CORE_COUNT,                    NA,    0 ) \
FIELD(  4,  0, EBX,  0, 12, LEAF4_CACHE_LINE,                    NA,    0 ) \
FIELD(  4,  0, EBX, 12, 10, LEAF4_CACHE_PART,                    NA,    0 ) \
FIELD(  4,  0, EBX, 22, 10, LEAF4_CACHE_WAYS,                    NA,    0 ) \
FIELD(  4,  0, ECX,  0, 32, LEAF4_CACHE_SETS,                    NA,    0 ) \
FLAG(   4,  0, EDX,  0,  1, LEAF4_CACHE_WBINVD_NOT_GUARANTEED,   NA,    0 ) \
FLAG(   4,  0, EDX,  1,  1, LEAF4_CACHE_IS_INCLUSIVE,            NA,    0 ) \
FLAG(   4,  0, EDX,  2,  1, LEAF4_CACHE_COMPLEX_INDEXING,        NA,    0 )

/*     LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,              MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_5                                            \
FIELD(  5,  0, EAX,  0, 16, MWAIT_MIN_SIZE,                      NA,    0 ) \
FIELD(  5,  0, EBX,  0, 16, MWAIT_MAX_SIZE,                      NA,    0 ) \
FLAG(   5,  0, ECX,  0,  1, MWAIT_EXTENSIONS,                    NA,    0 ) \
FLAG(   5,  0, ECX,  1,  1, MWAIT_INTR_BREAK,                    NA,    0 ) \
FIELD(  5,  0, EDX,  0,  4, MWAIT_C0_SUBSTATE,                   NA,    0 ) \
FIELD(  5,  0, EDX,  4,  4, MWAIT_C1_SUBSTATE,                   NA,    0 ) \
FIELD(  5,  0, EDX,  8,  4, MWAIT_C2_SUBSTATE,                   NA,    0 ) \
FIELD(  5,  0, EDX, 12,  4, MWAIT_C3_SUBSTATE,                   NA,    0 ) \
FIELD(  5,  0, EDX, 16,  4, MWAIT_C4_SUBSTATE,                   NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_6                                            \
FLAG(   6,  0, EAX,  0,  1, THERMAL_SENSOR,                      NO,    0 ) \
FLAG(   6,  0, EAX,  1,  1, TURBO_MODE,                          NO,    0 ) \
FLAG(   6,  0, EAX,  2,  1, APIC_INVARIANT,                      ANY,   4 ) \
FLAG(   6,  0, EAX,  4,  1, PLN,                                 NO,    0 ) \
FLAG(   6,  0, EAX,  5,  1, ECMD,                                NO,    0 ) \
FLAG(   6,  0, EAX,  6,  1, PTM,                                 NO,    0 ) \
FLAG(   6,  0, EAX,  7,  1, HWP,                                 NO,    0 ) \
FLAG(   6,  0, EAX,  8,  1, HWP_NOTIFICATION,                    NO,    0 ) \
FLAG(   6,  0, EAX,  9,  1, HWP_ACTIVITY_WINDOW,                 NO,    0 ) \
FLAG(   6,  0, EAX, 10,  1, HWP_ENERGY_PERFORMANCE_PREFERENCE,   NO,    0 ) \
FLAG(   6,  0, EAX, 11,  1, HWP_PACKAGE_LEVEL_REQUEST,           NO,    0 ) \
FLAG(   6,  0, EAX, 13,  1, HDC,                                 NO,    0 ) \
FLAG(   6,  0, EAX, 19,  1, HW_FEEDBACK,                         NO,    0 ) \
FIELD(  6,  0, EBX,  0,  4, NUM_INTR_THRESHOLDS,                 NO,    0 ) \
FLAG(   6,  0, ECX,  0,  1, HW_COORD_FEEDBACK,                   NO,    0 ) \
FLAG(   6,  0, ECX,  1,  1, ACNT2,                               ANY,  13 ) \
FLAG(   6,  0, ECX,  3,  1, ENERGY_PERF_BIAS,                    NO,    0 ) \
FLAG(   6,  0, EDX,  0,  1, PERF_CAP_REPORTING,                  NO,    0 ) \
FLAG(   6,  0, EDX,  1,  1, ENERGY_CAP_REPORTING,                NO,    0 ) \
FIELD(  6,  0, EDX,  8,  4, HW_FEEDBACK_SIZE,                    NO,    0 ) \
FIELD(  6,  0, EDX, 16, 16, HW_FEEDBACK_INDEX,                   NO,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_7_EDX_10 \
FLAG(   7,  0, EDX, 10,  1, LEAF7_RSVD,                          NO,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_7                                            \
FLAG(   7,  0, EBX,  0,  1, FSGSBASE,                            YES,   9 ) \
FLAG(   7,  0, EBX,  1,  1, TSC_ADJUST,                          ANY,  11 ) \
FLAG(   7,  0, EBX,  2,  1, SGX,                                 ANY,  17 ) \
FLAG(   7,  0, EBX,  3,  1, BMI1,                                YES,   9 ) \
FLAG(   7,  0, EBX,  4,  1, HLE,                                 YES,  11 ) \
FLAG(   7,  0, EBX,  5,  1, AVX2,                                YES,  11 ) \
FLAG(   7,  0, EBX,  6,  1, FDP_EXCPTN_ONLY,                     ANY,   4 ) \
FLAG(   7,  0, EBX,  7,  1, SMEP,                                YES,   9 ) \
FLAG(   7,  0, EBX,  8,  1, BMI2,                                YES,  11 ) \
FLAG(   7,  0, EBX,  9,  1, ENFSTRG,                             YES,   9 ) \
FLAG(   7,  0, EBX, 10,  1, INVPCID,                             YES,  11 ) \
FLAG(   7,  0, EBX, 11,  1, RTM,                                 YES,  11 ) \
FLAG(   7,  0, EBX, 12,  1, PQM,                                 NO,    0 ) \
FLAG(   7,  0, EBX, 13,  1, FP_SEGMENT_ZERO,                     ANY,  11 ) \
FLAG(   7,  0, EBX, 14,  1, MPX,                                 YES,  13 ) \
FLAG(   7,  0, EBX, 15,  1, PQE,                                 NO,    0 ) \
FLAG(   7,  0, EBX, 16,  1, AVX512F,                             YES,  13 ) \
FLAG(   7,  0, EBX, 17,  1, AVX512DQ,                            YES,  13 ) \
FLAG(   7,  0, EBX, 18,  1, RDSEED,                              YES,  11 ) \
FLAG(   7,  0, EBX, 19,  1, ADX,                                 YES,  11 ) \
FLAG(   7,  0, EBX, 20,  1, SMAP,                                YES,  11 ) \
FLAG(   7,  0, EBX, 21,  1, AVX512IFMA,                          YES,  17 ) \
FLAG(   7,  0, EBX, 23,  1, CLFLUSHOPT,                          YES,  13 ) \
FLAG(   7,  0, EBX, 24,  1, CLWB,                                YES,  13 ) \
FLAG(   7,  0, EBX, 25,  1, PT,                                  YES, FUT ) \
FLAG(   7,  0, EBX, 26,  1, AVX512PF,                            YES,  13 ) \
FLAG(   7,  0, EBX, 27,  1, AVX512ER,                            YES,  13 ) \
FLAG(   7,  0, EBX, 28,  1, AVX512CD,                            YES,  13 ) \
FLAG(   7,  0, EBX, 29,  1, SHA,                                 YES,  14 ) \
FLAG(   7,  0, EBX, 30,  1, AVX512BW,                            YES,  13 ) \
FLAG(   7,  0, EBX, 31,  1, AVX512VL,                            YES,  13 ) \
FLAG(   7,  0, ECX,  0,  1, PREFETCHWT1,                         YES,  13 ) \
FLAG(   7,  0, ECX,  1,  1, AVX512VBMI,                          YES,  17 ) \
FLAG(   7,  0, ECX,  2,  1, UMIP,                                YES,  17 ) \
FLAG(   7,  0, ECX,  3,  1, PKU,                                 YES,  13 ) \
FLAG(   7,  0, ECX,  4,  1, OSPKE,                               ANY,  13 ) \
FLAG(   7,  0, ECX,  5,  1, WAITPKG,                             NO,    0 ) \
FLAG(   7,  0, ECX,  6,  1, AVX512VBMI2,                         YES,  17 ) \
FLAG(   7,  0, ECX,  8,  1, GFNI,                                YES,  17 ) \
FLAG(   7,  0, ECX,  9,  1, VAES,                                YES,  17 ) \
FLAG(   7,  0, ECX, 10,  1, VPCLMULQDQ,                          YES,  17 ) \
FLAG(   7,  0, ECX, 11,  1, AVX512VNNI,                          YES,  17 ) \
FLAG(   7,  0, ECX, 12,  1, AVX512BITALG,                        YES,  17 ) \
FLAG(   7,  0, ECX, 14,  1, AVX512VPOPCNTDQ,                     YES,  16 ) \
FLAG(   7,  0, ECX, 16,  1, VA57,                                NO,    0 ) \
FIELD(  7,  0, ECX, 17,  5, MAWA,                                NO,    0 ) \
FLAG(   7,  0, ECX, 22,  1, RDPID,                               YES,  17 ) \
FLAG(   7,  0, ECX, 25,  1, CLDEMOTE,                            NO,    0 ) \
FLAG(   7,  0, ECX, 27,  1, MOVDIRI,                             NO,    0 ) \
FLAG(   7,  0, ECX, 28,  1, MOVDIR64B,                           NO,    0 ) \
FLAG(   7,  0, ECX, 30,  1, SGX_LC,                              ANY,  17 ) \
FLAG(   7,  0, EDX,  2,  1, AVX512QVNNIW,                        YES,  16 ) \
FLAG(   7,  0, EDX,  3,  1, AVX512QFMAPS,                        YES,  16 ) \
FLAG(   7,  0, EDX,  4,  1, FAST_SHORT_REPMOV,                   NO,    0 ) \
CPUID_7_EDX_10 \
FLAG(   7,  0, EDX, 13,  1, TSX_MICROCODE_UPDATE,                NO,    0 ) \
FLAG(   7,  0, EDX, 18,  1, PCONFIG,                             NO,    0 ) \
FLAG(   7,  0, EDX, 26,  1, IBRSIBPB,                            ANY,   9 ) \
FLAG(   7,  0, EDX, 27,  1, STIBP,                               YES,   9 ) \
FLAG(   7,  0, EDX, 28,  1, FCMD,                                YES,   9 ) \
FLAG(   7,  0, EDX, 29,  1, ARCH_CAPABILITIES,                   ANY,   9 ) \
FLAG(   7,  0, EDX, 31,  1, SSBD,                                YES,   9 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_A                                            \
FIELD(  A,  0, EAX,  0,  8, PMC_VERSION,                         NA,    0 ) \
FIELD(  A,  0, EAX,  8,  8, PMC_NUM_GEN,                         NA,    0 ) \
FIELD(  A,  0, EAX, 16,  8, PMC_WIDTH_GEN,                       NA,    0 ) \
FIELD(  A,  0, EAX, 24,  8, PMC_EBX_LENGTH,                      NA,    0 ) \
FLAG(   A,  0, EBX,  0,  1, PMC_CORE_CYCLES,                     NA,    0 ) \
FLAG(   A,  0, EBX,  1,  1, PMC_INSTR_RETIRED,                   NA,    0 ) \
FLAG(   A,  0, EBX,  2,  1, PMC_REF_CYCLES,                      NA,    0 ) \
FLAG(   A,  0, EBX,  3,  1, PMC_LAST_LVL_CREF,                   NA,    0 ) \
FLAG(   A,  0, EBX,  4,  1, PMC_LAST_LVL_CMISS,                  NA,    0 ) \
FLAG(   A,  0, EBX,  5,  1, PMC_BR_INST_RETIRED,                 NA,    0 ) \
FLAG(   A,  0, EBX,  6,  1, PMC_BR_MISS_RETIRED,                 NA,    0 ) \
FLAG(   A,  0, EBX,  7,  1, PMC_TOPDOWN_SLOTS,                   NA,    0 ) \
FLAG(   A,  0, ECX,  0,  1, PMC_FIXED0,                          NA,    0 ) \
FLAG(   A,  0, ECX,  1,  1, PMC_FIXED1,                          NA,    0 ) \
FLAG(   A,  0, ECX,  2,  1, PMC_FIXED2,                          NA,    0 ) \
FLAG(   A,  0, ECX,  3,  1, PMC_FIXED3,                          NA,    0 ) \
FIELD(  A,  0, EDX,  0,  5, PMC_NUM_FIXED,                       NA,    0 ) \
FIELD(  A,  0, EDX,  5,  8, PMC_WIDTH_FIXED,                     NA,    0 ) \
FLAG(   A,  0, EDX, 15,  1, PMC_ANYTHREAD_DEPRECATED,            NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_B                                            \
FIELD(  B,  0, EAX,  0,  5, TOPOLOGY_MASK_WIDTH,                 NA,    0 ) \
FIELD(  B,  0, EBX,  0, 16, TOPOLOGY_CPUS_SHARING_LEVEL,         NA,    0 ) \
FIELD(  B,  0, ECX,  0,  8, TOPOLOGY_LEVEL_NUMBER,               NA,    0 ) \
FIELD(  B,  0, ECX,  8,  8, TOPOLOGY_LEVEL_TYPE,                 NA,    0 ) \
FIELD(  B,  0, EDX,  0, 32, TOPOLOGY_X2APIC_ID,                  NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_D                                            \
FLAG(   D,  0, EAX,  0,  1, XCR0_MASTER_LEGACY_FP,               YES,   8 ) \
FLAG(   D,  0, EAX,  1,  1, XCR0_MASTER_SSE,                     YES,   8 ) \
FLAG(   D,  0, EAX,  2,  1, XCR0_MASTER_YMM_H,                   YES,   8 ) \
FLAG(   D,  0, EAX,  3,  1, XCR0_MASTER_BNDREGS,                 YES,  13 ) \
FLAG(   D,  0, EAX,  4,  1, XCR0_MASTER_BNDCSR,                  YES,  13 ) \
FLAG(   D,  0, EAX,  5,  1, XCR0_MASTER_OPMASK,                  YES,  13 ) \
FLAG(   D,  0, EAX,  6,  1, XCR0_MASTER_ZMM_H,                   YES,  13 ) \
FLAG(   D,  0, EAX,  7,  1, XCR0_MASTER_HI16_ZMM,                YES,  13 ) \
FLAG(   D,  0, EAX,  8,  1, XCR0_MASTER_XSS,                     NO,    0 ) \
FLAG(   D,  0, EAX,  9,  1, XCR0_MASTER_PKRU,                    YES,  13 ) \
FIELD(  D,  0, EAX,  10,22, XCR0_MASTER_LOWER,                   NO,    0 ) \
FIELD(  D,  0, EBX,  0, 32, XSAVE_ENABLED_SIZE,                  ANY,   8 ) \
FIELD(  D,  0, ECX,  0, 32, XSAVE_MAX_SIZE,                      YES,   8 ) \
FIELD(  D,  0, EDX,  0, 29, XCR0_MASTER_UPPER,                   NO,    0 ) \
FLAG(   D,  0, EDX, 30,  1, XCR0_MASTER_LWP,                     NO,    0 ) \
FLAG(   D,  0, EDX, 31,  1, XCR0_MASTER_EXTENDED_XSAVE,          NO,    0 ) \
FLAG(   D,  1, EAX,  0,  1, XSAVEOPT,                            YES,  11 ) \
FLAG(   D,  1, EAX,  1,  1, XSAVEC,                              YES,  13 ) \
FLAG(   D,  1, EAX,  2,  1, XGETBV_ECX1,                         YES,  17 ) \
FLAG(   D,  1, EAX,  3,  1, XSAVES,                              YES,  13 ) \
FIELD(  D,  1, EBX,  0, 32, XSAVES_ENABLED_SIZE,                 ANY,  13 ) \
FIELD(  D,  1, ECX,  0,  7, XSS_XCR0_USED0,                      NO,    0 ) \
FLAG(   D,  1, ECX,  8,  1, XSS_PT,                              NO,    0 ) \
FIELD(  D,  1, ECX,  9,  1, XSS_XCR0_USED1,                      NO,    0 ) \
FIELD(  D,  1, ECX,  10,22, XSS_RSVD0,                           NO,    0 ) \
FIELD(  D,  1, EDX,  0, 32, XSS_RSVD1,                           NO,    0 ) \
FIELD(  D,  2, EAX,  0, 32, XSAVE_YMM_SIZE,                      YES,   8 ) \
FIELD(  D,  2, EBX,  0, 32, XSAVE_YMM_OFFSET,                    YES,   8 ) \
FLAG(   D,  2, ECX,  0,  1, XSAVE_YMM_SUP_BY_XSS,                NO,    0 ) \
FLAG(   D,  2, ECX,  1,  1, XSAVE_YMM_ALIGN,                     YES,  13 ) \
FIELD(  D,  2, ECX,  2, 30, XSAVE_YMM_RSVD1,                     NO,    0 ) \
FIELD(  D,  2, EDX,  0, 32, XSAVE_YMM_RSVD2,                     NO,    0 ) \
FIELD(  D,  3, EAX,  0, 32, XSAVE_BNDREGS_SIZE,                  YES,  13 ) \
FIELD(  D,  3, EBX,  0, 32, XSAVE_BNDREGS_OFFSET,                YES,  13 ) \
FLAG(   D,  3, ECX,  0,  1, XSAVE_BNDREGS_SUP_BY_XSS,            NO,    0 ) \
FLAG(   D,  3, ECX,  1,  1, XSAVE_BNDREGS_ALIGN,                 YES,  13 ) \
FIELD(  D,  3, ECX,  2, 30, XSAVE_BNDREGS_RSVD1,                 NO,    0 ) \
FIELD(  D,  3, EDX,  0, 32, XSAVE_BNDREGS_RSVD2,                 NO,    0 ) \
FIELD(  D,  4, EAX,  0, 32, XSAVE_BNDCSR_SIZE,                   YES,  13 ) \
FIELD(  D,  4, EBX,  0, 32, XSAVE_BNDCSR_OFFSET,                 YES,  13 ) \
FLAG(   D,  4, ECX,  0,  1, XSAVE_BNDCSR_SUP_BY_XSS,             NO,    0 ) \
FLAG(   D,  4, ECX,  1,  1, XSAVE_BNDCSR_ALIGN,                  YES,  13 ) \
FIELD(  D,  4, ECX,  2, 30, XSAVE_BNDCSR_RSVD1,                  NO,    0 ) \
FIELD(  D,  4, EDX,  0, 32, XSAVE_BNDCSR_RSVD2,                  NO,    0 ) \
FIELD(  D,  5, EAX,  0, 32, XSAVE_OPMASK_SIZE,                   YES,  13 ) \
FIELD(  D,  5, EBX,  0, 32, XSAVE_OPMASK_OFFSET,                 YES,  13 ) \
FLAG(   D,  5, ECX,  0,  1, XSAVE_OPMASK_SUP_BY_XSS,             NO,    0 ) \
FLAG(   D,  5, ECX,  1,  1, XSAVE_OPMASK_ALIGN,                  YES,  13 ) \
FIELD(  D,  5, ECX,  2, 30, XSAVE_OPMASK_RSVD1,                  NO,    0 ) \
FIELD(  D,  5, EDX,  0, 32, XSAVE_OPMASK_RSVD2,                  NO,    0 ) \
FIELD(  D,  6, EAX,  0, 32, XSAVE_ZMM_H_SIZE,                    YES,  13 ) \
FIELD(  D,  6, EBX,  0, 32, XSAVE_ZMM_H_OFFSET,                  YES,  13 ) \
FLAG(   D,  6, ECX,  0,  1, XSAVE_ZMM_H_SUP_BY_XSS,              NO,    0 ) \
FLAG(   D,  6, ECX,  1,  1, XSAVE_ZMM_H_ALIGN,                   YES,  13 ) \
FIELD(  D,  6, ECX,  2, 30, XSAVE_ZMM_H_RSVD1,                   NO,    0 ) \
FIELD(  D,  6, EDX,  0, 32, XSAVE_ZMM_H_RSVD2,                   NO,    0 ) \
FIELD(  D,  7, EAX,  0, 32, XSAVE_HI16_ZMM_SIZE,                 YES,  13 ) \
FIELD(  D,  7, EBX,  0, 32, XSAVE_HI16_ZMM_OFFSET,               YES,  13 ) \
FLAG(   D,  7, ECX,  0,  1, XSAVE_HI16_ZMM_SUP_BY_XSS,           NO,    0 ) \
FLAG(   D,  7, ECX,  1,  1, XSAVE_HI16_ZMM_ALIGN,                YES,  13 ) \
FIELD(  D,  7, ECX,  2, 30, XSAVE_HI16_ZMM_RSVD1,                NO,    0 ) \
FIELD(  D,  7, EDX,  0, 32, XSAVE_HI16_ZMM_RSVD2,                NO,    0 ) \
FIELD(  D,  8, EAX,  0, 32, XSAVE_PT_STATE_SIZE,                 NO,    0 ) \
FIELD(  D,  8, EBX,  0, 32, XSAVE_PT_STATE_OFFSET,               NO,    0 ) \
FLAG(   D,  8, ECX,  0,  1, XSAVE_PT_STATE_SUP_BY_XSS,           NO,    0 ) \
FLAG(   D,  8, ECX,  1,  1, XSAVE_PT_STATE_ALIGN,                NO,    0 ) \
FIELD(  D,  8, ECX,  2, 30, XSAVE_PT_STATE_RSVD1,                NO,    0 ) \
FIELD(  D,  8, EDX,  0, 32, XSAVE_PT_STATE_RSVD2,                NO,    0 ) \
FIELD(  D,  9, EAX,  0, 32, XSAVE_PKRU_SIZE,                     YES,  13 ) \
FIELD(  D,  9, EBX,  0, 32, XSAVE_PKRU_OFFSET,                   YES,  13 ) \
FLAG(   D,  9, ECX,  0,  1, XSAVE_PKRU_SUP_BY_XSS,               NO,    0 ) \
FLAG(   D,  9, ECX,  1,  1, XSAVE_PKRU_ALIGN,                    YES,  13 ) \
FIELD(  D,  9, ECX,  2, 30, XSAVE_PKRU_RSVD1,                    NO,    0 ) \
FIELD(  D,  9, EDX,  0, 32, XSAVE_PKRU_RSVD2,                    NO,    0 ) \
FIELD(  D, 62, EAX,  0, 32, XSAVE_LWP_SIZE,                      NO,    0 ) \
FIELD(  D, 62, EBX,  0, 32, XSAVE_LWP_OFFSET,                    NO,    0 ) \
FIELD(  D, 62, ECX,  0, 32, XSAVE_LWP_RSVD1,                     NO,    0 ) \
FIELD(  D, 62, EDX,  0, 32, XSAVE_LWP_RSVD2,                     NO,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_F                                            \
FIELD(  F,  0, EBX,  0, 32, PQM_MAX_RMID,                        NO,    0 ) \
FLAG(   F,  0, EDX,  1,  1, PQM_CMT_SUPPORT,                     NO,    0 ) \
FIELD(  F,  1, EBX,  0, 32, PQM_CMT_CONV,                        NO,    0 ) \
FIELD(  F,  1, ECX,  0, 32, PQM_CMT_NUM_RMID,                    NO,    0 ) \
FLAG(   F,  1, EDX,  0,  1, PQM_CMT_OCCUPANCY,                   NO,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_10                                          \
FLAG(  10,  0, EBX,  1,  1, PQE_L3,                              NO,    0 ) \
FIELD( 10,  1, EAX,  0,  5, PQE_L3_MASK_LENGTH,                  NO,    0 ) \
FIELD( 10,  1, EBX,  0, 32, PQE_L3_ISOLATION_UNIT_MAP,           NO,    0 ) \
FLAG(  10,  1, ECX,  2,  1, PQE_L3_CDP,                          NO,    0 ) \
FIELD( 10,  1, EDX,  0, 16, PQE_L3_MAX_COS_NUMBER,               NO,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_12                                           \
FLAG(  12,  0, EAX,  0,  1, SGX1,                                ANY,  17 ) \
FLAG(  12,  0, EAX,  1,  1, SGX2,                                ANY, FUT ) \
FLAG(  12,  0, EAX,  5,  1, SGX_OVERSUB_ENCLV,                   ANY, FUT ) \
FLAG(  12,  0, EAX,  6,  1, SGX_OVERSUB_ENCLS,                   ANY, FUT ) \
FLAG(  12,  0, EBX,  0,  1, SGX_MISCSELECT_EXINFO,               ANY, FUT ) \
FIELD( 12,  0, EBX,  1, 31, SGX_MISCSELECT_RSVD,                 NO,    0 ) \
FIELD( 12,  0, EDX,  0,  8, MAX_ENCLAVE_SIZE_NOT64,              ANY,  17 ) \
FIELD( 12,  0, EDX,  8,  8, MAX_ENCLAVE_SIZE_64,                 ANY,  17 ) \
FIELD( 12,  1, EAX,  0, 32, SECS_ATTRIBUTES0,                    ANY,  17 ) \
FIELD( 12,  1, EBX,  0, 32, SECS_ATTRIBUTES1,                    ANY,  17 ) \
FIELD( 12,  1, ECX,  0, 32, SECS_ATTRIBUTES2,                    ANY,  17 ) \
FIELD( 12,  1, EDX,  0, 32, SECS_ATTRIBUTES3,                    ANY,  17 ) \
FIELD( 12,  2, EAX,  0, 15, EPC00_VALID,                         ANY,  17 ) \
FIELD( 12,  2, EAX, 12, 20, EPC00_BASE_LOW,                      ANY,  17 ) \
FIELD( 12,  2, EBX,  0, 20, EPC00_BASE_HIGH,                     ANY,  17 ) \
FIELD( 12,  2, ECX,  0, 15, EPC00_PROTECTED,                     ANY,  17 ) \
FIELD( 12,  2, ECX, 12, 20, EPC00_SIZE_LOW,                      ANY,  17 ) \
FIELD( 12,  2, EDX,  0, 20, EPC00_SIZE_HIGH,                     ANY,  17 ) \
FIELD( 12,  3, EAX,  0,  4, EPC01_VALID,                         NO,    0 ) \
FIELD( 12,  3, EAX, 12, 20, EPC01_BASE_LOW,                      NO,    0 ) \
FIELD( 12,  3, EBX,  0, 20, EPC01_BASE_HIGH,                     NO,    0 ) \
FIELD( 12,  3, ECX,  0,  4, EPC01_PROTECTED,                     NO,    0 ) \
FIELD( 12,  3, ECX, 12, 20, EPC01_SIZE_LOW,                      NO,    0 ) \
FIELD( 12,  3, EDX,  0, 20, EPC01_SIZE_HIGH,                     NO,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_14                                           \
FIELD( 14,  0, EAX,  0, 32, PT_MAX_SUBLEAF,                     YES, FUT ) \
FLAG(  14,  0, EBX,  0,  1, PT_CR3_FILTER,                      YES, FUT ) \
FLAG(  14,  0, EBX,  1,  1, PT_CFG_PSB_CYC,                     YES, FUT ) \
FLAG(  14,  0, EBX,  2,  1, PT_IP_FILTER_PERSIST_MSR,           YES, FUT ) \
FLAG(  14,  0, EBX,  3,  1, PT_MTC,                             YES, FUT ) \
FLAG(  14,  0, EBX,  4,  1, PT_PTWRITE,                         NO,    0 ) \
FLAG(  14,  0, EBX,  5,  1, PT_POWER_EVENT,                     NO,    0 ) \
FLAG(  14,  0, ECX,  0,  1, PT_TOPA,                            YES, FUT ) \
FLAG(  14,  0, ECX,  1,  1, PT_TOPA_MULTI,                      YES, FUT ) \
FLAG(  14,  0, ECX,  2,  1, PT_SRO,                             YES, FUT ) \
FLAG(  14,  0, ECX,  3,  1, PT_TRACE_TRANS,                     NO,    0 ) \
FLAG(  14,  0, ECX, 31,  1, PT_LIP,                             YES, FUT ) \
FIELD( 14,  1, EAX,  0,  3, PT_NUM_ADDR_RANGES,                 YES, FUT ) \
FIELD( 14,  1, EAX, 16, 16, PT_AVAIL_MTC_ENCS,                  YES, FUT ) \
FIELD( 14,  1, EBX,  0, 16, PT_AVAIL_CYC_THRESH_ENCS,           YES, FUT ) \
FIELD( 14,  1, EBX, 16, 16, PT_AVAIL_PSB_FREQ_ENCS,             YES, FUT ) \

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_15                                           \
FIELD( 15,  0, EAX,  0, 32, DENOM_TSC_TO_CORE_CRYSTAL_CLK,       NO,    0 ) \
FIELD( 15,  0, EBX,  0, 32, NUMER_TSC_TO_CORE_CRYSTAL_CLK,       NO,    0 ) \

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_16                                           \
FIELD( 16,  0, EAX,  0, 16, PROC_BASE_FREQ,                      NO,    0 ) \
FIELD( 16,  0, EBX,  0, 16, PROC_MIN_FREQ,                       NO,    0 ) \
FIELD( 16,  0, ECX,  0, 16, BUS_FREQ,                            NO,    0 ) \

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_17                                           \
FIELD( 17,  0, EAX,  0, 31, MAX_SOCID_INDEX,                     NO,    0 ) \
FIELD( 17,  0, EBX,  0, 16, SOC_VENDOR_ID,                       NO,    0 ) \
FIELD( 17,  0, EBX, 16,  1, SOC_INDUSTRY_STD,                    NO,    0 ) \
FIELD( 17,  0, ECX,  0, 31, SOC_PROJECT_ID,                      NO,    0 ) \
FIELD( 17,  0, EDX,  0, 31, SOC_STEPPING_ID,                     NO,    0 ) \
FIELD( 17,  1, EAX,  0, 32, SOC_VENDOR_BRAND_STRING_1_0,         NO,    0 ) \
FIELD( 17,  1, EBX,  0, 32, SOC_VENDOR_BRAND_STRING_1_1,         NO,    0 ) \
FIELD( 17,  1, ECX,  0, 32, SOC_VENDOR_BRAND_STRING_1_2,         NO,    0 ) \
FIELD( 17,  1, EDX,  0, 32, SOC_VENDOR_BRAND_STRING_1_3,         NO,    0 ) \
FIELD( 17,  2, EAX,  0, 32, SOC_VENDOR_BRAND_STRING_2_0,         NO,    0 ) \
FIELD( 17,  2, EBX,  0, 32, SOC_VENDOR_BRAND_STRING_2_1,         NO,    0 ) \
FIELD( 17,  2, ECX,  0, 32, SOC_VENDOR_BRAND_STRING_2_2,         NO,    0 ) \
FIELD( 17,  2, EDX,  0, 32, SOC_VENDOR_BRAND_STRING_2_3,         NO,    0 ) \
FIELD( 17,  3, EAX,  0, 32, SOC_VENDOR_BRAND_STRING_3_0,         NO,    0 ) \
FIELD( 17,  3, EBX,  0, 32, SOC_VENDOR_BRAND_STRING_3_1,         NO,    0 ) \
FIELD( 17,  3, ECX,  0, 32, SOC_VENDOR_BRAND_STRING_3_2,         NO,    0 ) \
FIELD( 17,  3, EDX,  0, 32, SOC_VENDOR_BRAND_STRING_3_3,         NO,    0 ) \

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_400                                         \
FIELD(400,  0, EAX,  0, 32, MAX_HYP_LEVEL,                       NA,    0 ) \
FIELD(400,  0, EBX,  0, 32, HYPERVISOR_VENDOR0,                  NA,    0 ) \
FIELD(400,  0, ECX,  0, 32, HYPERVISOR_VENDOR1,                  NA,    0 ) \
FIELD(400,  0, EDX,  0, 32, HYPERVISOR_VENDOR2,                  NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_401                                          \
FIELD(401,  0, EAX,  0, 32, HV_INTERFACE_SIGNATURE,              NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_402                                          \
FIELD(402,  0, EAX,  0, 32, BUILD_NUMBER,                        NA,    0 ) \
FIELD(402,  0, EBX,  0, 16, MINOR_VERSION,                       NA,    0 ) \
FIELD(402,  0, EBX, 16, 16, MAJOR_VERSION,                       NA,    0 ) \
FIELD(402,  0, ECX,  0, 32, SERVICE_PACK,                        NA,    0 ) \
FIELD(402,  0, EDX,  0, 24, SERVICE_NUMBER,                      NA,    0 ) \
FIELD(402,  0, EDX, 24,  8, SERVICE_BRANCH,                      NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_403                                          \
FLAG( 403,  0, EAX,  0,  1, VP_RUNTIME_AVAIL,                    NA,    0 ) \
FLAG( 403,  0, EAX,  1,  1, REF_COUNTER_AVAIL,                   NA,    0 ) \
FLAG( 403,  0, EAX,  2,  1, BASIC_SYNIC_MSRS_AVAIL,              NA,    0 ) \
FLAG( 403,  0, EAX,  3,  1, SYNTH_TIMER_MSRS_AVAIL,              NA,    0 ) \
FLAG( 403,  0, EAX,  4,  1, APIC_ACCESS_MSRS_AVAIL,              NA,    0 ) \
FLAG( 403,  0, EAX,  5,  1, HYPERCALL_MSRS_AVAIL,                NA,    0 ) \
FLAG( 403,  0, EAX,  6,  1, VP_INDEX_MSR_AVAIL,                  NA,    0 ) \
FLAG( 403,  0, EAX,  7,  1, VIRT_RESET_MSR_AVAIL,                NA,    0 ) \
FLAG( 403,  0, EAX,  8,  1, STATS_PAGES_MSRS_AVAIL,              NA,    0 ) \
FLAG( 403,  0, EAX,  9,  1, REF_TSC_AVAIL,                       NA,    0 ) \
FLAG( 403,  0, EAX, 10,  1, GUEST_IDLE_MSR_AVAIL,                NA,    0 ) \
FLAG( 403,  0, EAX, 11,  1, FREQUENCY_MSRS_AVAIL,                NA,    0 ) \
FLAG( 403,  0, EAX, 12,  1, SYNTH_DEBUG_MSRS_AVAIL,              NA,    0 ) \
FLAG( 403,  0, EBX,  0,  1, CREATE_PARTITIONS_FLAG,              NA,    0 ) \
FLAG( 403,  0, EBX,  1,  1, ACCESS_PARTITION_ID_FLAG,            NA,    0 ) \
FLAG( 403,  0, EBX,  2,  1, ACCESS_MEMORY_POOL_FLAG,             NA,    0 ) \
FLAG( 403,  0, EBX,  3,  1, ADJUST_MESSAGE_BUFFERS_FLAG,         NA,    0 ) \
FLAG( 403,  0, EBX,  4,  1, POST_MESSAGES_FLAG,                  NA,    0 ) \
FLAG( 403,  0, EBX,  5,  1, SIGNAL_EVENTS_FLAG,                  NA,    0 ) \
FLAG( 403,  0, EBX,  6,  1, CREATE_PORT_FLAG,                    NA,    0 ) \
FLAG( 403,  0, EBX,  7,  1, CONNECT_PORT_FLAG,                   NA,    0 ) \
FLAG( 403,  0, EBX,  8,  1, ACCESS_STATS_FLAG,                   NA,    0 ) \
FLAG( 403,  0, EBX, 11,  1, DEBUGGING_FLAG,                      NA,    0 ) \
FLAG( 403,  0, EBX, 12,  1, CPU_MANAGEMENT_FLAG,                 NA,    0 ) \
FLAG( 403,  0, EBX, 13,  1, CONFIGURE_PROFILER_FLAG,             NA,    0 ) \
FLAG( 403,  0, EBX, 14,  1, ENABLE_EXPANDED_STACKWALKING_FLAG,   NA,    0 ) \
FLAG( 403,  0, EBX, 16,  1, ACCESS_VSM,                          NA,    0 ) \
FLAG( 403,  0, EBX, 17,  1, ACCESS_VP_REGISTERS,                 NA,    0 ) \
FIELD(403,  0, ECX,  0,  4, MAX_POWER_STATE,                     NA,    0 ) \
FLAG( 403,  0, ECX,  4,  1, HPET_NEEDED_FOR_C3,                  NA,    0 ) \
FLAG( 403,  0, EDX,  0,  1, MWAIT_AVAIL,                         NA,    0 ) \
FLAG( 403,  0, EDX,  1,  1, GUEST_DEBUGGING_AVAIL,               NA,    0 ) \
FLAG( 403,  0, EDX,  2,  1, PERFORMANCE_MONITOR_AVAIL,           NA,    0 ) \
FLAG( 403,  0, EDX,  3,  1, CPU_DYN_PARTITIONING_AVAIL,          NA,    0 ) \
FLAG( 403,  0, EDX,  4,  1, XMM_REGS_FOR_HYPERCALL_INPUT,        NA,    0 ) \
FLAG( 403,  0, EDX,  5,  1, GUEST_IDLE_AVAIL,                    NA,    0 ) \
FLAG( 403,  0, EDX,  6,  1, HYPERVISOR_SLEEP_STATE_AVAIL,        NA,    0 ) \
FLAG( 403,  0, EDX,  7,  1, NUMA_DISTANCE_QUERY_AVAIL,           NA,    0 ) \
FLAG( 403,  0, EDX,  8,  1, TIMER_FREQUENCY_AVAIL,               NA,    0 ) \
FLAG( 403,  0, EDX,  9,  1, SYNTH_MACHINE_CHECK_AVAIL,           NA,    0 ) \
FLAG( 403,  0, EDX, 10,  1, GUEST_CRASH_MSRS_AVAIL,              NA,    0 ) \
FLAG( 403,  0, EDX, 11,  1, DEBUG_MSRS_AVAIL,                    NA,    0 ) \
FLAG( 403,  0, EDX, 12,  1, NPIEP1_AVAIL,                        NA,    0 ) \
FLAG( 403,  0, EDX, 13,  1, DISABLE_HYPERVISOR_AVAIL,            NA,    0 ) \
FLAG( 403,  0, EDX, 15,  1, XMM_REGS_FOR_HYPERCALL_OUTPUT,       NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_404                                         \
FLAG( 404,  0, EAX,  0,  1, USE_HYPERCALL_TO_SWITCH_ADDR_SPACE,  NA,    0 ) \
FLAG( 404,  0, EAX,  1,  1, USE_HYPERCALL_TO_FLUSH_TLB,          NA,    0 ) \
FLAG( 404,  0, EAX,  2,  1, USE_HYPERCALL_FOR_TLB_SHOOTDOWN,     NA,    0 ) \
FLAG( 404,  0, EAX,  3,  1, USE_MSRS_FOR_EOI_ICR_TPR,            NA,    0 ) \
FLAG( 404,  0, EAX,  4,  1, USE_MSR_FOR_RESET,                   NA,    0 ) \
FLAG( 404,  0, EAX,  5,  1, USE_RELAXED_TIMING,                  NA,    0 ) \
FLAG( 404,  0, EAX,  6,  1, USE_DMA_REMAPPING,                   NA,    0 ) \
FLAG( 404,  0, EAX,  7,  1, USE_INTERRUPT_REMAPPING,             NA,    0 ) \
FLAG( 404,  0, EAX,  8,  1, USE_X2APIC,                          NA,    0 ) \
FLAG( 404,  0, EAX,  9,  1, DEPRECATE_AUTOEOI,                   NA,    0 ) \
FIELD(404,  0, EBX,  0, 32, SPINLOCK_RETRIES,                    NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_405                                          \
FIELD(405,  0, EAX,  0, 32, MAX_VCPU,                            NA,    0 ) \
FIELD(405,  0, EBX,  0, 32, MAX_LCPU,                            NA,    0 ) \
FIELD(405,  0, ECX,  0, 32, MAX_REMAPPABLE_VECTORS,              NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_406                                          \
FLAG( 406,  0, EAX,  0,  1, APIC_OVERLAY_ASSIST,                 NA,    0 ) \
FLAG( 406,  0, EAX,  1,  1, MSR_BITMAPS,                         NA,    0 ) \
FLAG( 406,  0, EAX,  2,  1, ARCH_PMCS,                           NA,    0 ) \
FLAG( 406,  0, EAX,  3,  1, SLAT,                                NA,    0 ) \
FLAG( 406,  0, EAX,  4,  1, DMA_REMAPPING,                       NA,    0 ) \
FLAG( 406,  0, EAX,  5,  1, INTERRUPT_REMAPPING,                 NA,    0 ) \
FLAG( 406,  0, EAX,  6,  1, MEMORY_PATROL_SCRUBBER,              NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_410                                          \
FIELD(410,  0, EAX,  0, 32, TSC_HZ,                              NA,    0 ) \
FIELD(410,  0, EBX,  0, 32, APICBUS_HZ,                          NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_80                                           \
FIELD( 80,  0, EAX,  0, 32, NUM_EXT_LEVELS,                      NA,    0 ) \
FIELD( 80,  0, EBX,  0, 32, LEAF80_VENDOR1,                      NA,    0 ) \
FIELD( 80,  0, ECX,  0, 32, LEAF80_VENDOR3,                      NA,    0 ) \
FIELD( 80,  0, EDX,  0, 32, LEAF80_VENDOR2,                      NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_81                                           \
FIELD( 81,  0, EAX,  0, 32, UNKNOWN81EAX,                        ANY,   4 ) \
FIELD( 81,  0, EAX,  0,  4, LEAF81_STEPPING,                     ANY,   4 ) \
FIELD( 81,  0, EAX,  4,  4, LEAF81_MODEL,                        ANY,   4 ) \
FIELD( 81,  0, EAX,  8,  4, LEAF81_FAMILY,                       ANY,   4 ) \
FIELD( 81,  0, EAX, 12,  2, LEAF81_TYPE,                         ANY,   4 ) \
FIELD( 81,  0, EAX, 16,  4, LEAF81_EXTENDED_MODEL,               ANY,   4 ) \
FIELD( 81,  0, EAX, 20,  8, LEAF81_EXTENDED_FAMILY,              ANY,   4 ) \
FIELD( 81,  0, EBX,  0, 32, UNKNOWN81EBX,                        ANY,   4 ) \
FIELD( 81,  0, EBX,  0, 16, LEAF81_BRAND_ID,                     ANY,   4 ) \
FIELD( 81,  0, EBX, 16, 16, UNDEF,                               ANY,   4 ) \
FLAG(  81,  0, ECX,  0,  1, LAHF64,                              YES,   4 ) \
FLAG(  81,  0, ECX,  1,  1, CMPLEGACY,                           ANY,   9 ) \
FLAG(  81,  0, ECX,  2,  1, SVM,                                 YES,   8 ) \
FLAG(  81,  0, ECX,  3,  1, EXTAPICSPC,                          YES,   4 ) \
FLAG(  81,  0, ECX,  4,  1, CR8AVAIL,                            YES,   9 ) \
FLAG(  81,  0, ECX,  5,  1, ABM,                                 YES,   4 ) \
FLAG(  81,  0, ECX,  6,  1, SSE4A,                               YES,   4 ) \
FLAG(  81,  0, ECX,  7,  1, MISALIGNED_SSE,                      YES,   4 ) \
FLAG(  81,  0, ECX,  8,  1, 3DNPREFETCH,                         YES,   4 ) \
FLAG(  81,  0, ECX,  9,  1, OSVW,                                ANY,   8 ) \
FLAG(  81,  0, ECX, 10,  1, IBS,                                 NO,    0 ) \
FLAG(  81,  0, ECX, 11,  1, XOP,                                 YES,   8 ) \
FLAG(  81,  0, ECX, 12,  1, SKINIT,                              NO,    0 ) \
FLAG(  81,  0, ECX, 13,  1, WATCHDOG,                            NO,    0 ) \
FLAG(  81,  0, ECX, 15,  1, LWP,                                 NO,    0 ) \
FLAG(  81,  0, ECX, 16,  1, FMA4,                                YES,   8 ) \
FLAG(  81,  0, ECX, 17,  1, TCE,                                 NO,    0 ) \
FLAG(  81,  0, ECX, 19,  1, NODEID_MSR,                          NO,    0 ) \
FLAG(  81,  0, ECX, 21,  1, TBM,                                 YES,   9 ) \
FLAG(  81,  0, ECX, 22,  1, TOPOLOGY,                            NO,    0 ) \
FLAG(  81,  0, ECX, 23,  1, PERFCORE,                            ANY,   4 ) \
FLAG(  81,  0, ECX, 24,  1, PERFNB,                              NO,    0 ) \
FLAG(  81,  0, ECX, 26,  1, DATABK,                              NO,    0 ) \
FLAG(  81,  0, ECX, 27,  1, PERFTSC,                             NO,    0 ) \
FLAG(  81,  0, ECX, 28,  1, PERFL3,                              NO,    0 ) \
FLAG(  81,  0, ECX, 29,  1, MWAITX,                              NO,    0 ) \
FLAG(  81,  0, EDX,  0,  1, LEAF81_FPU,                          YES,   4 ) \
FLAG(  81,  0, EDX,  1,  1, LEAF81_VME,                          YES,   4 ) \
FLAG(  81,  0, EDX,  2,  1, LEAF81_DE,                           YES,   4 ) \
FLAG(  81,  0, EDX,  3,  1, LEAF81_PSE,                          YES,   4 ) \
FLAG(  81,  0, EDX,  4,  1, LEAF81_TSC,                          YES,   4 ) \
FLAG(  81,  0, EDX,  5,  1, LEAF81_MSR,                          YES,   4 ) \
FLAG(  81,  0, EDX,  6,  1, LEAF81_PAE,                          YES,   4 ) \
FLAG(  81,  0, EDX,  7,  1, LEAF81_MCE,                          YES,   4 ) \
FLAG(  81,  0, EDX,  8,  1, LEAF81_CX8,                          YES,   4 ) \
FLAG(  81,  0, EDX,  9,  1, LEAF81_APIC,                         ANY,   4 ) \
FLAG(  81,  0, EDX, 11,  1, SYSC,                                ANY,   4 ) \
FLAG(  81,  0, EDX, 12,  1, LEAF81_MTRR,                         YES,   4 ) \
FLAG(  81,  0, EDX, 13,  1, LEAF81_PGE,                          YES,   4 ) \
FLAG(  81,  0, EDX, 14,  1, LEAF81_MCA,                          YES,   4 ) \
FLAG(  81,  0, EDX, 15,  1, LEAF81_CMOV,                         YES,   4 ) \
FLAG(  81,  0, EDX, 16,  1, LEAF81_PAT,                          YES,   4 ) \
FLAG(  81,  0, EDX, 17,  1, LEAF81_PSE36,                        YES,   4 ) \
FLAG(  81,  0, EDX, 20,  1, NX,                                  YES,   4 ) \
FLAG(  81,  0, EDX, 22,  1, MMXEXT,                              YES,   4 ) \
FLAG(  81,  0, EDX, 23,  1, LEAF81_MMX,                          YES,   4 ) \
FLAG(  81,  0, EDX, 24,  1, LEAF81_FXSR,                         YES,   4 ) \
FLAG(  81,  0, EDX, 25,  1, FFXSR,                               YES,   4 ) \
FLAG(  81,  0, EDX, 26,  1, PDPE1GB,                             YES,   9 ) \
FLAG(  81,  0, EDX, 27,  1, RDTSCP,                              YES,   4 ) \
FLAG(  81,  0, EDX, 29,  1, LM,                                  YES,   4 ) \
FLAG(  81,  0, EDX, 30,  1, 3DNOWPLUS,                           YES,   4 ) \
FLAG(  81,  0, EDX, 31,  1, 3DNOW,                               YES,   4 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_82                                           \
FIELD( 82,  0, EAX,  0, 32, LEAF82_BRAND_STRING_EAX,             NA,    0 ) \
FIELD( 82,  0, EBX,  0, 32, LEAF82_BRAND_STRING_EBX,             NA,    0 ) \
FIELD( 82,  0, ECX,  0, 32, LEAF82_BRAND_STRING_ECX,             NA,    0 ) \
FIELD( 82,  0, EDX,  0, 32, LEAF82_BRAND_STRING_EDX,             NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_83                                           \
FIELD( 83,  0, EAX,  0, 32, LEAF83_BRAND_STRING_EAX,             NA,    0 ) \
FIELD( 83,  0, EBX,  0, 32, LEAF83_BRAND_STRING_EBX,             NA,    0 ) \
FIELD( 83,  0, ECX,  0, 32, LEAF83_BRAND_STRING_ECX,             NA,    0 ) \
FIELD( 83,  0, EDX,  0, 32, LEAF83_BRAND_STRING_EDX,             NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_84                                           \
FIELD( 84,  0, EAX,  0, 32, LEAF84_BRAND_STRING_EAX,             NA,    0 ) \
FIELD( 84,  0, EBX,  0, 32, LEAF84_BRAND_STRING_EBX,             NA,    0 ) \
FIELD( 84,  0, ECX,  0, 32, LEAF84_BRAND_STRING_ECX,             NA,    0 ) \
FIELD( 84,  0, EDX,  0, 32, LEAF84_BRAND_STRING_EDX,             NA,    0 )

/*    LEVEL, REG, POS, SIZE, NAME,                          MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_85                                           \
FIELD( 85,  0, EAX,  0,  8, ITLB_ENTRIES_2M4M_PGS,               NA,    0 ) \
FIELD( 85,  0, EAX,  8,  8, ITLB_ASSOC_2M4M_PGS,                 NA,    0 ) \
FIELD( 85,  0, EAX, 16,  8, DTLB_ENTRIES_2M4M_PGS,               NA,    0 ) \
FIELD( 85,  0, EAX, 24,  8, DTLB_ASSOC_2M4M_PGS,                 NA,    0 ) \
FIELD( 85,  0, EBX,  0,  8, ITLB_ENTRIES_4K_PGS,                 NA,    0 ) \
FIELD( 85,  0, EBX,  8,  8, ITLB_ASSOC_4K_PGS,                   NA,    0 ) \
FIELD( 85,  0, EBX, 16,  8, DTLB_ENTRIES_4K_PGS,                 NA,    0 ) \
FIELD( 85,  0, EBX, 24,  8, DTLB_ASSOC_4K_PGS,                   NA,    0 ) \
FIELD( 85,  0, ECX,  0,  8, L1_DCACHE_LINE_SIZE,                 NA,    0 ) \
FIELD( 85,  0, ECX,  8,  8, L1_DCACHE_LINES_PER_TAG,             NA,    0 ) \
FIELD( 85,  0, ECX, 16,  8, L1_DCACHE_ASSOC,                     NA,    0 ) \
FIELD( 85,  0, ECX, 24,  8, L1_DCACHE_SIZE,                      NA,    0 ) \
FIELD( 85,  0, EDX,  0,  8, L1_ICACHE_LINE_SIZE,                 NA,    0 ) \
FIELD( 85,  0, EDX,  8,  8, L1_ICACHE_LINES_PER_TAG,             NA,    0 ) \
FIELD( 85,  0, EDX, 16,  8, L1_ICACHE_ASSOC,                     NA,    0 ) \
FIELD( 85,  0, EDX, 24,  8, L1_ICACHE_SIZE,                      NA,    0 )

/*    LEVEL, REG, POS, SIZE, NAME,                          MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_86                                           \
FIELD( 86,  0, EAX,  0, 12, L2_ITLB_ENTRIES_2M4M_PGS,            NA,    0 ) \
FIELD( 86,  0, EAX, 12,  4, L2_ITLB_ASSOC_2M4M_PGS,              NA,    0 ) \
FIELD( 86,  0, EAX, 16, 12, L2_DTLB_ENTRIES_2M4M_PGS,            NA,    0 ) \
FIELD( 86,  0, EAX, 28,  4, L2_DTLB_ASSOC_2M4M_PGS,              NA,    0 ) \
FIELD( 86,  0, EBX,  0, 12, L2_ITLB_ENTRIES_4K_PGS,              NA,    0 ) \
FIELD( 86,  0, EBX, 12,  4, L2_ITLB_ASSOC_4K_PGS,                NA,    0 ) \
FIELD( 86,  0, EBX, 16, 12, L2_DTLB_ENTRIES_4K_PGS,              NA,    0 ) \
FIELD( 86,  0, EBX, 28,  4, L2_DTLB_ASSOC_4K_PGS,                NA,    0 ) \
FIELD( 86,  0, ECX,  0,  8, L2CACHE_LINE,                        NA,    0 ) \
FIELD( 86,  0, ECX,  8,  4, L2CACHE_LINE_PER_TAG,                NA,    0 ) \
FIELD( 86,  0, ECX, 12,  4, L2CACHE_WAYS,                        NA,    0 ) \
FIELD( 86,  0, ECX, 16, 16, L2CACHE_SIZE,                        NA,    0 ) \
FIELD( 86,  0, EDX,  0,  8, L3CACHE_LINE,                        NA,    0 ) \
FIELD( 86,  0, EDX,  8,  4, L3CACHE_LINE_PER_TAG,                NA,    0 ) \
FIELD( 86,  0, EDX, 12,  4, L3CACHE_WAYS,                        NA,    0 ) \
FIELD( 86,  0, EDX, 18, 14, L3CACHE_SIZE,                        NA,    0 )

/*    LEVEL, REG, POS, SIZE, NAME,                          MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_87                                           \
FLAG(  87,  0, EBX,  0,  1, MCA_OVERFLOW_RECOV,                  NA,    0 ) \
FLAG(  87,  0, EBX,  1,  1, SUCCOR,                              NA,    0 ) \
FLAG(  87,  0, EBX,  2,  1, HWA,                                 NA,    0 ) \
FLAG(  87,  0, EBX,  3,  1, SCALABLE_MCA,                        NA,    0 ) \
FLAG(  87,  0, EBX,  4,  1, PFEH_SUPPORT_PRESENT,                NA,    0 ) \
FLAG(  87,  0, EDX,  0,  1, TS,                                  NA,    0 ) \
FLAG(  87,  0, EDX,  1,  1, FID,                                 NA,    0 ) \
FLAG(  87,  0, EDX,  2,  1, VID,                                 NA,    0 ) \
FLAG(  87,  0, EDX,  3,  1, TTP,                                 NA,    0 ) \
FLAG(  87,  0, EDX,  4,  1, LEAF87_TM,                           NA,    0 ) \
FLAG(  87,  0, EDX,  5,  1, STC,                                 NA,    0 ) \
FLAG(  87,  0, EDX,  6,  1, 100MHZSTEPS,                         NA,    0 ) \
FLAG(  87,  0, EDX,  7,  1, HWPSTATE,                            NA,    0 ) \
FLAG(  87,  0, EDX,  8,  1, TSC_INVARIANT,                       NA,    0 ) \
FLAG(  87,  0, EDX,  9,  1, CORE_PERF_BOOST,                     NA,    0 )

/*    LEVEL, REG, POS, SIZE, NAME,                          MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_88                                           \
FIELD( 88,  0, EAX,  0,  8, PHYS_BITS,                           YES,   4 ) \
FIELD( 88,  0, EAX,  8,  8, VIRT_BITS,                           YES,   4 ) \
FIELD( 88,  0, EAX, 16,  8, GUEST_PHYS_ADDR_SZ,                  YES,   8 ) \
FLAG(  88,  0, EBX,  0,  1, CLZERO,                              YES,  14 ) \
FLAG(  88,  0, EBX,  1,  1, IRPERF,                              NO,    0 ) \
FLAG(  88,  0, EBX,  2,  1, XSAVE_ERR_PTR,                       NO,    0 ) \
FLAG(  88,  0, EBX,  9,  1, WBNOINVD,                            YES, FUT ) \
FLAG(  88,  0, EBX, 12,  1, LEAF88_IBPB,                         ANY,   9 ) \
FLAG(  88,  0, EBX, 14,  1, LEAF88_IBRS,                         NO,    0 ) \
FLAG(  88,  0, EBX, 15,  1, LEAF88_STIBP,                        NO,    0 ) \
FLAG(  88,  0, EBX, 16,  1, LEAF88_IBRS_ALWAYS,                  NO,    0 ) \
FLAG(  88,  0, EBX, 17,  1, LEAF88_STIBP_ALWAYS,                 NO,    0 ) \
FLAG(  88,  0, EBX, 18,  1, LEAF88_PREFER_IBRS,                  NO,    0 ) \
FLAG(  88,  0, EBX, 24,  1, LEAF88_SSBD_SPEC_CTRL,               ANY,   9 ) \
FLAG(  88,  0, EBX, 25,  1, LEAF88_SSBD_VIRT_SPEC_CTRL,          ANY,   9 ) \
FLAG(  88,  0, EBX, 26,  1, LEAF88_SSBD_NOT_NEEDED,              NO,    0 ) \
FIELD( 88,  0, ECX,  0,  8, LEAF88_CORE_COUNT,                   YES,   4 ) \
FIELD( 88,  0, ECX, 12,  4, APICID_COREID_SIZE,                  YES,   7 ) \
FIELD( 88,  0, ECX, 16,  2, PERFTSC_SIZE,                        NO,    0 )

#define CPUID_8A_EDX_11 \
FLAG(  8A,  0, EDX, 11,  1, SVMEDX_RSVD1,                        NO,    0 )
#define CPUID_8A_EDX_14 \
FLAG(  8A,  0, EDX, 14,  1, SVMEDX_RSVD2,                        NO,    0 )
#define CPUID_8A_EDX_17 \
FLAG(  8A,  0, EDX, 17,  1, SVMEDX_RSVD3,                        NO,    0 )

/*    LEVEL, REG, POS, SIZE, NAME,                          MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_8A                                           \
FIELD( 8A,  0, EAX,  0,  8, SVM_REVISION,                        YES,   4 ) \
FLAG(  8A,  0, EAX,  8,  1, SVM_HYPERVISOR,                      NO,    0 ) \
FIELD( 8A,  0, EAX,  9, 23, SVMEAX_RSVD,                         NO,    0 ) \
FIELD( 8A,  0, EBX,  0, 32, SVM_NUM_ASIDS,                       YES,   7 ) \
FIELD( 8A,  0, ECX,  0, 32, SVMECX_RSVD,                         NO,    0 ) \
FLAG(  8A,  0, EDX,  0,  1, SVM_NPT,                             YES,   7 ) \
FLAG(  8A,  0, EDX,  1,  1, SVM_LBR,                             NO,    0 ) \
FLAG(  8A,  0, EDX,  2,  1, SVM_LOCK,                            ANY,   7 ) \
FLAG(  8A,  0, EDX,  3,  1, SVM_NRIP,                            YES,   7 ) \
FLAG(  8A,  0, EDX,  4,  1, SVM_TSC_RATE_MSR,                    NO,    0 ) \
FLAG(  8A,  0, EDX,  5,  1, SVM_VMCB_CLEAN,                      YES,   7 ) \
FLAG(  8A,  0, EDX,  6,  1, SVM_FLUSH_BY_ASID,                   YES,   7 ) \
FLAG(  8A,  0, EDX,  7,  1, SVM_DECODE_ASSISTS,                  YES,   7 ) \
FIELD( 8A,  0, EDX,  8,  2, SVMEDX_RSVD0,                        NO,    0 ) \
FLAG(  8A,  0, EDX, 10,  1, SVM_PAUSE_FILTER,                    NO,    0 ) \
CPUID_8A_EDX_11 \
FLAG(  8A,  0, EDX, 12,  1, SVM_PAUSE_THRESHOLD,                 NO,    0 ) \
FLAG(  8A,  0, EDX, 13,  1, SVM_AVIC,                            NO,    0 ) \
CPUID_8A_EDX_14 \
FLAG(  8A,  0, EDX, 15,  1, SVM_V_VMSAVE_VMLOAD,                 NO,    0 ) \
FLAG(  8A,  0, EDX, 16,  1, SVM_VGIF,                            NO,    0 ) \
CPUID_8A_EDX_17 \
FIELD( 8A,  0, EDX, 18, 14, SVMEDX_RSVD,                         NO,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_819                                          \
FIELD(819,  0, EAX,  0, 12, L1_ITLB_ENTRIES_1G_PGS,              NA,    0 ) \
FIELD(819,  0, EAX, 12,  4, L1_ITLB_ASSOC_1G_PGS,                NA,    0 ) \
FIELD(819,  0, EAX, 16, 12, L1_DTLB_ENTRIES_1G_PGS,              NA,    0 ) \
FIELD(819,  0, EAX, 28,  4, L1_DTLB_ASSOC_1G_PGS,                NA,    0 ) \
FIELD(819,  0, EBX,  0, 12, L2_ITLB_ENTRIES_1G_PGS,              NA,    0 ) \
FIELD(819,  0, EBX, 12,  4, L2_ITLB_ASSOC_1G_PGS,                NA,    0 ) \
FIELD(819,  0, EBX, 16, 12, L2_DTLB_ENTRIES_1G_PGS,              NA,    0 ) \
FIELD(819,  0, EBX, 28,  4, L2_DTLB_ASSOC_1G_PGS,                NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_81A                                          \
FLAG( 81A,  0, EAX,  0,  1, FP128,                               NA,    0 ) \
FLAG( 81A,  0, EAX,  1,  1, MOVU,                                NA,    0 ) \
FLAG( 81A,  0, EAX,  2,  1, FP256,                               NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_81B                                          \
FLAG( 81B,  0, EAX,  0,  1, IBS_FFV,                             NA,    0 ) \
FLAG( 81B,  0, EAX,  1,  1, IBS_FETCHSAM,                        NA,    0 ) \
FLAG( 81B,  0, EAX,  2,  1, IBS_OPSAM,                           NA,    0 ) \
FLAG( 81B,  0, EAX,  3,  1, RW_OPCOUNT,                          NA,    0 ) \
FLAG( 81B,  0, EAX,  4,  1, OPCOUNT,                             NA,    0 ) \
FLAG( 81B,  0, EAX,  5,  1, BRANCH_TARGET_ADDR,                  NA,    0 ) \
FLAG( 81B,  0, EAX,  6,  1, OPCOUNT_EXT,                         NA,    0 ) \
FLAG( 81B,  0, EAX,  7,  1, RIP_INVALID_CHECK,                   NA,    0 ) \
FLAG( 81B,  0, EAX,  8,  1, OP_BRN_FUSE,                         NA,    0 ) \
FLAG( 81B,  0, EAX,  9,  1, IBS_FETCH_CTL_EXTD,                  NA,    0 ) \
FLAG( 81B,  0, EAX, 10,  1, IBS_OP_DATA4,                        NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_81C                                          \
FLAG( 81C,  0, EAX,  0,  1, LWP_AVAIL,                           NA,    0 ) \
FLAG( 81C,  0, EAX,  1,  1, LWP_VAL_AVAIL,                       NA,    0 ) \
FLAG( 81C,  0, EAX,  2,  1, LWP_IRE_AVAIL,                       NA,    0 ) \
FLAG( 81C,  0, EAX,  3,  1, LWP_BRE_AVAIL,                       NA,    0 ) \
FLAG( 81C,  0, EAX,  4,  1, LWP_DME_AVAIL,                       NA,    0 ) \
FLAG( 81C,  0, EAX,  5,  1, LWP_CNH_AVAIL,                       NA,    0 ) \
FLAG( 81C,  0, EAX,  6,  1, LWP_RNH_AVAIL,                       NA,    0 ) \
FLAG( 81C,  0, EAX, 29,  1, LWP_CONT_AVAIL,                      NA,    0 ) \
FLAG( 81C,  0, EAX, 30,  1, LWP_PTSC_AVAIL,                      NA,    0 ) \
FLAG( 81C,  0, EAX, 31,  1, LWP_INT_AVAIL,                       NA,    0 ) \
FIELD(81C,  0, EBX,  0,  8, LWP_CB_SIZE,                         NA,    0 ) \
FIELD(81C,  0, EBX,  8,  8, LWP_EVENT_SIZE,                      NA,    0 ) \
FIELD(81C,  0, EBX, 16,  8, LWP_MAX_EVENTS,                      NA,    0 ) \
FIELD(81C,  0, EBX, 24,  8, LWP_EVENT_OFFSET,                    NA,    0 ) \
FIELD(81C,  0, ECX,  0,  4, LWP_LATENCY_MAX,                     NA,    0 ) \
FLAG( 81C,  0, ECX,  5,  1, LWP_DATA_ADDR_VALID,                 NA,    0 ) \
FIELD(81C,  0, ECX,  6,  3, LWP_LATENCY_ROUND,                   NA,    0 ) \
FIELD(81C,  0, ECX,  9,  7, LWP_VERSION,                         NA,    0 ) \
FIELD(81C,  0, ECX, 16,  8, LWP_MIN_BUF_SIZE,                    NA,    0 ) \
FLAG( 81C,  0, ECX, 28,  1, LWP_BRANCH_PRED,                     NA,    0 ) \
FLAG( 81C,  0, ECX, 29,  1, LWP_IP_FILTERING,                    NA,    0 ) \
FLAG( 81C,  0, ECX, 30,  1, LWP_CACHE_LEVEL,                     NA,    0 ) \
FLAG( 81C,  0, ECX, 31,  1, LWP_CACHE_LATENCY,                   NA,    0 ) \
FLAG( 81C,  0, EDX,  0,  1, LWP_SUPPORTED,                       NA,    0 ) \
FLAG( 81C,  0, EDX,  1,  1, LWP_VAL_SUPPORTED,                   NA,    0 ) \
FLAG( 81C,  0, EDX,  2,  1, LWP_IRE_SUPPORTED,                   NA,    0 ) \
FLAG( 81C,  0, EDX,  3,  1, LWP_BRE_SUPPORTED,                   NA,    0 ) \
FLAG( 81C,  0, EDX,  4,  1, LWP_DME_SUPPORTED,                   NA,    0 ) \
FLAG( 81C,  0, EDX,  5,  1, LWP_CNH_SUPPORTED,                   NA,    0 ) \
FLAG( 81C,  0, EDX,  6,  1, LWP_RNH_SUPPORTED,                   NA,    0 ) \
FLAG( 81C,  0, EDX, 29,  1, LWP_CONT_SUPPORTED,                  NA,    0 ) \
FLAG( 81C,  0, EDX, 30,  1, LWP_PTSC_SUPPORTED,                  NA,    0 ) \
FLAG( 81C,  0, EDX, 31,  1, LWP_INT_SUPPORTED,                   NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_81D                                          \
FIELD(81D,  0, EAX,  0,  5, LEAF81D_CACHE_TYPE,                  NA,    0 ) \
FIELD(81D,  0, EAX,  5,  3, LEAF81D_CACHE_LEVEL,                 NA,    0 ) \
FLAG( 81D,  0, EAX,  8,  1, LEAF81D_CACHE_SELF_INIT,             NA,    0 ) \
FLAG( 81D,  0, EAX,  9,  1, LEAF81D_CACHE_FULLY_ASSOC,           NA,    0 ) \
FIELD(81D,  0, EAX, 14, 12, LEAF81D_NUM_SHARING_CACHE,           NA,    0 ) \
FIELD(81D,  0, EBX,  0, 12, LEAF81D_CACHE_LINE_SIZE,             NA,    0 ) \
FIELD(81D,  0, EBX, 12, 10, LEAF81D_CACHE_PHYS_PARTITIONS,       NA,    0 ) \
FIELD(81D,  0, EBX, 22, 10, LEAF81D_CACHE_WAYS,                  NA,    0 ) \
FIELD(81D,  0, ECX,  0, 32, LEAF81D_CACHE_NUM_SETS,              NA,    0 ) \
FLAG( 81D,  0, EDX,  0,  1, LEAF81D_CACHE_WBINVD,                NA,    0 ) \
FLAG( 81D,  0, EDX,  1,  1, LEAF81D_CACHE_INCLUSIVE,             NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_81E                                          \
FIELD(81E,  0, EAX,  0, 32, EXTENDED_APICID,                     NA,    0 ) \
FIELD(81E,  0, EBX,  0,  8, COMPUTE_UNIT_ID,                     NA,    0 ) \
FIELD(81E,  0, EBX,  8,  2, CORES_PER_COMPUTE_UNIT,              NA,    0 ) \
FIELD(81E,  0, ECX,  0,  8, NODEID_VAL,                          NA,    0 ) \
FIELD(81E,  0, ECX,  8,  3, NODES_PER_PKG,                       NA,    0 )

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_81F                                          \
FLAG( 81F,  0, EAX,  0,  1, SME,                                 NO,    0 ) \
FLAG( 81F,  0, EAX,  1,  1, SEV,                                 YES, FUT ) \
FLAG( 81F,  0, EAX,  2,  1, PAGE_FLUSH_MSR,                      NO,    0 ) \
FLAG( 81F,  0, EAX,  3,  1, SEV_ES,                              YES, FUT ) \
FIELD(81F,  0, EBX,  0,  6, SME_PAGE_TABLE_BIT_NUM,              YES, FUT ) \
FIELD(81F,  0, EBX,  6,  6, SME_PHYS_ADDR_SPACE_REDUCTION,       NO,    0 ) \
FIELD(81F,  0, ECX,  0, 32, NUM_ENCRYPTED_GUESTS,                NO,    0 ) \
FIELD(81F,  0, EDX,  0, 32, SEV_MIN_ASID,                        NO,    0 )

#define INTEL_CPUID_FIELD_DATA

#define AMD_CPUID_FIELD_DATA

#define CPUID_FIELD_DATA                                              \
   CPUID_FIELD_DATA_LEVEL_0                                           \
   CPUID_FIELD_DATA_LEVEL_1                                           \
   CPUID_FIELD_DATA_LEVEL_2                                           \
   CPUID_FIELD_DATA_LEVEL_4                                           \
   CPUID_FIELD_DATA_LEVEL_5                                           \
   CPUID_FIELD_DATA_LEVEL_6                                           \
   CPUID_FIELD_DATA_LEVEL_7                                           \
   CPUID_FIELD_DATA_LEVEL_A                                           \
   CPUID_FIELD_DATA_LEVEL_B                                           \
   CPUID_FIELD_DATA_LEVEL_D                                           \
   CPUID_FIELD_DATA_LEVEL_F                                           \
   CPUID_FIELD_DATA_LEVEL_10                                          \
   CPUID_FIELD_DATA_LEVEL_12                                          \
   CPUID_FIELD_DATA_LEVEL_14                                          \
   CPUID_FIELD_DATA_LEVEL_15                                          \
   CPUID_FIELD_DATA_LEVEL_16                                          \
   CPUID_FIELD_DATA_LEVEL_17                                          \
   CPUID_FIELD_DATA_LEVEL_400                                         \
   CPUID_FIELD_DATA_LEVEL_401                                         \
   CPUID_FIELD_DATA_LEVEL_402                                         \
   CPUID_FIELD_DATA_LEVEL_403                                         \
   CPUID_FIELD_DATA_LEVEL_404                                         \
   CPUID_FIELD_DATA_LEVEL_405                                         \
   CPUID_FIELD_DATA_LEVEL_406                                         \
   CPUID_FIELD_DATA_LEVEL_410                                         \
   CPUID_FIELD_DATA_LEVEL_80                                          \
   CPUID_FIELD_DATA_LEVEL_81                                          \
   CPUID_FIELD_DATA_LEVEL_82                                          \
   CPUID_FIELD_DATA_LEVEL_83                                          \
   CPUID_FIELD_DATA_LEVEL_84                                          \
   CPUID_FIELD_DATA_LEVEL_85                                          \
   CPUID_FIELD_DATA_LEVEL_86                                          \
   CPUID_FIELD_DATA_LEVEL_87                                          \
   CPUID_FIELD_DATA_LEVEL_88                                          \
   CPUID_FIELD_DATA_LEVEL_8A                                          \
   CPUID_FIELD_DATA_LEVEL_819                                         \
   CPUID_FIELD_DATA_LEVEL_81A                                         \
   CPUID_FIELD_DATA_LEVEL_81B                                         \
   CPUID_FIELD_DATA_LEVEL_81C                                         \
   CPUID_FIELD_DATA_LEVEL_81D                                         \
   CPUID_FIELD_DATA_LEVEL_81E                                         \
   CPUID_FIELD_DATA_LEVEL_81F                                         \
   INTEL_CPUID_FIELD_DATA                                             \
   AMD_CPUID_FIELD_DATA

/*
 * Define all field and flag values as an enum.  The result is a full
 * set of values taken from the table above in the form:
 *
 * CPUID_<name>_MASK  == mask for feature/field
 * CPUID_<name>_SHIFT == offset of field
 *
 * e.g. - CPUID_VIRT_BITS_MASK  = 0xff00
 *      - CPUID_VIRT_BITS_SHIFT = 8
 */
#define VMW_BIT_MASK(shift)  (0xffffffffu >> (32 - shift))

#define FIELD(lvl, ecxIn, reg, bitpos, size, name, s, hwv)     \
   CPUID_##name##_SHIFT        = bitpos,                       \
   CPUID_##name##_MASK         = VMW_BIT_MASK(size) << bitpos, \
   CPUID_INTERNAL_SHIFT_##name = bitpos,                       \
   CPUID_INTERNAL_MASK_##name  = VMW_BIT_MASK(size) << bitpos, \
   CPUID_INTERNAL_REG_##name   = CPUID_REG_##reg,              \
   CPUID_INTERNAL_EAXIN_##name = CPUID_LEVEL_VAL_##lvl,        \
   CPUID_INTERNAL_ECXIN_##name = ecxIn,

#define FLAG FIELD

enum {
   /* Define data for every CPUID field we have */
   CPUID_FIELD_DATA
};
#undef VMW_BIT_MASK
#undef FIELD
#undef FLAG

/*
 * CPUID_MASK --
 * CPUID_SHIFT --
 * CPUID_ISSET --
 * CPUID_GET --
 * CPUID_SET --
 * CPUID_CLEAR --
 * CPUID_SETTO --
 *
 * Accessor macros for all CPUID consts/fields/flags.  Level and reg are not
 * required, but are used to force compile-time asserts which help verify that
 * the flag is being used on the right CPUID input and result register.
 *
 * Note: ASSERT_ON_COMPILE is duplicated rather than factored into its own
 * macro, because token concatenation does not work as expected if an input is
 * #defined (e.g. APIC) when macros are nested.  Also, compound statements
 * within parenthes is a GCC extension, so we must use runtime asserts with
 * other compilers.
 */

#if defined(__GNUC__) && !defined(__clang__)

#define CPUID_MASK(eaxIn, reg, flag)                                    \
   ({                                                                   \
      ASSERT_ON_COMPILE(eaxIn == CPUID_INTERNAL_EAXIN_##flag &&         \
              CPUID_REG_##reg == (CpuidReg)CPUID_INTERNAL_REG_##flag);  \
      CPUID_INTERNAL_MASK_##flag;                                       \
   })

#define CPUID_SHIFT(eaxIn, reg, flag)                                   \
   ({                                                                   \
      ASSERT_ON_COMPILE(eaxIn == CPUID_INTERNAL_EAXIN_##flag &&         \
              CPUID_REG_##reg == (CpuidReg)CPUID_INTERNAL_REG_##flag);  \
      CPUID_INTERNAL_SHIFT_##flag;                                      \
   })

#define CPUID_ISSET(eaxIn, reg, flag, data)                             \
   ({                                                                   \
      ASSERT_ON_COMPILE(eaxIn == CPUID_INTERNAL_EAXIN_##flag &&         \
              CPUID_REG_##reg == (CpuidReg)CPUID_INTERNAL_REG_##flag);  \
      (((data) & CPUID_INTERNAL_MASK_##flag) != 0);                     \
   })

#define CPUID_GET(eaxIn, reg, field, data)                              \
   ({                                                                   \
      ASSERT_ON_COMPILE(eaxIn == CPUID_INTERNAL_EAXIN_##field &&        \
              CPUID_REG_##reg == (CpuidReg)CPUID_INTERNAL_REG_##field); \
      (((uint32)(data) & CPUID_INTERNAL_MASK_##field) >>                \
       CPUID_INTERNAL_SHIFT_##field);                                   \
   })

#else

/*
 * CPUIDCheck --
 *
 * Return val after verifying parameters.
 */

static INLINE uint32
CPUIDCheck(int32 eaxIn, int32 eaxInCheck,
           CpuidReg reg, CpuidReg regCheck, uint32 val)
{
   ASSERT(eaxIn == eaxInCheck && reg == regCheck);
   return val;
}

#define CPUID_MASK(eaxIn, reg, flag)                                    \
   CPUIDCheck(eaxIn, CPUID_INTERNAL_EAXIN_##flag,                       \
              CPUID_REG_##reg, (CpuidReg)CPUID_INTERNAL_REG_##flag,     \
              CPUID_INTERNAL_MASK_##flag)

#define CPUID_SHIFT(eaxIn, reg, flag)                                   \
   CPUIDCheck(eaxIn, CPUID_INTERNAL_EAXIN_##flag,                       \
              CPUID_REG_##reg, (CpuidReg)CPUID_INTERNAL_REG_##flag,     \
              CPUID_INTERNAL_SHIFT_##flag)

#define CPUID_ISSET(eaxIn, reg, flag, data)                             \
   (CPUIDCheck(eaxIn, CPUID_INTERNAL_EAXIN_##flag,                      \
               CPUID_REG_##reg, (CpuidReg)CPUID_INTERNAL_REG_##flag,    \
               CPUID_INTERNAL_MASK_##flag & (data)) != 0)

#define CPUID_GET(eaxIn, reg, field, data)                              \
   CPUIDCheck(eaxIn, CPUID_INTERNAL_EAXIN_##field,                      \
              CPUID_REG_##reg, (CpuidReg)CPUID_INTERNAL_REG_##field,    \
              ((uint32)(data) & CPUID_INTERNAL_MASK_##field) >>         \
              CPUID_INTERNAL_SHIFT_##field)

#endif


#define CPUID_SET(eaxIn, reg, flag, dataPtr)                            \
   do {                                                                 \
      ASSERT_ON_COMPILE(                                                \
         (uint32)eaxIn   == (uint32)CPUID_INTERNAL_EAXIN_##flag &&      \
         CPUID_REG_##reg == (CpuidReg)CPUID_INTERNAL_REG_##flag);       \
      *(dataPtr) |= CPUID_INTERNAL_MASK_##flag;                         \
   } while (0)

#define CPUID_CLEAR(eaxIn, reg, flag, dataPtr)                          \
   do {                                                                 \
      ASSERT_ON_COMPILE(                                                \
         (uint32)eaxIn   == (uint32)CPUID_INTERNAL_EAXIN_##flag &&      \
         CPUID_REG_##reg == (CpuidReg)CPUID_INTERNAL_REG_##flag);       \
      *(dataPtr) &= ~CPUID_INTERNAL_MASK_##flag;                        \
   } while (0)

#define CPUID_SETTO(eaxIn, reg, field, dataPtr, val)                    \
   do {                                                                 \
      uint32 _v = val;                                                  \
      uint32 *_d = dataPtr;                                             \
      ASSERT_ON_COMPILE(                                                \
         (uint32)eaxIn   == (uint32)CPUID_INTERNAL_EAXIN_##field &&     \
         CPUID_REG_##reg == (CpuidReg)CPUID_INTERNAL_REG_##field);      \
      *_d = (*_d & ~CPUID_INTERNAL_MASK_##field) |                      \
         (_v << CPUID_INTERNAL_SHIFT_##field);                          \
      ASSERT(_v == (*_d & CPUID_INTERNAL_MASK_##field) >>               \
             CPUID_INTERNAL_SHIFT_##field);                             \
   } while (0)


/*
 * Definitions of various fields' values and more complicated
 * macros/functions for reading cpuid fields.
 */

#define CPUID_FAMILY_EXTENDED        15

/* Effective Intel CPU Families */
#define CPUID_FAMILY_486              4
#define CPUID_FAMILY_P5               5
#define CPUID_FAMILY_P6               6
#define CPUID_FAMILY_P4              15

/* Effective AMD CPU Families */
#define CPUID_FAMILY_5x86            0x4
#define CPUID_FAMILY_K5              0x5
#define CPUID_FAMILY_K6              0x5
#define CPUID_FAMILY_K7              0x6
#define CPUID_FAMILY_K8              0xf
#define CPUID_FAMILY_K8L             0x10
#define CPUID_FAMILY_K8MOBILE        0x11
#define CPUID_FAMILY_LLANO           0x12
#define CPUID_FAMILY_BOBCAT          0x14
#define CPUID_FAMILY_BULLDOZER       0x15  // BD PD SR EX
#define CPUID_FAMILY_KYOTO           0x16  // Note: Jaguar microarch
#define CPUID_FAMILY_ZEN             0x17

/* Effective VIA CPU Families */
#define CPUID_FAMILY_C7               6

/* Effective Hygon CPU Families. */
#define CPUID_FAMILY_DHYANA          0x18

/* Intel model information */
#define CPUID_MODEL_PPRO              1
#define CPUID_MODEL_PII_03            3
#define CPUID_MODEL_PII_05            5
#define CPUID_MODEL_CELERON_06        6
#define CPUID_MODEL_PM_09             9
#define CPUID_MODEL_PM_0D            13
#define CPUID_MODEL_PM_0E            14  // Yonah / Sossaman
#define CPUID_MODEL_CORE_0F          15  // Conroe / Merom
#define CPUID_MODEL_CORE_17        0x17  // Penryn
#define CPUID_MODEL_NEHALEM_1A     0x1a  // Nehalem / Gainestown
#define CPUID_MODEL_ATOM_1C        0x1c  // Silverthorne / Diamondville
#define CPUID_MODEL_CORE_1D        0x1d  // Dunnington
#define CPUID_MODEL_NEHALEM_1E     0x1e  // Lynnfield
#define CPUID_MODEL_NEHALEM_1F     0x1f  // Havendale
#define CPUID_MODEL_NEHALEM_25     0x25  // Westmere / Clarkdale
#define CPUID_MODEL_ATOM_26        0x26  // Lincroft
#define CPUID_MODEL_ATOM_27        0x27  // Saltwell
#define CPUID_MODEL_SANDYBRIDGE_2A 0x2a  // Sandybridge (desktop/mobile)
#define CPUID_MODEL_NEHALEM_2C     0x2c  // Westmere-EP
#define CPUID_MODEL_SANDYBRIDGE_2D 0x2d  // Sandybridge-EP
#define CPUID_MODEL_NEHALEM_2E     0x2e  // Nehalem-EX
#define CPUID_MODEL_NEHALEM_2F     0x2f  // Westmere-EX
#define CPUID_MODEL_ATOM_35        0x35  // Cloverview
#define CPUID_MODEL_ATOM_36        0x36  // Cedarview
#define CPUID_MODEL_ATOM_37        0x37  // Bay Trail
#define CPUID_MODEL_SANDYBRIDGE_3A 0x3a  // Ivy Bridge
#define CPUID_MODEL_HASWELL_3C     0x3c  // Haswell DT
#define CPUID_MODEL_BROADWELL_3D   0x3d  // Broadwell-Ult
#define CPUID_MODEL_SANDYBRIDGE_3E 0x3e  // Ivy Bridge-EP
#define CPUID_MODEL_HASWELL_3F     0x3f  // Haswell EP/EN/EX
#define CPUID_MODEL_HASWELL_45     0x45  // Haswell Ultrathin
#define CPUID_MODEL_HASWELL_46     0x46  // Haswell (Crystal Well)
#define CPUID_MODEL_BROADWELL_47   0x47  // Broadwell (Denlow)
#define CPUID_MODEL_ATOM_4A        0x4a  // Future Silvermont
#define CPUID_MODEL_ATOM_4C        0x4c  // Airmont
#define CPUID_MODEL_ATOM_4D        0x4d  // Avoton
#define CPUID_MODEL_SKYLAKE_4E     0x4e  // Skylake-Y / Kaby Lake U/Y ES
#define CPUID_MODEL_BROADWELL_4F   0x4f  // Broadwell EP/EN/EX
#define CPUID_MODEL_SKYLAKE_55     0x55  // Skylake EP/EN/EX
#define CPUID_MODEL_BROADWELL_56   0x56  // Broadwell DE
#define CPUID_MODEL_KNL_57         0x57  // Knights Landing
#define CPUID_MODEL_ATOM_5A        0x5a  // Future Silvermont
#define CPUID_MODEL_ATOM_5D        0x5d  // Future Silvermont
#define CPUID_MODEL_SKYLAKE_5E     0x5e  // Skylake-S / Kaby Lake S/H ES
#define CPUID_MODEL_ATOM_5F        0x5f  // Denverton
#define CPUID_MODEL_CANNONLAKE_66  0x66  // Cannon Lake
#define CPUID_MODEL_KNM_85         0x85  // Knights Mill
#define CPUID_MODEL_KABYLAKE_8E    0x8e  // Kaby Lake U/Y QS
#define CPUID_MODEL_KABYLAKE_9E    0x9e  // Kaby Lake S/H QS

/* Intel stepping information */
#define CPUID_STEPPING_KABYLAKE_ES     0x8  // Kaby Lake S/H/U/Y ES
#define CPUID_STEPPING_COFFEELAKE_A    0xA  // Coffee Lake U/S/H
#define CPUID_STEPPING_COFFEELAKE_C    0xC  // Last Coffee Lake stepping
#define CPUID_STEPPING_CASCADELAKE_A   0x5  // Cascade Lake A-step
#define CPUID_STEPPING_CASCADELAKE_B1  0x7  // Cascade Lake B1-step
#define CPUID_STEPPING_WHISKEYLAKE     0xB  // Whiskey Lake U

#define CPUID_MODEL_PIII_07    7
#define CPUID_MODEL_PIII_08    8
#define CPUID_MODEL_PIII_0A    10

/* AMD model information */
#define CPUID_MODEL_BARCELONA_02      0x02 // Barcelona (Opteron & Phenom)
#define CPUID_MODEL_SHANGHAI_04       0x04 // Shanghai RB
#define CPUID_MODEL_SHANGHAI_05       0x05 // Shanghai BL
#define CPUID_MODEL_SHANGHAI_06       0x06 // Shanghai DA
#define CPUID_MODEL_ISTANBUL_MAGNY_08 0x08 // Istanbul (6 core) & Magny-cours (12) HY
#define CPUID_MODEL_ISTANBUL_MAGNY_09 0x09 // HY - G34 package
#define CPUID_MODEL_PHAROAH_HOUND_0A  0x0A // Pharoah Hound
#define CPUID_MODEL_PILEDRIVER_1F     0x1F // Max piledriver model defined in BKDG
#define CPUID_MODEL_PILEDRIVER_10     0x10 // family == CPUID_FAMILY_BULLDOZER
#define CPUID_MODEL_PILEDRIVER_02     0x02 // family == CPUID_FAMILY_BULLDOZER
#define CPUID_MODEL_OPTERON_REVF_41   0x41 // family == CPUID_FAMILY_K8
#define CPUID_MODEL_KYOTO_00          0x00 // family == CPUID_FAMILY_KYOTO
#define CPUID_MODEL_STEAMROLLER_3F    0x3F // Max Steamroller model defined in BKDG
#define CPUID_MODEL_STEAMROLLER_30    0x30 // family == CPUID_FAMILY_BULLDOZER
#define CPUID_MODEL_EXCAVATOR_60      0x60 // family == CPUID_FAMILY_BULLDOZER
#define CPUID_MODEL_EXCAVATOR_6F      0x6F // Max Excavator model defined in BKDG
#define CPUID_MODEL_ZEN_00            0x00 // family == CPUID_FAMILY_ZEN
#define CPUID_MODEL_ZEN_NAPLES_01     0x01 // family == CPUID_FAMILY_ZEN
#define CPUID_MODEL_ZEN_1F            0x1F // Max Zen model defined in BKDG
#define CPUID_MODEL_ZEN2_30           0x30 // family == CPUID_FAMILY_ZEN
#define CPUID_MODEL_ZEN2_3F           0x3F // Max Zen2 model

/* AMD stepping information */
#define CPUID_STEPPING_ZEN_NAPLES_B2  0x02 // Zen Naples ZP-B2

/* VIA model information */
#define CPUID_MODEL_NANO                15 // Isaiah

/* Hygon model information. */
#define CPUID_MODEL_DHYANA_A             0 // Dhyana A

/*
 *----------------------------------------------------------------------
 *
 * CPUID_IsVendor{AMD,Intel,VIA,Hygon} --
 *
 *      Determines if the vendor string in cpuid id0 is from
 *      {AMD,Intel,VIA,Hygon}.
 *
 * Results:
 *      True iff vendor string is CPUID_{AMD,INTEL,VIA,HYGON}_VENDOR_STRING
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CPUID_IsRawVendor(CPUIDRegs *id0, const char* vendor)
{
   // hard to get strcmp() in some environments, so do it in the raw
   return (id0->ebx == *(const uint32 *)(uintptr_t) (vendor + 0) &&
           id0->ecx == *(const uint32 *)(uintptr_t) (vendor + 4) &&
           id0->edx == *(const uint32 *)(uintptr_t) (vendor + 8));
}

static INLINE Bool
CPUID_IsVendorAMD(CPUIDRegs *id0)
{
   return CPUID_IsRawVendor(id0, CPUID_AMD_VENDOR_STRING);
}

static INLINE Bool
CPUID_IsVendorIntel(CPUIDRegs *id0)
{
   return CPUID_IsRawVendor(id0, CPUID_INTEL_VENDOR_STRING);
}

static INLINE Bool
CPUID_IsVendorVIA(CPUIDRegs *id0)
{
   return CPUID_IsRawVendor(id0, CPUID_VIA_VENDOR_STRING);
}

static INLINE Bool
CPUID_IsVendorHygon(CPUIDRegs *id0)
{
   return CPUID_IsRawVendor(id0, CPUID_HYGON_VENDOR_STRING);
}

static INLINE uint32
CPUID_EFFECTIVE_FAMILY(uint32 v) /* %eax from CPUID with %eax=1. */
{
   uint32 f = CPUID_GET(1, EAX, FAMILY, v);
   return f != CPUID_FAMILY_EXTENDED ? f : f +
      CPUID_GET(1, EAX, EXTENDED_FAMILY, v);
}

/* Normally only used when FAMILY==CPUID_FAMILY_EXTENDED, but Intel is
 * now using the extended model field for FAMILY==CPUID_FAMILY_P6 to
 * refer to the newer Core2 CPUs
 */
static INLINE uint32
CPUID_EFFECTIVE_MODEL(uint32 v) /* %eax from CPUID with %eax=1. */
{
   uint32 m = CPUID_GET(1, EAX, MODEL, v);
   uint32 em = CPUID_GET(1, EAX, EXTENDED_MODEL, v);
   return m + (em << 4);
}

static INLINE uint32
CPUID_EFFECTIVE_STEPPING(uint32 v) /* %eax from CPUID with %eax=1. */
{
   return CPUID_GET(1, EAX, STEPPING, v);
}

/*
 * Notice that CPUID families for Intel and AMD overlap. The following macros
 * should only be used AFTER the manufacturer has been established (through
 * the use of CPUID standard function 0).
 */
static INLINE Bool
CPUID_FAMILY_IS_486(uint32 eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_486;
}

static INLINE Bool
CPUID_FAMILY_IS_P5(uint32 eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_P5;
}

static INLINE Bool
CPUID_FAMILY_IS_P6(uint32 eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_P6;
}

static INLINE Bool
CPUID_FAMILY_IS_PENTIUM4(uint32 eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_P4;
}

/*
 * Intel Pentium M processors are Yonah/Sossaman or an older P-M
 */
static INLINE Bool
CPUID_UARCH_IS_PENTIUM_M(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          (CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_PM_09 ||
           CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_PM_0D ||
           CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_PM_0E);
}

/*
 * Intel Core processors are Merom, Conroe, Woodcrest, Clovertown,
 * Penryn, Dunnington, Kentsfield, Yorktown, Harpertown, ........
 */
static INLINE Bool
CPUID_UARCH_IS_CORE(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   uint32 model = CPUID_EFFECTIVE_MODEL(v);
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          model >= CPUID_MODEL_CORE_0F &&
          (model < CPUID_MODEL_NEHALEM_1A ||
           model == CPUID_MODEL_CORE_1D);
}

/*
 * Intel Nehalem processors are: Nehalem, Gainestown, Lynnfield, Clarkdale.
 */
static INLINE Bool
CPUID_UARCH_IS_NEHALEM(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   uint32 effectiveModel = CPUID_EFFECTIVE_MODEL(v);

   return CPUID_FAMILY_IS_P6(v) &&
          (effectiveModel == CPUID_MODEL_NEHALEM_1A ||
           effectiveModel == CPUID_MODEL_NEHALEM_1E ||
           effectiveModel == CPUID_MODEL_NEHALEM_1F ||
           effectiveModel == CPUID_MODEL_NEHALEM_25 ||
           effectiveModel == CPUID_MODEL_NEHALEM_2C ||
           effectiveModel == CPUID_MODEL_NEHALEM_2E ||
           effectiveModel == CPUID_MODEL_NEHALEM_2F);
}


static INLINE Bool
CPUID_UARCH_IS_SANDYBRIDGE(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   uint32 effectiveModel = CPUID_EFFECTIVE_MODEL(v);

   return CPUID_FAMILY_IS_P6(v) &&
          (effectiveModel == CPUID_MODEL_SANDYBRIDGE_2A ||
           effectiveModel == CPUID_MODEL_SANDYBRIDGE_2D ||
           effectiveModel == CPUID_MODEL_SANDYBRIDGE_3E ||
           effectiveModel == CPUID_MODEL_SANDYBRIDGE_3A);
}


static INLINE Bool
CPUID_MODEL_IS_BROADWELL(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   uint32 effectiveModel = CPUID_EFFECTIVE_MODEL(v);

   return CPUID_FAMILY_IS_P6(v) &&
          (effectiveModel == CPUID_MODEL_BROADWELL_3D ||
           effectiveModel == CPUID_MODEL_BROADWELL_47 ||
           effectiveModel == CPUID_MODEL_BROADWELL_4F ||
           effectiveModel == CPUID_MODEL_BROADWELL_56);
}


static INLINE Bool
CPUID_MODEL_IS_HASWELL(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   uint32 effectiveModel = CPUID_EFFECTIVE_MODEL(v);

   return CPUID_FAMILY_IS_P6(v) &&
          (effectiveModel == CPUID_MODEL_HASWELL_3C ||
           effectiveModel == CPUID_MODEL_HASWELL_3F ||
           effectiveModel == CPUID_MODEL_HASWELL_45 ||
           effectiveModel == CPUID_MODEL_HASWELL_46);
}

static INLINE Bool
CPUID_MODEL_IS_CASCADELAKE(uint32 v) // IN: %eax from CPUID with %eax=1.
{
      /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_SKYLAKE_55 &&
          CPUID_EFFECTIVE_STEPPING(v) >= CPUID_STEPPING_CASCADELAKE_A &&
          CPUID_EFFECTIVE_STEPPING(v) <= CPUID_STEPPING_CASCADELAKE_B1;
}


static INLINE Bool
CPUID_MODEL_IS_SKYLAKE(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          ((CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_SKYLAKE_5E &&
            CPUID_EFFECTIVE_STEPPING(v) != CPUID_STEPPING_KABYLAKE_ES) ||
           (CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_SKYLAKE_55 &&
            !CPUID_MODEL_IS_CASCADELAKE(v))                            ||
           (CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_SKYLAKE_4E &&
            CPUID_EFFECTIVE_STEPPING(v) != CPUID_STEPPING_KABYLAKE_ES));
}

static INLINE Bool
CPUID_MODEL_IS_COFFEELAKE(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          ((CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_KABYLAKE_9E &&
            CPUID_EFFECTIVE_STEPPING(v) >= CPUID_STEPPING_COFFEELAKE_A &&
            CPUID_EFFECTIVE_STEPPING(v) <= CPUID_STEPPING_COFFEELAKE_C) ||
           (CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_KABYLAKE_8E &&
            CPUID_EFFECTIVE_STEPPING(v) == CPUID_STEPPING_COFFEELAKE_A));
}

static INLINE Bool
CPUID_MODEL_IS_WHISKEYLAKE(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_KABYLAKE_8E &&
          CPUID_EFFECTIVE_STEPPING(v) == CPUID_STEPPING_WHISKEYLAKE;
}

static INLINE Bool
CPUID_MODEL_IS_KABYLAKE(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
      return CPUID_FAMILY_IS_P6(v) &&
          ((CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_KABYLAKE_9E &&
            CPUID_EFFECTIVE_STEPPING(v) < CPUID_STEPPING_COFFEELAKE_A) ||
           (CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_KABYLAKE_8E &&
            CPUID_EFFECTIVE_STEPPING(v) < CPUID_STEPPING_COFFEELAKE_A) ||
           (CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_SKYLAKE_5E &&
            CPUID_EFFECTIVE_STEPPING(v) == CPUID_STEPPING_KABYLAKE_ES) ||
           (CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_SKYLAKE_4E &&
            CPUID_EFFECTIVE_STEPPING(v) == CPUID_STEPPING_KABYLAKE_ES));
}

static INLINE Bool
CPUID_MODEL_IS_CANNONLAKE(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_CANNONLAKE_66;
}

static INLINE Bool
CPUID_UARCH_IS_SKYLAKE(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_MODEL_IS_SKYLAKE(v)     ||
          CPUID_MODEL_IS_KABYLAKE(v)    ||
          CPUID_MODEL_IS_COFFEELAKE(v)  ||
          CPUID_MODEL_IS_WHISKEYLAKE(v) ||
          CPUID_MODEL_IS_CASCADELAKE(v) ||
          CPUID_MODEL_IS_CANNONLAKE(v);
}


static INLINE Bool
CPUID_UARCH_IS_HASWELL(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          (CPUID_MODEL_IS_BROADWELL(v) || CPUID_MODEL_IS_HASWELL(v));
}


static INLINE Bool
CPUID_MODEL_IS_CENTERTON(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_ATOM_1C;
}

static INLINE Bool
CPUID_MODEL_IS_AVOTON(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_ATOM_4D;
}

static INLINE Bool
CPUID_MODEL_IS_BAYTRAIL(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_ATOM_37;
}

static INLINE Bool
CPUID_UARCH_IS_SILVERMONT(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          (CPUID_MODEL_IS_AVOTON(v) || CPUID_MODEL_IS_BAYTRAIL(v));
}

static INLINE Bool
CPUID_MODEL_IS_DENVERTON(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_ATOM_5F;
}

static INLINE Bool
CPUID_MODEL_IS_WESTMERE(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   uint32 effectiveModel = CPUID_EFFECTIVE_MODEL(v);

   return CPUID_FAMILY_IS_P6(v) &&
          (effectiveModel == CPUID_MODEL_NEHALEM_25 || // Clarkdale
           effectiveModel == CPUID_MODEL_NEHALEM_2C || // Westmere-EP
           effectiveModel == CPUID_MODEL_NEHALEM_2F);  // Westmere-EX
}


static INLINE Bool
CPUID_MODEL_IS_SANDYBRIDGE(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   uint32 effectiveModel = CPUID_EFFECTIVE_MODEL(v);

   return CPUID_FAMILY_IS_P6(v) &&
          (effectiveModel == CPUID_MODEL_SANDYBRIDGE_2A ||
           effectiveModel == CPUID_MODEL_SANDYBRIDGE_2D);
}


static INLINE Bool
CPUID_MODEL_IS_IVYBRIDGE(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   uint32 effectiveModel = CPUID_EFFECTIVE_MODEL(v);

   return CPUID_FAMILY_IS_P6(v) && (
       effectiveModel == CPUID_MODEL_SANDYBRIDGE_3E ||
       effectiveModel == CPUID_MODEL_SANDYBRIDGE_3A);
}


static INLINE Bool
CPUID_MODEL_IS_KNIGHTS_LANDING(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_KNL_57;
}

static INLINE Bool
CPUID_MODEL_IS_KNIGHTS_MILL(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_KNM_85;
}


static INLINE Bool
CPUID_FAMILY_IS_K7(uint32 eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_K7;
}

static INLINE Bool
CPUID_FAMILY_IS_K8(uint32 eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_K8;
}

/*
 *----------------------------------------------------------------------
 *
 * CPUID_FAMILY_IS_K8EXT --
 *
 *      Return TRUE for family K8 with effective model >= 0x10.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CPUID_FAMILY_IS_K8EXT(uint32 eax)
{
   return CPUID_FAMILY_IS_K8(eax) &&
          CPUID_GET(1, EAX, EXTENDED_MODEL, eax) != 0;
}

static INLINE Bool
CPUID_FAMILY_IS_K8L(uint32 eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_K8L ||
          CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_LLANO;
}

static INLINE Bool
CPUID_FAMILY_IS_LLANO(uint32 eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_LLANO;
}

static INLINE Bool
CPUID_FAMILY_IS_K8MOBILE(uint32 eax)
{
   /* Essentially a K8 (not K8L) part, but with mobile features. */
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_K8MOBILE;
}

static INLINE Bool
CPUID_FAMILY_IS_K8STAR(uint32 eax)
{
   /*
    * Read function name as "K8*", as in wildcard.
    * Matches K8 or K8L or K8MOBILE
    */
   return CPUID_FAMILY_IS_K8(eax) || CPUID_FAMILY_IS_K8L(eax) ||
          CPUID_FAMILY_IS_K8MOBILE(eax);
}

static INLINE Bool
CPUID_FAMILY_IS_BOBCAT(uint32 eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_BOBCAT;
}

static INLINE Bool
CPUID_FAMILY_IS_BULLDOZER(uint32 eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_BULLDOZER;
}

static INLINE Bool
CPUID_FAMILY_IS_KYOTO(uint32 eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_KYOTO;
}

static INLINE Bool
CPUID_FAMILY_IS_ZEN(uint32 eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_ZEN;
}

/*
 * AMD Barcelona (of either Opteron or Phenom kind).
 */
static INLINE Bool
CPUID_MODEL_IS_BARCELONA(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is AMD. */
   return CPUID_EFFECTIVE_FAMILY(v) == CPUID_FAMILY_K8L &&
          CPUID_EFFECTIVE_MODEL(v)  == CPUID_MODEL_BARCELONA_02;
}


static INLINE Bool
CPUID_MODEL_IS_SHANGHAI(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is AMD. */
   return CPUID_EFFECTIVE_FAMILY(v) == CPUID_FAMILY_K8L &&
          (CPUID_MODEL_SHANGHAI_04  <= CPUID_EFFECTIVE_MODEL(v) &&
           CPUID_EFFECTIVE_MODEL(v) <= CPUID_MODEL_SHANGHAI_06);
}


static INLINE Bool
CPUID_MODEL_IS_ISTANBUL_MAGNY(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is AMD. */
   return CPUID_EFFECTIVE_FAMILY(v) == CPUID_FAMILY_K8L &&
          (CPUID_MODEL_ISTANBUL_MAGNY_08 <= CPUID_EFFECTIVE_MODEL(v) &&
           CPUID_EFFECTIVE_MODEL(v)      <= CPUID_MODEL_ISTANBUL_MAGNY_09);
}


static INLINE Bool
CPUID_MODEL_IS_PHAROAH_HOUND(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is AMD. */
   return CPUID_EFFECTIVE_FAMILY(v) == CPUID_FAMILY_K8L &&
          CPUID_EFFECTIVE_MODEL(v)  == CPUID_MODEL_PHAROAH_HOUND_0A;
}


static INLINE Bool
CPUID_MODEL_IS_BULLDOZER(uint32 eax)
{
   /*
    * Bulldozer is models of family 0x15 that are below 10 excluding
    * Piledriver 02.
    */
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_BULLDOZER &&
          CPUID_EFFECTIVE_MODEL(eax)  < CPUID_MODEL_PILEDRIVER_10 &&
          CPUID_EFFECTIVE_MODEL(eax) != CPUID_MODEL_PILEDRIVER_02;
}


static INLINE Bool
CPUID_MODEL_IS_PILEDRIVER(uint32 eax)
{
   /* Piledriver is models 0x02 & 0x10 of family 0x15 (so far). */
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_BULLDOZER &&
          ((CPUID_EFFECTIVE_MODEL(eax) >= CPUID_MODEL_PILEDRIVER_10 &&
            CPUID_EFFECTIVE_MODEL(eax) <= CPUID_MODEL_PILEDRIVER_1F) ||
           CPUID_EFFECTIVE_MODEL(eax) == CPUID_MODEL_PILEDRIVER_02);
}


static INLINE Bool
CPUID_MODEL_IS_STEAMROLLER(uint32 eax)
{
   /* Steamroller is model 0x30 of family 0x15 (so far). */
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_BULLDOZER &&
          (CPUID_EFFECTIVE_MODEL(eax) >= CPUID_MODEL_STEAMROLLER_30 &&
           CPUID_EFFECTIVE_MODEL(eax) <= CPUID_MODEL_STEAMROLLER_3F);
}


static INLINE Bool
CPUID_MODEL_IS_EXCAVATOR(uint32 eax)
{
   /* Excavator is model 0x60 of family 0x15 (so far). */
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_BULLDOZER &&
          (CPUID_EFFECTIVE_MODEL(eax) >= CPUID_MODEL_EXCAVATOR_60 &&
           CPUID_EFFECTIVE_MODEL(eax) <= CPUID_MODEL_EXCAVATOR_6F);
}


static INLINE Bool
CPUID_MODEL_IS_KYOTO(uint32 eax)
{
   /* Kyoto is models 0x00 of family 0x16 (so far). */
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_KYOTO &&
          CPUID_EFFECTIVE_MODEL(eax) == CPUID_MODEL_KYOTO_00;
}


static INLINE Bool
CPUID_MODEL_IS_ZEN(uint32 eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_ZEN &&
          CPUID_EFFECTIVE_MODEL(eax) <= CPUID_MODEL_ZEN_1F;
}


static INLINE Bool
CPUID_MODEL_IS_ZEN2(uint32 eax)
{
  return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_ZEN &&
         (CPUID_EFFECTIVE_MODEL(eax) >= CPUID_MODEL_ZEN2_30 &&
          CPUID_EFFECTIVE_MODEL(eax) <= CPUID_MODEL_ZEN2_3F);
}


static INLINE Bool
CPUID_FAMILY_IS_DHYANA(uint32 eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_DHYANA;
}


static INLINE Bool
CPUID_MODEL_IS_DHYANA_A(uint32 eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_DHYANA &&
          CPUID_EFFECTIVE_MODEL(eax)  == CPUID_MODEL_DHYANA_A;
}


#define CPUID_TYPE_PRIMARY     0
#define CPUID_TYPE_OVERDRIVE   1
#define CPUID_TYPE_SECONDARY   2

#define CPUID_INTEL_ID4EAX_LEAF4_CACHE_TYPE_NULL      0
#define CPUID_INTEL_ID4EAX_LEAF4_CACHE_TYPE_DATA      1
#define CPUID_INTEL_ID4EAX_LEAF4_CACHE_TYPE_INST      2
#define CPUID_INTEL_ID4EAX_LEAF4_CACHE_TYPE_UNIF      3
#define CPUID_LEAF4_CACHE_TYPE_NULL      0
#define CPUID_LEAF4_CACHE_TYPE_DATA      1
#define CPUID_LEAF4_CACHE_TYPE_INST      2
#define CPUID_LEAF4_CACHE_TYPE_UNIF      3
#define CPUID_LEAF4_CACHE_INDEXING_DIRECT  0
#define CPUID_LEAF4_CACHE_INDEXING_COMPLEX 1

#define CPUID_INTEL_ID4EAX_LEAF4_CACHE_SELF_INIT      0x00000100
#define CPUID_INTEL_ID4EAX_LEAF4_CACHE_FULLY_ASSOC    0x00000200
#define CPUID_LEAF4_CACHE_SELF_INIT      0x00000100
#define CPUID_LEAF4_CACHE_FULLY_ASSOC    0x00000200

#define CPUID_INTEL_IDBECX_LEVEL_TYPE_INVALID   0
#define CPUID_INTEL_IDBECX_LEVEL_TYPE_SMT       1
#define CPUID_INTEL_IDBECX_LEVEL_TYPE_CORE      2
#define CPUID_TOPOLOGY_LEVEL_TYPE_INVALID   0
#define CPUID_TOPOLOGY_LEVEL_TYPE_SMT       1
#define CPUID_TOPOLOGY_LEVEL_TYPE_CORE      2

#define CPUID_AMD_LEAF85_L1_CACHE_FULLY_ASSOC     0xff
#define CPUID_AMD_LEAF86_L2_L3_CACHE_FULLY_ASSOC  0x0f
#define CPUID_AMD_LEAF81D_CACHE_TYPE_NULL   0
#define CPUID_AMD_LEAF81D_CACHE_TYPE_DATA   1
#define CPUID_AMD_LEAF81D_CACHE_TYPE_INST   2
#define CPUID_AMD_LEAF81D_CACHE_TYPE_UNIF   3

/*
 * For certain AMD processors, an lfence instruction is necessary at various
 * places to ensure ordering.
 */

static INLINE Bool
CPUID_VendorRequiresFence(CpuidVendor vendor)
{
   return vendor == CPUID_VENDOR_AMD;
}

static INLINE Bool
CPUID_VersionRequiresFence(uint32 version)
{
   return CPUID_EFFECTIVE_FAMILY(version) == CPUID_FAMILY_K8 &&
          CPUID_EFFECTIVE_MODEL(version) < 0x40;
}

static INLINE Bool
CPUID_ID0RequiresFence(CPUIDRegs *id0)
{
   if (id0->eax == 0) {
      return FALSE;
   }
   return CPUID_IsVendorAMD(id0);
}

static INLINE Bool
CPUID_ID1RequiresFence(CPUIDRegs *id1)
{
   return CPUID_VersionRequiresFence(id1->eax);
}

static INLINE Bool
CPUID_RequiresFence(CpuidVendor vendor, // IN
                    uint32 version)      // IN: %eax from CPUID with %eax=1.
{
   return CPUID_VendorRequiresFence(vendor) &&
          CPUID_VersionRequiresFence(version);
}


/*
 * The following low-level functions compute the number of
 * cores per cpu.  They should be used cautiously because
 * they do not necessarily work on all types of CPUs.
 * High-level functions that are correct for all CPUs are
 * available elsewhere: see lib/cpuidInfo/cpuidInfo.c.
 */

static INLINE uint32
CPUID_IntelCoresPerPackage(uint32 v) /* %eax from CPUID with %eax=4 and %ecx=0. */
{
   // Note: This is not guaranteed to work on older Intel CPUs.
   return 1 + CPUID_GET(4, EAX, LEAF4_CORE_COUNT, v);
}


static INLINE uint32
CPUID_AMDCoresPerPackage(uint32 v) /* %ecx from CPUID with %eax=0x80000008. */
{
   // Note: This is not guaranteed to work on older AMD CPUs.
   return 1 + CPUID_GET(0x80000008, ECX, LEAF88_CORE_COUNT, v);
}


/*
 * Hypervisor CPUID space is 0x400000XX.
 */
static INLINE Bool
CPUID_IsHypervisorLevel(uint32 level)
{
   return (level & 0xffffff00) == 0x40000000;
}

/*
 *----------------------------------------------------------------------
 *
 * CPUID_LevelUsesEcx --
 *
 *      Returns TRUE for leaves that support input ECX != 0 (subleaves).
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
CPUID_LevelUsesEcx(uint32 level) {
   switch (level)
   {

#define CPUIDLEVEL(t, s, v, c, h)   \
      case v:                       \
         return c != 0;

      CPUID_ALL_LEVELS

#undef CPUIDLEVEL

      default:
         return FALSE;
   }
}

/*
 *----------------------------------------------------------------------
 *
 * CPUID_IsValid*Subleaf --
 *
 *      Functions to determine the last subleaf for the level specified
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
CPUID_IsValidBSubleaf(uint32 ebx)  // IN: %ebx = cpuid.b.sublevel.ebx
{
   return ebx != 0;
}

static INLINE Bool
CPUID_IsValid4Subleaf(uint32 eax)  // IN: %eax = cpuid.4.sublevel.eax
{
   return eax != 0;
}

static INLINE Bool
CPUID_IsValid7Subleaf(uint32 eax, uint32 subleaf)  // IN: %eax = cpuid.7.0.eax
{
   /*
    * cpuid.7.0.eax is the max ecx (subleaf) index
    */
   return subleaf <= eax;
}

/*
 *----------------------------------------------------------------------
 *
 * CPUID_IsValidDSubleaf --
 *
 *    It is the caller's repsonsibility to determine if the processor
 *    supports XSAVE and therefore has D sub-leaves.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CPUID_IsValidDSubleaf(uint32 subleaf)  // IN: subleaf to check
{
   return subleaf <= 63;
}

/*
 *----------------------------------------------------------------------
 *
 * CPUID_SupportsMsrPlatformInfo --
 *
 *    Uses vendor and cpuid.1.0.eax to determine if the processor
 *    supports MSR_PLATFORM_INFO.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CPUID_SupportsMsrPlatformInfo(CpuidVendor vendor, uint32 version)
{
   return vendor == CPUID_VENDOR_INTEL &&
          (CPUID_UARCH_IS_NEHALEM(version)     ||
           CPUID_UARCH_IS_SANDYBRIDGE(version) ||
           CPUID_UARCH_IS_HASWELL(version)     ||
           CPUID_UARCH_IS_SKYLAKE(version)     ||
           CPUID_MODEL_IS_KNIGHTS_LANDING(version) ||
           CPUID_MODEL_IS_DENVERTON(version) ||
           CPUID_UARCH_IS_SILVERMONT(version));
}

#ifdef _MSC_VER
#pragma warning (pop)
#endif


#if defined __cplusplus
} // extern "C"
#endif

#endif // _X86CPUID_H_
