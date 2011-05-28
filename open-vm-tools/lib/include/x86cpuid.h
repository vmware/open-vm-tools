/*********************************************************
 * Copyright (C) 1998-2008 VMware, Inc. All rights reserved.
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
#pragma warning (disable :4200) // non-std extension: zero-sized array in struct
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
 * CPUID levels the monitor caches and ones that are not cached, but
 * have fields defined below (short name and actual value).
 * 
 * The first parameter defines whether the level has its default masks
 * generated from the values in this file.  Any level which is marked
 * as FALSE here *must* have all monitor support types set to NA.  A
 * static assert in lib/cpuidcompat/cpuidcompat.c will check this.
 */

#define CPUID_CACHED_LEVELS                     \
   CPUIDLEVEL(TRUE,  0,  0)                     \
   CPUIDLEVEL(TRUE,  1,  1)                     \
   CPUIDLEVEL(FALSE, 5,  5)                     \
   CPUIDLEVEL(TRUE,  7,  7)                     \
   CPUIDLEVEL(FALSE, A,  0xA)                   \
   CPUIDLEVEL(TRUE,  D,  0xD)                   \
   CPUIDLEVEL(FALSE,400, 0x40000000)            \
   CPUIDLEVEL(FALSE,410, 0x40000010)            \
   CPUIDLEVEL(FALSE, 80, 0x80000000)            \
   CPUIDLEVEL(TRUE,  81, 0x80000001)            \
   CPUIDLEVEL(FALSE, 87, 0x80000007)            \
   CPUIDLEVEL(FALSE, 88, 0x80000008)            \
   CPUIDLEVEL(TRUE,  8A, 0x8000000A)

#define CPUID_UNCACHED_LEVELS                   \
   CPUIDLEVEL(FALSE, 4, 4)                      \
   CPUIDLEVEL(FALSE, 6, 6)                      \
   CPUIDLEVEL(FALSE, B, 0xB)                    \
   CPUIDLEVEL(FALSE, 85, 0x80000005)            \
   CPUIDLEVEL(FALSE, 86, 0x80000006)            \
   CPUIDLEVEL(FALSE, 819, 0x80000019)           \
   CPUIDLEVEL(FALSE, 81A, 0x8000001A)           \
   CPUIDLEVEL(FALSE, 81B, 0x8000001B)           \
   CPUIDLEVEL(FALSE, 81C, 0x8000001C)           \
   CPUIDLEVEL(FALSE, 81D, 0x8000001D)           \
   CPUIDLEVEL(FALSE, 81E, 0x8000001E)

#define CPUID_ALL_LEVELS                        \
   CPUID_CACHED_LEVELS                          \
   CPUID_UNCACHED_LEVELS

/* Define cached CPUID levels in the form: CPUID_LEVEL_<ShortName> */
typedef enum {
#define CPUIDLEVEL(t, s, v) CPUID_LEVEL_##s,
   CPUID_CACHED_LEVELS
#undef CPUIDLEVEL
   CPUID_NUM_CACHED_LEVELS
} CpuidCachedLevel;

/* Enum to translate between shorthand name and actual CPUID level value. */
enum {
#define CPUIDLEVEL(t, s, v) CPUID_LEVEL_VAL_##s = v,
   CPUID_ALL_LEVELS
#undef CPUIDLEVEL
};


/* SVM CPUID feature leaf */
#define CPUID_SVM_FEATURES         0x8000000a


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

#define CPUID_HYPERV_HYPERVISOR_VENDOR_STRING  "Microsoft Hv"
#define CPUID_KVM_HYPERVISOR_VENDOR_STRING     "KVMKVMKVM\0\0\0"
#define CPUID_VMWARE_HYPERVISOR_VENDOR_STRING  "VMwareVMware"
#define CPUID_XEN_HYPERVISOR_VENDOR_STRING     "XenVMMXenVMM"

#define CPUID_INTEL_VENDOR_STRING_FIXED "GenuineIntel"
#define CPUID_AMD_VENDOR_STRING_FIXED   "AuthenticAMD"
#define CPUID_CYRIX_VENDOR_STRING_FIXED "CyrixInstead"
#define CPUID_VIA_VENDOR_STRING_FIXED   "CentaurHauls"

/*
 * FIELD can be defined to process the CPUID information provided
 * in the following CPUID_FIELD_DATA macro.  The first parameter is
 * the CPUID level of the feature (must be defined in
 * CPUID_ALL_LEVELS, above.  The second parameter is the CPUID result
 * register in which the field is returned (defined in CPUID_REGS).
 * The third field is the vendor(s) this feature applies to.  "COMMON"
 * means all vendors apply.  UNKNOWN may not be used here.  The fourth
 * and fifth parameters are the bit position of the field and the
 * width, respectively.  The sixth is the text name of the field.
 *
 * The seventh parameters specifies the monitor support
 * characteristics for this field.  The value must be a valid
 * CpuidFieldSupported value (omitting CPUID_FIELD_SUPPORT_ for
 * convenience).  The meaning of those values are described below.
 *
 * The eighth parameter describes whether the feature is capable of
 * being used by usermode code (TRUE), or just CPL0 kernel code
 * (FALSE).
 *
 * FLAG is defined identically to FIELD, but its accessors are more
 * appropriate for 1-bit flags, and compile-time asserts enforce that
 * the size is 1 bit wide.
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
 *     expose the feature to the guest.
 *
 *     NA: Only legal for levels not masked/tested by default (see
 *     above for this definition).  Such fields must always be marked
 *     as NA.
 *
 * These distinctions, when combined with the feature's CPL3
 * properties can be translated into a common CPUID mask string as
 * follows:
 *
 *     NO + CPL3 --> "R" (Reserved).  We don't support the feature,
 *     but we can't properly hide this from applications when using
 *     direct execution or HV with apps that do try/catch/fail, so we
 *     must still perform compatibility checks.
 *
 *     NO + !CPL3 --> "0" (Masked).  We can hide this from the guest.
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
typedef enum {
   CPUID_FIELD_SUPPORTED_NO,
   CPUID_FIELD_SUPPORTED_YES,
   CPUID_FIELD_SUPPORTED_ANY,
   CPUID_FIELD_SUPPORTED_NA,
   CPUID_NUM_FIELD_SUPPORTEDS
} CpuidFieldSupported;


#define CPUID_1_ECX_29
#define CPUID_1_ECX_30
#define CPUID_7_EBX_0
#define CPUID_7_EBX_7
#define CPUID_7_EBX_9


/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,                   MON SUPP, CPL3 */
#define CPUID_FIELD_DATA_LEVEL_0                                               \
FIELD(  0, EAX, COMMON,  0, 32, NUMLEVELS,                         ANY, FALSE) \
FIELD(  0, EBX, COMMON,  0, 32, VENDOR1,                           YES, TRUE)  \
FIELD(  0, ECX, COMMON,  0, 32, VENDOR3,                           YES, TRUE)  \
FIELD(  0, EDX, COMMON,  0, 32, VENDOR2,                           YES, TRUE)
                                                        
/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,                   MON SUPP, CPL3 */
#define CPUID_FIELD_DATA_LEVEL_1                                               \
FIELD(  1, EAX, COMMON,  0,  4, STEPPING,                          ANY, FALSE) \
FIELD(  1, EAX, COMMON,  4,  4, MODEL,                             ANY, FALSE) \
FIELD(  1, EAX, COMMON,  8,  4, FAMILY,                            YES, FALSE) \
FIELD(  1, EAX, COMMON, 12,  2, TYPE,                              ANY, FALSE) \
FIELD(  1, EAX, COMMON, 16,  4, EXTENDED_MODEL,                    ANY, FALSE) \
FIELD(  1, EAX, COMMON, 20,  8, EXTENDED_FAMILY,                   YES, FALSE) \
FIELD(  1, EBX, COMMON,  0,  8, BRAND_ID,                          ANY, FALSE) \
FIELD(  1, EBX, COMMON,  8,  8, CLFL_SIZE,                         ANY, FALSE) \
FIELD(  1, EBX, COMMON, 16,  8, LCPU_COUNT,                        ANY, FALSE) \
FIELD(  1, EBX, COMMON, 24,  8, APICID,                            ANY, FALSE) \
FLAG(   1, ECX, COMMON, 0,   1, SSE3,                              YES, TRUE)  \
FLAG(   1, ECX, COMMON, 1,   1, PCLMULQDQ,                         YES, TRUE)  \
FLAG(   1, ECX, INTEL,  2,   1, DTES64,                            NO,  FALSE) \
FLAG(   1, ECX, COMMON, 3,   1, MWAIT,                             ANY, FALSE) \
FLAG(   1, ECX, INTEL,  4,   1, DSCPL,                             NO,  FALSE) \
FLAG(   1, ECX, INTEL,  5,   1, VMX,                               YES, FALSE) \
FLAG(   1, ECX, VIA,    5,   1, VIA_VMX,                           YES, FALSE) \
FLAG(   1, ECX, INTEL,  6,   1, SMX,                               NO,  FALSE) \
FLAG(   1, ECX, INTEL,  7,   1, EIST,                              NO,  FALSE) \
FLAG(   1, ECX, INTEL,  8,   1, TM2,                               NO,  FALSE) \
FLAG(   1, ECX, COMMON, 9,   1, SSSE3,                             YES, TRUE)  \
FLAG(   1, ECX, INTEL,  10,  1, CNXTID,                            NO,  FALSE) \
FLAG(   1, ECX, INTEL,  11,  1, NDA11,                             NO,  FALSE) \
FLAG(   1, ECX, COMMON, 12,  1, FMA,                               YES, TRUE)  \
FLAG(   1, ECX, COMMON, 13,  1, CMPXCHG16B,                        YES, TRUE)  \
FLAG(   1, ECX, INTEL,  14,  1, xTPR,                              NO,  FALSE) \
FLAG(   1, ECX, INTEL,  15,  1, PDCM,                              NO,  FALSE) \
FLAG(   1, ECX, INTEL,  17,  1, PCID,                              YES, FALSE) \
FLAG(   1, ECX, INTEL,  18,  1, DCA,                               NO,  FALSE) \
FLAG(   1, ECX, COMMON, 19,  1, SSE41,                             YES, TRUE)  \
FLAG(   1, ECX, COMMON, 20,  1, SSE42,                             YES, TRUE)  \
FLAG(   1, ECX, INTEL,  21,  1, x2APIC,                            NO,  FALSE) \
FLAG(   1, ECX, INTEL,  22,  1, MOVBE,                             YES, TRUE)  \
FLAG(   1, ECX, COMMON, 23,  1, POPCNT,                            YES, TRUE)  \
FLAG(   1, ECX, COMMON, 24,  1, TSC_DEADLINE,                      NO,  FALSE) \
FLAG(   1, ECX, COMMON, 25,  1, AES,                               YES, TRUE)  \
FLAG(   1, ECX, COMMON, 26,  1, XSAVE,                             YES, FALSE) \
FLAG(   1, ECX, COMMON, 27,  1, OSXSAVE,                           ANY, FALSE) \
FLAG(   1, ECX, COMMON, 28,  1, AVX,                               YES, FALSE)  \
CPUID_1_ECX_29                                                                 \
CPUID_1_ECX_30                                                                 \
FLAG(   1, ECX, COMMON, 31,  1, HYPERVISOR,                        ANY, FALSE) \
FLAG(   1, EDX, COMMON, 0,   1, FPU,                               YES, TRUE)  \
FLAG(   1, EDX, COMMON, 1,   1, VME,                               YES, FALSE) \
FLAG(   1, EDX, COMMON, 2,   1, DE,                                YES, FALSE) \
FLAG(   1, EDX, COMMON, 3,   1, PSE,                               YES, FALSE) \
FLAG(   1, EDX, COMMON, 4,   1, TSC,                               YES, TRUE)  \
FLAG(   1, EDX, COMMON, 5,   1, MSR,                               YES, FALSE) \
FLAG(   1, EDX, COMMON, 6,   1, PAE,                               YES, FALSE) \
FLAG(   1, EDX, COMMON, 7,   1, MCE,                               YES, FALSE) \
FLAG(   1, EDX, COMMON, 8,   1, CX8,                               YES, TRUE)  \
FLAG(   1, EDX, COMMON, 9,   1, APIC,                              ANY, FALSE) \
FLAG(   1, EDX, COMMON, 11,  1, SEP,                               YES, TRUE)  \
FLAG(   1, EDX, COMMON, 12,  1, MTRR,                              YES, FALSE) \
FLAG(   1, EDX, COMMON, 13,  1, PGE,                               YES, FALSE) \
FLAG(   1, EDX, COMMON, 14,  1, MCA,                               YES, FALSE) \
FLAG(   1, EDX, COMMON, 15,  1, CMOV,                              YES, TRUE)  \
FLAG(   1, EDX, COMMON, 16,  1, PAT,                               YES, FALSE) \
FLAG(   1, EDX, COMMON, 17,  1, PSE36,                             YES, FALSE) \
FLAG(   1, EDX, INTEL,  18,  1, PSN,                               YES, FALSE) \
FLAG(   1, EDX, COMMON, 19,  1, CLFSH,                             YES, TRUE)  \
FLAG(   1, EDX, INTEL,  21,  1, DS,                                YES, FALSE) \
FLAG(   1, EDX, INTEL,  22,  1, ACPI,                              YES, FALSE) \
FLAG(   1, EDX, COMMON, 23,  1, MMX,                               YES, TRUE)  \
FLAG(   1, EDX, COMMON, 24,  1, FXSR,                              YES, TRUE)  \
FLAG(   1, EDX, COMMON, 25,  1, SSE,                               YES, TRUE)  \
FLAG(   1, EDX, COMMON, 26,  1, SSE2,                              YES, TRUE)  \
FLAG(   1, EDX, INTEL,  27,  1, SS,                                YES, FALSE) \
FLAG(   1, EDX, COMMON, 28,  1, HTT,                               ANY, FALSE) \
FLAG(   1, EDX, INTEL,  29,  1, TM,                                NO,  FALSE) \
FLAG(   1, EDX, INTEL,  30,  1, IA64,                              NO,  FALSE) \
FLAG(   1, EDX, INTEL,  31,  1, PBE,                               NO,  FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,                   MON SUPP, CPL3 */
#define CPUID_FIELD_DATA_LEVEL_4                                               \
FIELD(  4, EAX, INTEL,   0,  5, LEAF4_CACHE_TYPE,                  NA,  FALSE) \
FIELD(  4, EAX, INTEL,   5,  3, LEAF4_CACHE_LEVEL,                 NA,  FALSE) \
FLAG(   4, EAX, INTEL,   8,  1, LEAF4_CACHE_SELF_INIT,             NA,  FALSE) \
FLAG(   4, EAX, INTEL,   9,  1, LEAF4_CACHE_FULLY_ASSOC,           NA,  FALSE) \
FIELD(  4, EAX, INTEL,  14, 12, LEAF4_CACHE_NUMHT_SHARING,         NA,  FALSE) \
FIELD(  4, EAX, INTEL,  26,  6, LEAF4_CORE_COUNT,                  NA,  FALSE) \
FIELD(  4, EBX, INTEL,   0, 12, LEAF4_CACHE_LINE,                  NA,  FALSE) \
FIELD(  4, EBX, INTEL,  12, 10, LEAF4_CACHE_PART,                  NA,  FALSE) \
FIELD(  4, EBX, INTEL,  22, 10, LEAF4_CACHE_WAYS,                  NA,  FALSE) \
FIELD(  4, ECX, INTEL,   0, 32, LEAF4_CACHE_SETS,                  NA,  FALSE) \
FLAG(   4, EDX, INTEL,   0,  1, LEAF4_CACHE_WBINVD_NOT_GUARANTEED, NA,  FALSE) \
FLAG(   4, EDX, INTEL,   1,  1, LEAF4_CACHE_IS_INCLUSIVE,          NA,  FALSE)

/*     LEVEL, REG, VENDOR, POS, SIZE, NAME,                   MON SUPP, CPL3 */
#define CPUID_FIELD_DATA_LEVEL_5                                               \
FIELD(  5, EAX, COMMON,  0, 16, MWAIT_MIN_SIZE,                    NA,  FALSE) \
FIELD(  5, EBX, COMMON,  0, 16, MWAIT_MAX_SIZE,                    NA,  FALSE) \
FLAG(   5, ECX, COMMON,  0,  1, MWAIT_EXTENSIONS,                  NA,  FALSE) \
FLAG(   5, ECX, COMMON,  1,  1, MWAIT_INTR_BREAK,                  NA,  FALSE) \
FIELD(  5, EDX, INTEL,   0,  4, MWAIT_C0_SUBSTATE,                 NA,  FALSE) \
FIELD(  5, EDX, INTEL,   4,  4, MWAIT_C1_SUBSTATE,                 NA,  FALSE) \
FIELD(  5, EDX, INTEL,   8,  4, MWAIT_C2_SUBSTATE,                 NA,  FALSE) \
FIELD(  5, EDX, INTEL,  12,  4, MWAIT_C3_SUBSTATE,                 NA,  FALSE) \
FIELD(  5, EDX, INTEL,  16,  4, MWAIT_C4_SUBSTATE,                 NA,  FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,                   MON SUPP, CPL3 */
#define CPUID_FIELD_DATA_LEVEL_6                                               \
FLAG(   6, EAX, INTEL,   0,  1, THERMAL_SENSOR,                    NA,  FALSE) \
FLAG(   6, EAX, INTEL,   1,  1, TURBO_MODE,                        NA,  FALSE) \
FLAG(   6, EAX, INTEL,   2,  1, APIC_INVARIANT,                    NA,  FALSE) \
FIELD(  6, EBX, INTEL,   0,  4, NUM_INTR_THRESHOLDS,               NA,  FALSE) \
FLAG(   6, ECX, INTEL,   0,  1, HW_COORD_FEEDBACK,                 NA,  FALSE) \
FLAG(   6, ECX, INTEL,   3,  1, ENERGY_PERF_BIAS,                  NA,  FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,                   MON SUPP, CPL3 */
#define CPUID_FIELD_DATA_LEVEL_7                                               \
CPUID_7_EBX_0                                                                  \
CPUID_7_EBX_7                                                                  \
CPUID_7_EBX_9

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,                   MON SUPP, CPL3 */
#define CPUID_FIELD_DATA_LEVEL_A                                               \
FIELD(  A, EAX, INTEL,   0,  8, PMC_VERSION,                       NA,  FALSE) \
FIELD(  A, EAX, INTEL,   8,  8, PMC_NUM_GEN,                       NA,  FALSE) \
FIELD(  A, EAX, INTEL,  16,  8, PMC_WIDTH_GEN,                     NA,  FALSE) \
FIELD(  A, EAX, INTEL,  24,  8, PMC_EBX_LENGTH,                    NA,  FALSE) \
FLAG(   A, EBX, INTEL,   0,  1, PMC_CORE_CYCLES,                   NA,  FALSE) \
FLAG(   A, EBX, INTEL,   1,  1, PMC_INSTR_RETIRED,                 NA,  FALSE) \
FLAG(   A, EBX, INTEL,   2,  1, PMC_REF_CYCLES,                    NA,  FALSE) \
FLAG(   A, EBX, INTEL,   3,  1, PMC_LAST_LVL_CREF,                 NA,  FALSE) \
FLAG(   A, EBX, INTEL,   4,  1, PMC_LAST_LVL_CMISS,                NA,  FALSE) \
FLAG(   A, EBX, INTEL,   5,  1, PMC_BR_INST_RETIRED,               NA,  FALSE) \
FLAG(   A, EBX, INTEL,   6,  1, PMC_BR_MISS_RETIRED,               NA,  FALSE) \
FIELD(  A, EDX, INTEL,   0,  5, PMC_NUM_FIXED,                     NA,  FALSE) \
FIELD(  A, EDX, INTEL,   5,  8, PMC_WIDTH_FIXED,                   NA,  FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,                   MON SUPP, CPL3 */
#define CPUID_FIELD_DATA_LEVEL_B                                               \
FIELD(  B, EAX, INTEL,   0,  5, TOPOLOGY_MASK_WIDTH,               NA,  FALSE) \
FIELD(  B, EBX, INTEL,   0, 16, TOPOLOGY_CPUS_SHARING_LEVEL,       NA,  FALSE) \
FIELD(  B, ECX, INTEL,   0,  8, TOPOLOGY_LEVEL_NUMBER,             NA,  FALSE) \
FIELD(  B, ECX, INTEL,   8,  8, TOPOLOGY_LEVEL_TYPE,               NA,  FALSE) \
FIELD(  B, EDX, INTEL,   0, 32, TOPOLOGY_X2APIC_ID,                NA,  FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,                   MON SUPP, CPL3 */
#define CPUID_FIELD_DATA_LEVEL_D                                               \
FLAG(   D, EAX, COMMON,  0,  1, XCR0_MASTER_LEGACY_FP,             YES, FALSE) \
FLAG(   D, EAX, COMMON,  1,  1, XCR0_MASTER_SSE,                   YES, FALSE) \
FLAG(   D, EAX, COMMON,  2,  1, XCR0_MASTER_YMM_H,                 YES, FALSE) \
FIELD(  D, EAX, COMMON,  3, 29, XCR0_MASTER_LOWER,                 NO,  FALSE) \
FIELD(  D, EBX, COMMON,  0, 32, XSAVE_ENABLED_SIZE,                ANY, FALSE) \
FIELD(  D, ECX, COMMON,  0, 32, XSAVE_MAX_SIZE,                    YES, FALSE) \
FIELD(  D, EDX, COMMON,  0, 32, XCR0_MASTER_UPPER,                 NO,  FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,                   MON SUPP, CPL3 */
#define CPUID_FIELD_DATA_LEVEL_400                                             \
FIELD(400, EAX, COMMON,  0, 32, NUM_HYP_LEVELS,                    NA,  FALSE) \
FIELD(400, EBX, COMMON,  0, 32, HYPERVISOR1,                       NA,  FALSE) \
FIELD(400, ECX, COMMON,  0, 32, HYPERVISOR2,                       NA,  FALSE) \
FIELD(400, EDX, COMMON,  0, 32, HYPERVISOR3,                       NA,  FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,                   MON SUPP, CPL3 */
#define CPUID_FIELD_DATA_LEVEL_410                                             \
FIELD(410, EAX, COMMON,  0, 32, TSC_HZ,                            NA,  FALSE) \
FIELD(410, EBX, COMMON,  0, 32, ACPIBUS_HZ,                        NA,  FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,                   MON SUPP, CPL3 */
#define CPUID_FIELD_DATA_LEVEL_80                                              \
FIELD( 80, EAX, COMMON,  0, 32, NUM_EXT_LEVELS,                    NA,  FALSE) \
FIELD( 80, EBX, AMD,     0, 32, LEAF80_VENDOR1,                    NA,  FALSE) \
FIELD( 80, ECX, AMD,     0, 32, LEAF80_VENDOR3,                    NA,  FALSE) \
FIELD( 80, EDX, AMD,     0, 32, LEAF80_VENDOR2,                    NA,  FALSE)
                                                        
/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,                   MON SUPP, CPL3 */
#define CPUID_FIELD_DATA_LEVEL_81                                              \
FIELD( 81, EAX, INTEL,   0, 32, UNKNOWN81EAX,                      ANY, FALSE) \
FIELD( 81, EAX, AMD,     0,  4, LEAF81_STEPPING,                   ANY, FALSE) \
FIELD( 81, EAX, AMD,     4,  4, LEAF81_MODEL,                      ANY, FALSE) \
FIELD( 81, EAX, AMD,     8,  4, LEAF81_FAMILY,                     ANY, FALSE) \
FIELD( 81, EAX, AMD,    12,  2, LEAF81_TYPE,                       ANY, FALSE) \
FIELD( 81, EAX, AMD,    16,  4, LEAF81_EXTENDED_MODEL,             ANY, FALSE) \
FIELD( 81, EAX, AMD,    20,  8, LEAF81_EXTENDED_FAMILY,            ANY, FALSE) \
FIELD( 81, EBX, INTEL,   0, 32, UNKNOWN81EBX,                      ANY, FALSE) \
FIELD( 81, EBX, AMD,     0, 16, LEAF81_BRAND_ID,                   ANY, FALSE) \
FIELD( 81, EBX, AMD,    16, 16, UNDEF,                             ANY, FALSE) \
FLAG(  81, ECX, COMMON,  0,  1, LAHF64,                            YES, TRUE)  \
FLAG(  81, ECX, AMD,     1,  1, CMPLEGACY,                         NO,  FALSE) \
FLAG(  81, ECX, AMD,     2,  1, SVM,                               YES, FALSE) \
FLAG(  81, ECX, AMD,     3,  1, EXTAPICSPC,                        YES, FALSE) \
FLAG(  81, ECX, AMD,     4,  1, CR8AVAIL,                          NO,  FALSE) \
FLAG(  81, ECX, AMD,     5,  1, ABM,                               YES, TRUE)  \
FLAG(  81, ECX, AMD,     6,  1, SSE4A,                             YES, TRUE)  \
FLAG(  81, ECX, AMD,     7,  1, MISALIGNED_SSE,                    YES, TRUE)  \
FLAG(  81, ECX, AMD,     8,  1, 3DNPREFETCH,                       YES, TRUE)  \
FLAG(  81, ECX, AMD,     9,  1, OSVW,                              ANY, FALSE) \
FLAG(  81, ECX, AMD,    10,  1, IBS,                               NO,  FALSE) \
FLAG(  81, ECX, AMD,    11,  1, XOP,                               YES, TRUE)  \
FLAG(  81, ECX, AMD,    12,  1, SKINIT,                            NO,  FALSE) \
FLAG(  81, ECX, AMD,    13,  1, WATCHDOG,                          NO,  FALSE) \
FLAG(  81, ECX, AMD,    15,  1, LWP,                               NO,  FALSE) \
FLAG(  81, ECX, AMD,    16,  1, FMA4,                              YES, TRUE)  \
FLAG(  81, ECX, AMD,    19,  1, NODEID_MSR,                        NO,  FALSE) \
FLAG(  81, ECX, AMD,    22,  1, TOPOLOGY,                          NO,  FALSE) \
FLAG(  81, EDX, AMD,     0,  1, LEAF81_FPU,                        YES, TRUE)  \
FLAG(  81, EDX, AMD,     1,  1, LEAF81_VME,                        YES, FALSE) \
FLAG(  81, EDX, AMD,     2,  1, LEAF81_DE,                         YES, FALSE) \
FLAG(  81, EDX, AMD,     3,  1, LEAF81_PSE,                        YES, FALSE) \
FLAG(  81, EDX, AMD,     4,  1, LEAF81_TSC,                        YES, TRUE)  \
FLAG(  81, EDX, AMD,     5,  1, LEAF81_MSR,                        YES, FALSE) \
FLAG(  81, EDX, AMD,     6,  1, LEAF81_PAE,                        YES, FALSE) \
FLAG(  81, EDX, AMD,     7,  1, LEAF81_MCE,                        YES, FALSE) \
FLAG(  81, EDX, AMD,     8,  1, LEAF81_CX8,                        YES, TRUE)  \
FLAG(  81, EDX, AMD,     9,  1, LEAF81_APIC,                       ANY, FALSE) \
FLAG(  81, EDX, COMMON, 11,  1, SYSC,                              ANY, TRUE)  \
FLAG(  81, EDX, AMD,    12,  1, LEAF81_MTRR,                       YES, FALSE) \
FLAG(  81, EDX, AMD,    13,  1, LEAF81_PGE,                        YES, FALSE) \
FLAG(  81, EDX, AMD,    14,  1, LEAF81_MCA,                        YES, FALSE) \
FLAG(  81, EDX, AMD,    15,  1, LEAF81_CMOV,                       YES, TRUE)  \
FLAG(  81, EDX, AMD,    16,  1, LEAF81_PAT,                        YES, FALSE) \
FLAG(  81, EDX, AMD,    17,  1, LEAF81_PSE36,                      YES, FALSE) \
FLAG(  81, EDX, COMMON, 20,  1, NX,                                YES, FALSE) \
FLAG(  81, EDX, AMD,    22,  1, MMXEXT,                            YES, TRUE)  \
FLAG(  81, EDX, AMD,    23,  1, LEAF81_MMX,                        YES, TRUE)  \
FLAG(  81, EDX, AMD,    24,  1, LEAF81_FXSR,                       YES, TRUE)  \
FLAG(  81, EDX, AMD,    25,  1, FFXSR,                             YES, FALSE) \
FLAG(  81, EDX, COMMON, 26,  1, PDPE1GB,                           YES, FALSE) \
FLAG(  81, EDX, COMMON, 27,  1, RDTSCP,                            YES, TRUE)  \
FLAG(  81, EDX, COMMON, 29,  1, LM,                                YES, FALSE) \
FLAG(  81, EDX, AMD,    30,  1, 3DNOWPLUS,                         YES, TRUE)  \
FLAG(  81, EDX, AMD,    31,  1, 3DNOW,                             YES, TRUE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,                   MON SUPP, CPL3 */
#define CPUID_FIELD_DATA_LEVEL_8x                                              \
FIELD( 85, EAX, AMD,     0,  8, ITLB_ENTRIES_2M4M_PGS,             NA,  FALSE) \
FIELD( 85, EAX, AMD,     8,  8, ITLB_ASSOC_2M4M_PGS,               NA,  FALSE) \
FIELD( 85, EAX, AMD,    16,  8, DTLB_ENTRIES_2M4M_PGS,             NA,  FALSE) \
FIELD( 85, EAX, AMD,    24,  8, DTLB_ASSOC_2M4M_PGS,               NA,  FALSE) \
FIELD( 85, EBX, AMD,     0,  8, ITLB_ENTRIES_4K_PGS,               NA,  FALSE) \
FIELD( 85, EBX, AMD,     8,  8, ITLB_ASSOC_4K_PGS,                 NA,  FALSE) \
FIELD( 85, EBX, AMD,    16,  8, DTLB_ENTRIES_4K_PGS,               NA,  FALSE) \
FIELD( 85, EBX, AMD,    24,  8, DTLB_ASSOC_4K_PGS,                 NA,  FALSE) \
FIELD( 85, ECX, AMD,     0,  8, L1_DCACHE_LINE_SIZE,               NA,  FALSE) \
FIELD( 85, ECX, AMD,     8,  8, L1_DCACHE_LINES_PER_TAG,           NA,  FALSE) \
FIELD( 85, ECX, AMD,    16,  8, L1_DCACHE_ASSOC,                   NA,  FALSE) \
FIELD( 85, ECX, AMD,    24,  8, L1_DCACHE_SIZE,                    NA,  FALSE) \
FIELD( 85, EDX, AMD,     0,  8, L1_ICACHE_LINE_SIZE,               NA,  FALSE) \
FIELD( 85, EDX, AMD,     8,  8, L1_ICACHE_LINES_PER_TAG,           NA,  FALSE) \
FIELD( 85, EDX, AMD,    16,  8, L1_ICACHE_ASSOC,                   NA,  FALSE) \
FIELD( 85, EDX, AMD,    24,  8, L1_ICACHE_SIZE,                    NA,  FALSE) \
FIELD( 86, EAX, AMD,     0, 12, L2_ITLB_ENTRIES_2M4M_PGS,          NA,  FALSE) \
FIELD( 86, EAX, AMD,    12,  4, L2_ITLB_ASSOC_2M4M_PGS,            NA,  FALSE) \
FIELD( 86, EAX, AMD,    16, 12, L2_DTLB_ENTRIES_2M4M_PGS,          NA,  FALSE) \
FIELD( 86, EAX, AMD,    28,  4, L2_DTLB_ASSOC_2M4M_PGS,            NA,  FALSE) \
FIELD( 86, EBX, AMD,     0, 12, L2_ITLB_ENTRIES_4K_PGS,            NA,  FALSE) \
FIELD( 86, EBX, AMD,    12,  4, L2_ITLB_ASSOC_4K_PGS,              NA,  FALSE) \
FIELD( 86, EBX, AMD,    16, 12, L2_DTLB_ENTRIES_4K_PGS,            NA,  FALSE) \
FIELD( 86, EBX, AMD,    28,  4, L2_DTLB_ASSOC_4K_PGS,              NA,  FALSE) \
FIELD( 86, ECX, AMD,     0,  8, L2CACHE_LINE,                      NA,  FALSE) \
FIELD( 86, ECX, AMD,     8,  4, L2CACHE_LINE_PER_TAG,              NA,  FALSE) \
FIELD( 86, ECX, AMD,    12,  4, L2CACHE_WAYS,                      NA,  FALSE) \
FIELD( 86, ECX, AMD,    16, 16, L2CACHE_SIZE,                      NA,  FALSE) \
FIELD( 86, EDX, AMD,     0,  8, L3CACHE_LINE,                      NA,  FALSE) \
FIELD( 86, EDX, AMD,     8,  4, L3CACHE_LINE_PER_TAG,              NA,  FALSE) \
FIELD( 86, EDX, AMD,    12,  4, L3CACHE_WAYS,                      NA,  FALSE) \
FIELD( 86, EDX, AMD,    18, 14, L3CACHE_SIZE,                      NA,  FALSE) \
FLAG(  87, EDX, AMD,     0,  1, TS,                                NA,  FALSE) \
FLAG(  87, EDX, AMD,     1,  1, FID,                               NA,  FALSE) \
FLAG(  87, EDX, AMD,     2,  1, VID,                               NA,  FALSE) \
FLAG(  87, EDX, AMD,     3,  1, TTP,                               NA,  FALSE) \
FLAG(  87, EDX, AMD,     4,  1, LEAF87_TM,                         NA,  FALSE) \
FLAG(  87, EDX, AMD,     5,  1, STC,                               NA,  FALSE) \
FLAG(  87, EDX, AMD,     6,  1, 100MHZSTEPS,                       NA,  FALSE) \
FLAG(  87, EDX, AMD,     7,  1, HWPSTATE,                          NA,  FALSE) \
FLAG(  87, EDX, COMMON,  8,  1, TSC_INVARIANT,                     NA,  FALSE) \
FLAG(  87, EDX, COMMON,  9,  1, CORE_PERF_BOOST,                   NA,  FALSE) \
FIELD( 88, EAX, COMMON,  0,  8, PHYS_BITS,                         NA,  FALSE) \
FIELD( 88, EAX, COMMON,  8,  8, VIRT_BITS,                         NA,  FALSE) \
FIELD( 88, EAX, COMMON, 16,  8, GUEST_PHYS_ADDR_SZ,                NA,  FALSE) \
FIELD( 88, ECX, AMD,     0,  8, LEAF88_CORE_COUNT,                 NA,  FALSE) \
FIELD( 88, ECX, AMD,    12,  4, APICID_COREID_SIZE,                NA,  FALSE) \
FIELD( 8A, EAX, AMD,     0,  8, SVM_REVISION,                      YES, FALSE) \
FLAG(  8A, EAX, AMD,     8,  1, SVM_HYPERVISOR,                    NO,  FALSE) \
FIELD( 8A, EAX, AMD,     9, 23, SVMEAX_RSVD,                       NO,  FALSE) \
FIELD( 8A, EBX, AMD,     0, 32, SVM_NUM_ASIDS,                     YES, FALSE) \
FIELD( 8A, ECX, AMD,     0, 32, SVMECX_RSVD,                       NO,  FALSE) \
FLAG(  8A, EDX, AMD,     0,  1, SVM_NPT,                           YES, FALSE) \
FLAG(  8A, EDX, AMD,     1,  1, SVM_LBR,                           NO,  FALSE) \
FLAG(  8A, EDX, AMD,     2,  1, SVM_LOCK,                          YES, FALSE) \
FLAG(  8A, EDX, AMD,     3,  1, SVM_NRIP,                          YES, FALSE) \
FLAG(  8A, EDX, AMD,     4,  1, SVM_TSC_RATE_MSR,                  NO,  FALSE) \
FLAG(  8A, EDX, AMD,     5,  1, SVM_VMCB_CLEAN,                    YES, FALSE) \
FLAG(  8A, EDX, AMD,     6,  1, SVM_FLUSH_BY_ASID,                 YES, FALSE) \
FLAG(  8A, EDX, AMD,     7,  1, SVM_DECODE_ASSISTS,                YES, FALSE) \
FIELD( 8A, EDX, AMD,     8,  2, SVMEDX_RSVD0,                      NO,  FALSE) \
FLAG(  8A, EDX, AMD,    10,  1, SVM_PAUSE_FILTER,                  NO,  FALSE) \
FLAG(  8A, EDX, AMD,    11,  1, SVMEDX_RSVD1,                      NO,  FALSE) \
FLAG(  8A, EDX, AMD,    12,  1, SVM_PAUSE_THRESHOLD,               NO,  FALSE) \
FIELD( 8A, EDX, AMD,    13, 19, SVMEDX_RSVD2,                      NO,  FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,                   MON SUPP, CPL3 */
#define CPUID_FIELD_DATA_LEVEL_81x                                             \
FIELD(819, EAX, AMD,     0, 12, L1_ITLB_ENTRIES_1G_PGS,            NA,  FALSE) \
FIELD(819, EAX, AMD,    12,  4, L1_ITLB_ASSOC_1G_PGS,              NA,  FALSE) \
FIELD(819, EAX, AMD,    16, 12, L1_DTLB_ENTRIES_1G_PGS,            NA,  FALSE) \
FIELD(819, EAX, AMD,    28,  4, L1_DTLB_ASSOC_1G_PGS,              NA,  FALSE) \
FIELD(819, EBX, AMD,     0, 12, L2_ITLB_ENTRIES_1G_PGS,            NA,  FALSE) \
FIELD(819, EBX, AMD,    12,  4, L2_ITLB_ASSOC_1G_PGS,              NA,  FALSE) \
FIELD(819, EBX, AMD,    16, 12, L2_DTLB_ENTRIES_1G_PGS,            NA,  FALSE) \
FIELD(819, EBX, AMD,    28,  4, L2_DTLB_ASSOC_1G_PGS,              NA,  FALSE) \
FLAG( 81A, EAX, AMD,     0,  1, FP128,                             NA,  FALSE) \
FLAG( 81A, EAX, AMD,     1,  1, MOVU,                              NA,  FALSE) \
FLAG( 81B, EAX, AMD,     0,  1, IBS_FFV,                           NA,  FALSE) \
FLAG( 81B, EAX, AMD,     1,  1, IBS_FETCHSAM,                      NA,  FALSE) \
FLAG( 81B, EAX, AMD,     2,  1, IBS_OPSAM,                         NA,  FALSE) \
FLAG( 81B, EAX, AMD,     3,  1, RW_OPCOUNT,                        NA,  FALSE) \
FLAG( 81B, EAX, AMD,     4,  1, OPCOUNT,                           NA,  FALSE) \
FLAG( 81B, EAX, AMD,     5,  1, BRANCH_TARGET_ADDR,                NA,  FALSE) \
FLAG( 81B, EAX, AMD,     6,  1, OPCOUNT_EXT,                       NA,  FALSE) \
FLAG( 81B, EAX, AMD,     7,  1, RIP_INVALID_CHECK,                 NA,  FALSE) \
FLAG( 81C, EAX, AMD,     0,  1, LWP_AVAIL,                         NA,  FALSE) \
FLAG( 81C, EAX, AMD,     1,  1, LWP_VAL_AVAIL,                     NA,  FALSE) \
FLAG( 81C, EAX, AMD,     2,  1, LWP_IRE_AVAIL,                     NA,  FALSE) \
FLAG( 81C, EAX, AMD,     3,  1, LWP_BRE_AVAIL,                     NA,  FALSE) \
FLAG( 81C, EAX, AMD,     4,  1, LWP_DME_AVAIL,                     NA,  FALSE) \
FLAG( 81C, EAX, AMD,     5,  1, LWP_CNH_AVAIL,                     NA,  FALSE) \
FLAG( 81C, EAX, AMD,     6,  1, LWP_RNH_AVAIL,                     NA,  FALSE) \
FLAG( 81C, EAX, AMD,    31,  1, LWP_INT_AVAIL,                     NA,  FALSE) \
FIELD(81C, EBX, AMD,     0,  8, LWP_CB_SIZE,                       NA,  FALSE) \
FIELD(81C, EBX, AMD,     8,  8, LWP_EVENT_SIZE,                    NA,  FALSE) \
FIELD(81C, EBX, AMD,    16,  8, LWP_MAX_EVENTS,                    NA,  FALSE) \
FIELD(81C, EBX, AMD,    24,  8, LWP_EVENT_OFFSET,                  NA,  FALSE) \
FIELD(81C, ECX, AMD,     0,  4, LWP_LATENCY_MAX,                   NA,  FALSE) \
FLAG( 81C, ECX, AMD,     5,  1, LWP_DATA_ADDR_VALID,               NA,  FALSE) \
FIELD(81C, ECX, AMD,     6,  3, LWP_LATENCY_ROUND,                 NA,  FALSE) \
FIELD(81C, ECX, AMD,     9,  7, LWP_VERSION,                       NA,  FALSE) \
FIELD(81C, ECX, AMD,    16,  8, LWP_MIN_BUF_SIZE,                  NA,  FALSE) \
FLAG( 81C, ECX, AMD,    28,  1, LWP_BRANCH_PRED,                   NA,  FALSE) \
FLAG( 81C, ECX, AMD,    29,  1, LWP_IP_FILTERING,                  NA,  FALSE) \
FLAG( 81C, ECX, AMD,    30,  1, LWP_CACHE_LEVEL,                   NA,  FALSE) \
FLAG( 81C, ECX, AMD,    31,  1, LWP_CACHE_LATENCY,                 NA,  FALSE) \
FLAG( 81C, EDX, AMD,     0,  1, LWP_SUPPORTED,                     NA,  FALSE) \
FLAG( 81C, EDX, AMD,     1,  1, LWP_VAL_SUPPORTED,                 NA,  FALSE) \
FLAG( 81C, EDX, AMD,     2,  1, LWP_IRE_SUPPORTED,                 NA,  FALSE) \
FLAG( 81C, EDX, AMD,     3,  1, LWP_BRE_SUPPORTED,                 NA,  FALSE) \
FLAG( 81C, EDX, AMD,     4,  1, LWP_DME_SUPPORTED,                 NA,  FALSE) \
FLAG( 81C, EDX, AMD,     5,  1, LWP_CNH_SUPPORTED,                 NA,  FALSE) \
FLAG( 81C, EDX, AMD,     6,  1, LWP_RNH_SUPPORTED,                 NA,  FALSE) \
FLAG( 81C, EDX, AMD,    31,  1, LWP_INT_SUPPORTED,                 NA,  FALSE) \
FIELD(81D, EAX, AMD,     0,  5, LEAF81D_CACHE_TYPE,                NA,  FALSE) \
FIELD(81D, EAX, AMD,     5,  3, LEAF81D_CACHE_LEVEL,               NA,  FALSE) \
FLAG( 81D, EAX, AMD,     8,  1, LEAF81D_CACHE_SELF_INIT,           NA,  FALSE) \
FLAG( 81D, EAX, AMD,     9,  1, LEAF81D_CACHE_FULLY_ASSOC,         NA,  FALSE) \
FIELD(81D, EAX, AMD,    14, 12, LEAF81D_NUM_SHARING_CACHE,         NA,  FALSE) \
FIELD(81D, EBX, AMD,     0, 12, LEAF81D_CACHE_LINE_SIZE,           NA,  FALSE) \
FIELD(81D, EBX, AMD,    12, 10, LEAF81D_CACHE_PHYS_PARTITIONS,     NA,  FALSE) \
FIELD(81D, EBX, AMD,    22, 10, LEAF81D_CACHE_WAYS,                NA,  FALSE) \
FIELD(81D, ECX, AMD,     0, 32, LEAF81D_CACHE_NUM_SETS,            NA,  FALSE) \
FLAG( 81D, EDX, AMD,     0,  1, LEAF81D_CACHE_WBINVD,              NA,  FALSE) \
FLAG( 81D, EDX, AMD,     1,  1, LEAF81D_CACHE_INCLUSIVE,           NA,  FALSE) \
FIELD(81E, EAX, AMD,     0, 32, EXTENDED_APICID,                   NA,  FALSE) \
FIELD(81E, EBX, AMD,     0,  8, COMPUTE_UNIT_ID,                   NA,  FALSE) \
FIELD(81E, EBX, AMD,     8,  2, CORES_PER_COMPUTE_UNIT,            NA,  FALSE) \
FIELD(81E, ECX, AMD,     0,  8, NODEID_VAL,                        NA,  FALSE) \
FIELD(81E, ECX, AMD,     8,  3, NODES_PER_PKG,                     NA,  FALSE)

#define INTEL_CPUID_FIELD_DATA

#define AMD_CPUID_FIELD_DATA

#define CPUID_FIELD_DATA                                              \
   CPUID_FIELD_DATA_LEVEL_0                                           \
   CPUID_FIELD_DATA_LEVEL_1                                           \
   CPUID_FIELD_DATA_LEVEL_4                                           \
   CPUID_FIELD_DATA_LEVEL_5                                           \
   CPUID_FIELD_DATA_LEVEL_6                                           \
   CPUID_FIELD_DATA_LEVEL_7                                           \
   CPUID_FIELD_DATA_LEVEL_A                                           \
   CPUID_FIELD_DATA_LEVEL_B                                           \
   CPUID_FIELD_DATA_LEVEL_D                                           \
   CPUID_FIELD_DATA_LEVEL_400                                         \
   CPUID_FIELD_DATA_LEVEL_410                                         \
   CPUID_FIELD_DATA_LEVEL_80                                          \
   CPUID_FIELD_DATA_LEVEL_81                                          \
   CPUID_FIELD_DATA_LEVEL_8x                                          \
   CPUID_FIELD_DATA_LEVEL_81x                                         \
   INTEL_CPUID_FIELD_DATA                                             \
   AMD_CPUID_FIELD_DATA

/*
 * Define all field and flag values as an enum.  The result is a full
 * set of values taken from the table above in the form:
 *
 * CPUID_FEATURE_<vendor>_ID<level><reg>_<name> == mask for feature
 * CPUID_<vendor>_ID<level><reg>_<name>_MASK    == mask for field
 * CPUID_<vendor>_ID<level><reg>_<name>_SHIFT   == offset of field
 *
 * e.g. - CPUID_FEATURE_COMMON_ID1EDX_FPU     = 0x1
 *      - CPUID_COMMON_ID88EAX_VIRT_BITS_MASK  = 0xff00
 *      - CPUID_COMMON_ID88EAX_VIRT_BITS_SHIFT = 8
 *
 * Note: The FEATURE/MASK definitions must use some gymnastics to get
 * around a warning when shifting left by 32.
 */
#define VMW_BIT_MASK(shift)  (((1 << (shift - 1)) << 1) - 1)

#define FIELD(lvl, reg, vend, bitpos, size, name, s, c3)              \
   CPUID_##vend##_ID##lvl##reg##_##name##_SHIFT = bitpos,             \
   CPUID_##vend##_ID##lvl##reg##_##name##_MASK  =                     \
                      VMW_BIT_MASK(size) << bitpos,                   \
   CPUID_FEATURE_##vend##_ID##lvl##reg##_##name =                     \
      CPUID_##vend##_ID##lvl##reg##_##name##_MASK,                    \
   CPUID_INTERNAL_SHIFT_##name  = bitpos,                             \
   CPUID_INTERNAL_MASK_##name   = VMW_BIT_MASK(size) << bitpos,       \
   CPUID_INTERNAL_REG_##name    = CPUID_REG_##reg,                    \
   CPUID_INTERNAL_EAXIN_##name  = CPUID_LEVEL_VAL_##lvl,              \
   CPUID_INTERNAL_ECXIN_##name  = 0,

#define FLAG FIELD

enum {
   /* Define data for every CPUID field we have */
   CPUID_FIELD_DATA
};
#undef VMW_BIT_MASK
#undef FIELD
#undef FLAG

/* Level D subleaf 1 eax XSAVEOPT */
#define CPUID_COMMON_IDDsub1EAX_XSAVEOPT 1
#define CPUID_INTERNAL_MASK_XSAVEOPT  1
#define CPUID_INTERNAL_SHIFT_XSAVEOPT 0
#define CPUID_INTERNAL_REG_XSAVEOPT   CPUID_REG_EAX
#define CPUID_INTERNAL_EAXIN_XSAVEOPT 0xd

/*
 * Legal CPUID config file mask characters.  For a description of the
 * cpuid masking system, please see:
 *
 * http://vmweb.vmware.com/~mts/cgi-bin/view.cgi/Apps/CpuMigrationChecks
 */

#define CPUID_MASK_HIDE_CHR    '0'
#define CPUID_MASK_HIDE_STR    "0"
#define CPUID_MASK_FORCE_CHR   '1'
#define CPUID_MASK_FORCE_STR   "1"
#define CPUID_MASK_PASS_CHR    '-'
#define CPUID_MASK_PASS_STR    "-"
#define CPUID_MASK_TRUE_CHR    'T'
#define CPUID_MASK_TRUE_STR    "T"
#define CPUID_MASK_FALSE_CHR   'F'
#define CPUID_MASK_FALSE_STR   "F"
#define CPUID_MASK_IGNORE_CHR  'X'
#define CPUID_MASK_IGNORE_STR  "X"
#define CPUID_MASK_HOST_CHR    'H'
#define CPUID_MASK_HOST_STR    "H"
#define CPUID_MASK_RSVD_CHR    'R'
#define CPUID_MASK_RSVD_STR    "R"
#define CPUID_MASK_INSTALL_CHR 'I'
#define CPUID_MASK_INSTALL_STR "I"

/*
 * When LM is disabled, we overlay the following masks onto the
 * guest's default masks.  Any level that is not defined below should
 * be treated as all "-"s
 */

#define CPT_ID1ECX_LM_DISABLED  "----:----:----:----:--0-:----:----:----"
#define CPT_ID81EDX_LM_DISABLED "--0-:----:----:----:----:----:----:----"
#define CPT_ID81ECX_LM_DISABLED "----:----:----:----:----:----:----:---0"

#define CPT_GET_LM_DISABLED_MASK(lvl, reg)                                  \
   ((lvl == 1 && reg == CPUID_REG_ECX) ? CPT_ID1ECX_LM_DISABLED :           \
    (lvl == 0x80000001 && reg == CPUID_REG_ECX) ? CPT_ID81ECX_LM_DISABLED : \
    (lvl == 0x80000001 && reg == CPUID_REG_EDX) ? CPT_ID81EDX_LM_DISABLED : \
    NULL)

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

#ifdef __GNUC__

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
CPUIDCheck(uint32 eaxIn, uint32 eaxInCheck,
           CpuidReg reg, CpuidReg regCheck, uint32 val)
{
   ASSERT(eaxIn == eaxInCheck && reg == regCheck);
   return val;
}

#define CPUID_MASK(eaxIn, reg, flag)                                    \
   CPUIDCheck((uint32)eaxIn, CPUID_INTERNAL_EAXIN_##flag,               \
              CPUID_REG_##reg, (CpuidReg)CPUID_INTERNAL_REG_##flag,     \
              CPUID_INTERNAL_MASK_##flag)

#define CPUID_SHIFT(eaxIn, reg, flag)                                   \
   CPUIDCheck((uint32)eaxIn, CPUID_INTERNAL_EAXIN_##flag,               \
              CPUID_REG_##reg, (CpuidReg)CPUID_INTERNAL_REG_##flag,     \
              CPUID_INTERNAL_SHIFT_##flag)

#define CPUID_ISSET(eaxIn, reg, flag, data)                             \
   (CPUIDCheck((uint32)eaxIn, CPUID_INTERNAL_EAXIN_##flag,              \
               CPUID_REG_##reg, (CpuidReg)CPUID_INTERNAL_REG_##flag,    \
               CPUID_INTERNAL_MASK_##flag & (data)) != 0)

#define CPUID_GET(eaxIn, reg, field, data)                              \
   CPUIDCheck((uint32)eaxIn, CPUID_INTERNAL_EAXIN_##field,              \
              CPUID_REG_##reg, (CpuidReg)CPUID_INTERNAL_REG_##field,    \
              ((uint32)(data) & CPUID_INTERNAL_MASK_##field) >>         \
              CPUID_INTERNAL_SHIFT_##field)

#endif


#define CPUID_SET(eaxIn, reg, flag, dataPtr)                            \
   do {                                                                 \
      ASSERT_ON_COMPILE((uint32)eaxIn == (uint32)CPUID_INTERNAL_EAXIN_##flag && \
              CPUID_REG_##reg == (CpuidReg)CPUID_INTERNAL_REG_##flag);  \
      *(dataPtr) |= CPUID_INTERNAL_MASK_##flag;                         \
   } while (0)

#define CPUID_CLEAR(eaxIn, reg, flag, dataPtr)                          \
   do {                                                                 \
      ASSERT_ON_COMPILE((uint32)eaxIn == (uint32)CPUID_INTERNAL_EAXIN_##flag && \
              CPUID_REG_##reg == (CpuidReg)CPUID_INTERNAL_REG_##flag);  \
      *(dataPtr) &= ~CPUID_INTERNAL_MASK_##flag;                        \
   } while (0)

#define CPUID_SETTO(eaxIn, reg, field, dataPtr, val)                    \
   do {                                                                 \
      uint32 _v = val;                                                  \
      uint32 *_d = dataPtr;                                             \
      ASSERT_ON_COMPILE((uint32)eaxIn == (uint32)CPUID_INTERNAL_EAXIN_##field && \
              CPUID_REG_##reg == (CpuidReg)CPUID_INTERNAL_REG_##field); \
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
#define CPUID_FAMILY_5x86             4
#define CPUID_FAMILY_K5               5 
#define CPUID_FAMILY_K6               5
#define CPUID_FAMILY_K7               6
#define CPUID_FAMILY_K8              15
#define CPUID_FAMILY_K8L             16
#define CPUID_FAMILY_K8MOBILE        17
#define CPUID_FAMILY_LLANO           18
#define CPUID_FAMILY_BOBCAT          20
#define CPUID_FAMILY_BULLDOZER       21

/* Effective VIA CPU Families */
#define CPUID_FAMILY_C7               6

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
#define CPUID_MODEL_SANDYBRIDGE_2A 0x2a  // Sandybridge (desktop/mobile)
#define CPUID_MODEL_SANDYBRIDGE_2D 0x2d  // Sandybridge-EP
#define CPUID_MODEL_NEHALEM_2C     0x2c  // Westmere-EP
#define CPUID_MODEL_NEHALEM_2E     0x2e  // Nehalem-EX
#define CPUID_MODEL_NEHALEM_2F     0x2f  // Westmere-EX

#define CPUID_MODEL_PIII_07    7
#define CPUID_MODEL_PIII_08    8
#define CPUID_MODEL_PIII_0A    10

/* AMD model information */
#define CPUID_MODEL_BARCELONA_02 0x02 // Barcelona (both Opteron & Phenom kind)

/* VIA model information */
#define CPUID_MODEL_NANO       15     // Isaiah

/*
 *----------------------------------------------------------------------
 *
 * CPUID_IsVendor{AMD,Intel,VIA} --
 *
 *      Determines if the vendor string in cpuid id0 is from {AMD,Intel,VIA}.
 *
 * Results:
 *      True iff vendor string is CPUID_{AMD,INTEL,VIA}_VENDOR_STRING
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
   return (id0->ebx == *(const uint32 *) (vendor + 0) &&
           id0->ecx == *(const uint32 *) (vendor + 4) &&
           id0->edx == *(const uint32 *) (vendor + 8));   
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
           effectiveModel == CPUID_MODEL_SANDYBRIDGE_2D);
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

static INLINE Bool
CPUID_FAMILY_IS_K8EXT(uint32 eax)
{
   /*
    * We check for this pattern often enough that it's
    * worth a separate function, for syntactic sugar.
    */
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
   return level == 4 || level == 7 || level == 0xb || level == 0xd;
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
#endif
