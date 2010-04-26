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
   CPUIDLEVEL(FALSE, A, 0xA)                    \
   CPUIDLEVEL(FALSE, B, 0xB)                    \
   CPUIDLEVEL(FALSE, 86, 0x80000006)

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
 * FIELDDEF can be defined to process the CPUID information provided
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
 * FIELDDEFA takes a ninth parameter: the name used when generating
 * accessor functions in lib/public/cpuidInfoFuncs.h.
 *
 * FLAGDEF/FLAGDEFA is defined identically to fields, but their
 * accessors are more appropriate for 1-bit flags, and compile-time
 * asserts enforce that the size is 1 bit wide.
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


/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,       MON SUPP, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_0                                               \
FIELDDEF(  0, EAX, COMMON,  0, 32, NUMLEVELS,           ANY, FALSE)     \
FIELDDEF(  0, EBX, COMMON,  0, 32, VENDOR1,             YES, TRUE)      \
FIELDDEF(  0, ECX, COMMON,  0, 32, VENDOR3,             YES, TRUE)      \
FIELDDEF(  0, EDX, COMMON,  0, 32, VENDOR2,             YES, TRUE)
                                                        
/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,       MON SUPP, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_1                                               \
FIELDDEFA( 1, EAX, COMMON,  0,  4, STEPPING,            ANY, FALSE, STEPPING)  \
FIELDDEFA( 1, EAX, COMMON,  4,  4, MODEL,               ANY, FALSE, MODEL)     \
FIELDDEFA( 1, EAX, COMMON,  8,  4, FAMILY,              YES, FALSE, FAMILY)    \
FIELDDEF(  1, EAX, COMMON, 12,  2, TYPE,                ANY, FALSE)            \
FIELDDEFA( 1, EAX, COMMON, 16,  4, EXTMODEL,            ANY, FALSE, EXT_MODEL) \
FIELDDEFA( 1, EAX, COMMON, 20,  8, EXTFAMILY,           YES, FALSE, EXT_FAMILY) \
FIELDDEF(  1, EBX, COMMON,  0,  8, BRAND_ID,            ANY, FALSE)            \
FIELDDEF(  1, EBX, COMMON,  8,  8, CLFL_SIZE,           ANY, FALSE)            \
FIELDDEFA( 1, EBX, COMMON, 16,  8, LCPU_COUNT,          ANY, FALSE, LCPU_COUNT) \
FIELDDEFA( 1, EBX, COMMON, 24,  8, APICID,              ANY, FALSE, APICID)    \
FLAGDEFA(  1, ECX, COMMON, 0,   1, SSE3,                YES, TRUE,  SSE3)      \
FLAGDEFA(  1, ECX, INTEL,  1,   1, PCLMULQDQ,           YES, TRUE, PCLMULQDQ)  \
FLAGDEF(   1, ECX, INTEL,  2,   1, NDA2,                NO,  FALSE)            \
FLAGDEFA(  1, ECX, COMMON, 3,   1, MWAIT,               ANY, FALSE, MWAIT)     \
FLAGDEFA(  1, ECX, INTEL,  4,   1, DSCPL,               NO,  FALSE, DSCPL)     \
FLAGDEFA(  1, ECX, INTEL,  5,   1, VMX,                 YES, FALSE, VMX)       \
FLAGDEF(   1, ECX, VIA,    5,   1, VMX,                 YES, FALSE)            \
FLAGDEF(   1, ECX, INTEL,  6,   1, SMX,                 NO,  FALSE)            \
FLAGDEF(   1, ECX, INTEL,  7,   1, EST,                 NO,  FALSE)            \
FLAGDEF(   1, ECX, INTEL,  8,   1, TM2,                 NO,  FALSE)            \
FLAGDEFA(  1, ECX, COMMON, 9,   1, SSSE3,               YES, TRUE,  SSSE3)     \
FLAGDEF(   1, ECX, INTEL,  10,  1, HTCACHE,             NO,  FALSE)            \
FLAGDEFA(  1, ECX, INTEL,  12,  1, FMA,                 NO,  TRUE,  FMA)       \
FLAGDEFA(  1, ECX, COMMON, 13,  1, CMPX16,              YES, TRUE,  CMPX16)    \
FLAGDEF(   1, ECX, INTEL,  14,  1, xPPR,                NO,  FALSE)            \
FLAGDEF(   1, ECX, INTEL,  15,  1, PERF_MSR,            NO,  FALSE)            \
FLAGDEF(   1, ECX, INTEL,  17,  1, PCID,                NO,  FALSE)            \
FLAGDEF(   1, ECX, INTEL,  18,  1, DCA,                 NO,  FALSE)            \
FLAGDEFA(  1, ECX, INTEL,  19,  1, SSE41,               YES, TRUE,  SSE41)     \
FLAGDEFA(  1, ECX, INTEL,  20,  1, SSE42,               YES, TRUE,  SSE42)     \
FLAGDEF(   1, ECX, INTEL,  21,  1, X2APIC,              NO,  FALSE)            \
FLAGDEFA(  1, ECX, INTEL,  22,  1, MOVBE,               YES, TRUE,  MOVBE)     \
FLAGDEFA(  1, ECX, COMMON, 23,  1, POPCNT,              YES, TRUE,  POPCNT)    \
FLAGDEF(   1, ECX, INTEL,  24,  1, ULE,                 NO,  TRUE)             \
FLAGDEFA(  1, ECX, INTEL,  25,  1, AES,                 YES, TRUE, AES)        \
FLAGDEF(   1, ECX, INTEL,  26,  1, XSAVE,               NO,  FALSE)            \
FLAGDEF(   1, ECX, INTEL,  27,  1, OSXSAVE,             NO,  TRUE)             \
FLAGDEFA(  1, ECX, INTEL,  28,  1, AVX,                 NO,  TRUE,  AVX)       \
FLAGDEFA(  1, ECX, COMMON, 31,  1, HYPERVISOR,          ANY, FALSE, HYPERVISOR)\
FLAGDEFA(  1, EDX, COMMON, 0,   1, FPU,                 YES, TRUE, FPU)        \
FLAGDEFA(  1, EDX, COMMON, 1,   1, VME,                 YES, FALSE, VME)       \
FLAGDEF(   1, EDX, COMMON, 2,   1, DBGE,                YES, FALSE)            \
FLAGDEF(   1, EDX, COMMON, 3,   1, PGSZE,               YES, FALSE)            \
FLAGDEFA(  1, EDX, COMMON, 4,   1, TSC,                 YES, TRUE, TSC)        \
FLAGDEF(   1, EDX, COMMON, 5,   1, MSR,                 YES, FALSE)            \
FLAGDEFA(  1, EDX, COMMON, 6,   1, PAE,                 YES, FALSE, PAE)       \
FLAGDEF(   1, EDX, COMMON, 7,   1, MCK,                 YES, FALSE)            \
FLAGDEF(   1, EDX, COMMON, 8,   1, CPMX,                YES, TRUE)             \
FLAGDEFA(  1, EDX, COMMON, 9,   1, APIC,                ANY, FALSE, APIC)      \
FLAGDEFA(  1, EDX, COMMON, 11,  1, SEP,                 YES, TRUE,  SEP)       \
FLAGDEFA(  1, EDX, COMMON, 12,  1, MTRR,                YES, FALSE, MTRR)      \
FLAGDEFA(  1, EDX, COMMON, 13,  1, PGE,                 YES, FALSE, PGE)       \
FLAGDEFA(  1, EDX, COMMON, 14,  1, MCA,                 YES, FALSE, MCA)       \
FLAGDEFA(  1, EDX, COMMON, 15,  1, CMOV,                YES, TRUE,  CMOV)      \
FLAGDEFA(  1, EDX, COMMON, 16,  1, PAT,                 YES, FALSE, PAT)       \
FLAGDEF(   1, EDX, COMMON, 17,  1, 36PG,                YES, FALSE)            \
FLAGDEF(   1, EDX, INTEL,  18,  1, PSN,                 YES, FALSE)            \
FLAGDEFA(  1, EDX, COMMON, 19,  1, CLFL,                YES, TRUE,  CLFL)      \
FLAGDEF(   1, EDX, INTEL,  21,  1, DTES,                YES, FALSE)            \
FLAGDEF(   1, EDX, INTEL,  22,  1, ACPI,                YES, FALSE)            \
FLAGDEFA(  1, EDX, COMMON, 23,  1, MMX,                 YES, TRUE,  MMX)       \
FLAGDEFA(  1, EDX, COMMON, 24,  1, FXSAVE,              YES, TRUE,  FXSAVE)    \
FLAGDEFA(  1, EDX, COMMON, 25,  1, SSE,                 YES, TRUE,  SSE)       \
FLAGDEFA(  1, EDX, COMMON, 26,  1, SSE2,                YES, TRUE,  SSE2)      \
FLAGDEF(   1, EDX, INTEL,  27,  1, SS,                  YES, FALSE)            \
FLAGDEFA(  1, EDX, COMMON, 28,  1, HT,                  ANY, FALSE, HT)        \
FLAGDEF(   1, EDX, INTEL,  29,  1, TM,                  NO,  FALSE)            \
FLAGDEF(   1, EDX, INTEL,  30,  1, IA64,                NO,  FALSE)            \
FLAGDEF(   1, EDX, INTEL,  31,  1, PBE,                 NO,  FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,       MON SUPP, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_4                                               \
FIELDDEF(  4, EAX, INTEL,   0,  5, CACHE_TYPE,          NA,  FALSE)            \
FIELDDEF(  4, EAX, INTEL,   5,  3, CACHE_LEVEL,         NA,  FALSE)            \
FLAGDEF(   4, EAX, INTEL,   8,  1, CACHE_SELF_INIT,     NA,  FALSE)            \
FLAGDEF(   4, EAX, INTEL,   9,  1, CACHE_FULLY_ASSOC,   NA,  FALSE)            \
FIELDDEF(  4, EAX, INTEL,  14, 12, CACHE_NUMHT_SHARING, NA,  FALSE)            \
FIELDDEFA( 4, EAX, INTEL,  26,  6, CORE_COUNT,          NA,  FALSE, INTEL_CORE_COUNT)  \
FIELDDEF(  4, EBX, INTEL,   0, 12, CACHE_LINE,          NA,  FALSE)            \
FIELDDEF(  4, EBX, INTEL,  12, 10, CACHE_PART,          NA,  FALSE)            \
FIELDDEF(  4, EBX, INTEL,  22, 10, CACHE_WAYS,          NA,  FALSE)            \
FIELDDEF(  4, ECX, INTEL,   0, 32, CACHE_SETS,          NA,  FALSE)            \
FLAGDEF(   4, EDX, INTEL,   0,  1, CACHE_WBINVD_NOT_GUARANTEED, NA,  FALSE)    \
FLAGDEF(   4, EDX, INTEL,   1,  1, CACHE_IS_INCLUSIVE,  NA,  FALSE)

/*     LEVEL, REG, VENDOR, POS, SIZE, NAME,       MON SUPP, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_5                                           \
FIELDDEFA( 5, EAX, COMMON,  0, 16, MWAIT_MIN_SIZE,      NA,  FALSE, MWAIT_MIN_SIZE) \
FIELDDEFA( 5, EBX, COMMON,  0, 16, MWAIT_MAX_SIZE,      NA,  FALSE, MWAIT_MAX_SIZE) \
FLAGDEF(   5, ECX, COMMON,  0,  1, MWAIT_EXTENSIONS,    NA,  FALSE) \
FLAGDEFA(  5, ECX, COMMON,  1,  1, MWAIT_INTR_BREAK,    NA,  FALSE, MWAIT_INTR_BREAK) \
FIELDDEF(  5, EDX, INTEL,   0,  4, MWAIT_C0_SUBSTATE,   NA,  FALSE) \
FIELDDEF(  5, EDX, INTEL,   4,  4, MWAIT_C1_SUBSTATE,   NA,  FALSE) \
FIELDDEF(  5, EDX, INTEL,   8,  4, MWAIT_C2_SUBSTATE,   NA,  FALSE) \
FIELDDEF(  5, EDX, INTEL,  12,  4, MWAIT_C3_SUBSTATE,   NA,  FALSE) \
FIELDDEF(  5, EDX, INTEL,  16,  4, MWAIT_C4_SUBSTATE,   NA,  FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,       MON SUPP, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_6                                               \
FLAGDEF(   6, EAX, INTEL,   0,  1, THERMAL_SENSOR,      NA,  FALSE)     \
FLAGDEF(   6, EAX, INTEL,   1,  1, TURBO_MODE,          NA,  FALSE)     \
FIELDDEF(  6, EBX, INTEL,   0,  4, NUM_INTR_THRESHOLDS, NA,  FALSE)     \
FLAGDEF(   6, ECX, INTEL,   0,  1, HW_COORD_FEEDBACK,   NA,  FALSE)	\
FLAGDEF(   6, ECX, INTEL,   3,  1, ENERGY_PERF_BIAS,    NA,  FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,       MON SUPP, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_A                                               \
FIELDDEFA( A, EAX, INTEL,   0,  8, PMC_VERSION,         NA,  FALSE, PMC_VERSION) \
FIELDDEFA( A, EAX, INTEL,   8,  8, NUM_PMCS,            NA,  FALSE, NUM_PMCS)  \
FIELDDEF(  A, EAX, INTEL,  16,  8, PMC_BIT_WIDTH,       NA,  FALSE)            \
FIELDDEFA( A, EAX, INTEL,  24,  8, PMC_EBX_LENGTH,      NA,  FALSE, PMC_EBX_LENGTH) \
FLAGDEF(   A, EBX, INTEL,   0,  1, PMC_CORE_CYCLE,      NA,  FALSE)            \
FLAGDEF(   A, EBX, INTEL,   1,  1, PMC_INSTR_RETIRED,   NA,  FALSE)            \
FLAGDEF(   A, EBX, INTEL,   2,  1, PMC_REF_CYCLES,      NA,  FALSE)            \
FLAGDEF(   A, EBX, INTEL,   3,  1, PMC_LAST_LVL_CREF,   NA,  FALSE)            \
FLAGDEF(   A, EBX, INTEL,   4,  1, PMC_LAST_LVL_CMISS,  NA,  FALSE)            \
FLAGDEF(   A, EBX, INTEL,   5,  1, PMC_BR_INST_RETIRED, NA,  FALSE)            \
FLAGDEF(   A, EBX, INTEL,   6,  1, PMC_BR_MISS_RETIRED, NA,  FALSE)            \
FIELDDEF(  A, EDX, INTEL,   0,  5, PMC_FIXED_NUM,       NA,  FALSE)            \
FIELDDEF(  A, EDX, INTEL,   5,  8, PMC_FIXED_SIZE,      NA,  FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,       MON SUPP, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_B                                               \
FIELDDEF(  B, EAX, INTEL,   0,  5, MASK_WIDTH,          NA,  FALSE)            \
FIELDDEF(  B, EBX, INTEL,   0, 16, CPUS_SHARING_LEVEL,  NA,  FALSE)            \
FIELDDEF(  B, ECX, INTEL,   0,  8, LEVEL_NUMBER,        NA,  FALSE)            \
FIELDDEF(  B, ECX, INTEL,   8,  8, LEVEL_TYPE,          NA,  FALSE)            \
FIELDDEF(  B, EDX, INTEL,   0, 32, X2APIC_ID,           NA,  FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,       MON SUPP, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_400                                             \
FIELDDEF(400, EAX, COMMON,  0, 32, NUM_HYP_LEVELS,      NA,  FALSE)            \
FIELDDEF(400, EBX, COMMON,  0, 32, HYPERVISOR1,         NA,  FALSE)            \
FIELDDEF(400, ECX, COMMON,  0, 32, HYPERVISOR2,         NA,  FALSE)            \
FIELDDEF(400, EDX, COMMON,  0, 32, HYPERVISOR3,         NA,  FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,       MON SUPP, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_410                                             \
FIELDDEF(410, EAX, COMMON,  0, 32, TSC_HZ,              NA,  FALSE)            \
FIELDDEF(410, EBX, COMMON,  0, 32, ACPIBUS_HZ,          NA,  FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,       MON SUPP, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_80                                              \
FIELDDEF( 80, EAX, COMMON,  0, 32, NUM_EXT_LEVELS,      NA,  FALSE)            \
FIELDDEF( 80, EBX, AMD,     0, 32, AMD_VENDOR1,         NA,  FALSE)            \
FIELDDEF( 80, ECX, AMD,     0, 32, AMD_VENDOR3,         NA,  FALSE)            \
FIELDDEF( 80, EDX, AMD,     0, 32, AMD_VENDOR2,         NA,  FALSE)
                                                        
/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,       MON SUPP, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_81                                              \
FIELDDEF( 81, EAX, INTEL,   0, 32, UNKNOWN81EAX,        ANY, FALSE)            \
FIELDDEF( 81, EAX, AMD,     0,  4, STEPPING,            ANY, FALSE)            \
FIELDDEF( 81, EAX, AMD,     4,  4, MODEL,               ANY, FALSE)            \
FIELDDEF( 81, EAX, AMD,     8,  4, FAMILY,              ANY, FALSE)            \
FIELDDEF( 81, EAX, AMD,    12,  2, TYPE,                ANY, FALSE)            \
FIELDDEF( 81, EAX, AMD,    16,  4, EXTMODEL,            ANY, FALSE)            \
FIELDDEF( 81, EAX, AMD,    20,  8, EXTFAMILY,           ANY, FALSE)            \
FIELDDEF( 81, EBX, INTEL,   0, 32, UNKNOWN81EBX,        ANY, FALSE)            \
FIELDDEF( 81, EBX, AMD,     0, 16, BRAND_ID,            ANY, FALSE)            \
FIELDDEF( 81, EBX, AMD,    16, 16, UNDEF,               ANY, FALSE)            \
FLAGDEFA( 81, ECX, COMMON,  0,  1, LAHF,                YES, TRUE,  LAHF64)    \
FLAGDEFA( 81, ECX, AMD,     1,  1, CMPLEGACY,           NO,  FALSE, CMPLEGACY) \
FLAGDEFA( 81, ECX, AMD,     2,  1, SVM,                 YES, FALSE, SVM)       \
FLAGDEFA( 81, ECX, AMD,     3,  1, EXTAPICSPC,          YES, FALSE, EXTAPICSPC) \
FLAGDEFA( 81, ECX, AMD,     4,  1, CR8AVAIL,            NO,  FALSE, CR8AVAIL)  \
FLAGDEFA( 81, ECX, AMD,     5,  1, ABM,                 YES, TRUE,  ABM)       \
FLAGDEFA( 81, ECX, AMD,     6,  1, SSE4A,               YES, TRUE,  SSE4A)     \
FLAGDEF(  81, ECX, AMD,     7,  1, MISALIGNED_SSE,      YES, TRUE)             \
FLAGDEFA( 81, ECX, AMD,     8,  1, 3DNPREFETCH,         YES, TRUE, 3DNPREFETCH) \
FLAGDEF(  81, ECX, AMD,     9,  1, OSVW,                NO,  FALSE)            \
FLAGDEF(  81, ECX, AMD,    10,  1, IBS,                 NO,  FALSE)            \
FLAGDEF(  81, ECX, AMD,    11,  1, SSE5,                NO,  TRUE)             \
FLAGDEF(  81, ECX, AMD,    12,  1, SKINIT,              NO,  FALSE)            \
FLAGDEF(  81, ECX, AMD,    13,  1, WATCHDOG,            NO,  FALSE)            \
FLAGDEF(  81, EDX, AMD,     0,  1, FPU,                 YES, TRUE)             \
FLAGDEF(  81, EDX, AMD,     1,  1, VME,                 YES, FALSE)            \
FLAGDEF(  81, EDX, AMD,     2,  1, DBGE,                YES, FALSE)            \
FLAGDEF(  81, EDX, AMD,     3,  1, PGSZE,               YES, FALSE)            \
FLAGDEF(  81, EDX, AMD,     4,  1, TSC,                 YES, TRUE)             \
FLAGDEF(  81, EDX, AMD,     5,  1, MSR,                 YES, FALSE)            \
FLAGDEF(  81, EDX, AMD,     6,  1, PAE,                 YES, FALSE)            \
FLAGDEF(  81, EDX, AMD,     7,  1, MCK,                 YES, FALSE)            \
FLAGDEF(  81, EDX, AMD,     8,  1, CPMX,                YES, TRUE)             \
FLAGDEF(  81, EDX, AMD,     9,  1, APIC,                ANY, FALSE)            \
FLAGDEFA( 81, EDX, COMMON, 11,  1, SYSC,                ANY, TRUE, SYSC)       \
FLAGDEF(  81, EDX, AMD,    12,  1, MTRR,                YES, FALSE)            \
FLAGDEF(  81, EDX, AMD,    13,  1, PGE,                 YES, FALSE)            \
FLAGDEF(  81, EDX, AMD,    14,  1, MCA,                 YES, FALSE)            \
FLAGDEF(  81, EDX, AMD,    15,  1, CMOV,                YES, TRUE)             \
FLAGDEF(  81, EDX, AMD,    16,  1, PAT,                 YES, FALSE)            \
FLAGDEF(  81, EDX, AMD,    17,  1, 36PG,                YES, FALSE)            \
FLAGDEFA( 81, EDX, COMMON, 20,  1, NX,                  YES, FALSE, NX)        \
FLAGDEFA( 81, EDX, AMD,    22,  1, MMXEXT,              YES, TRUE,  MMXEXT)    \
FLAGDEF(  81, EDX, AMD,    23,  1, MMX,                 YES, TRUE)             \
FLAGDEF(  81, EDX, AMD,    24,  1, FXSAVE,              YES, TRUE)             \
FLAGDEFA( 81, EDX, AMD,    25,  1, FFXSR,               YES, FALSE, FFXSR)     \
FLAGDEF(  81, EDX, COMMON, 26,  1, PDPE1GB,             NO,  FALSE)            \
FLAGDEFA( 81, EDX, COMMON, 27,  1, RDTSCP,              YES, TRUE,  RDTSCP)    \
FLAGDEFA( 81, EDX, COMMON, 29,  1, LM,                  YES, FALSE, LM) \
FLAGDEFA( 81, EDX, AMD,    30,  1, 3DNOWPLUS,           YES, TRUE,  3DNOWPLUS) \
FLAGDEFA( 81, EDX, AMD,    31,  1, 3DNOW,               YES, TRUE,  3DNOW)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,       MON SUPP, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_8x                                              \
FIELDDEF( 86, ECX, AMD,     0,  8, L2CACHE_LINE,        NA,  FALSE)            \
FIELDDEF( 86, ECX, AMD,     8,  4, L2CACHE_LINE_PER_TAG, NA, FALSE)            \
FIELDDEF( 86, ECX, AMD,    12,  4, L2CACHE_WAYS,        NA,  FALSE)            \
FIELDDEF( 86, ECX, AMD,    16, 16, L2CACHE_SIZE,        NA,  FALSE)            \
FIELDDEF( 86, EDX, AMD,     0,  8, L3CACHE_LINE,        NA,  FALSE)            \
FIELDDEF( 86, EDX, AMD,     8,  4, L3CACHE_LINE_PER_TAG,NA,  FALSE)            \
FIELDDEF( 86, EDX, AMD,    12,  4, L3CACHE_WAYS,        NA,  FALSE)            \
FIELDDEF( 86, EDX, AMD,    18, 14, L3CACHE_SIZE,        NA,  FALSE)            \
FLAGDEF(  87, EDX, AMD,     0,  1, TS,                  NA,  FALSE)            \
FLAGDEF(  87, EDX, AMD,     1,  1, FID,                 NA,  FALSE)            \
FLAGDEF(  87, EDX, AMD,     2,  1, VID,                 NA,  FALSE)            \
FLAGDEF(  87, EDX, AMD,     3,  1, TTP,                 NA,  FALSE)            \
FLAGDEF(  87, EDX, AMD,     4,  1, TM,                  NA,  FALSE)            \
FLAGDEF(  87, EDX, AMD,     5,  1, STC,                 NA,  FALSE)            \
FLAGDEF(  87, EDX, AMD,     6,  1, 100MHZSTEPS,         NA,  FALSE)            \
FLAGDEF(  87, EDX, AMD,     7,  1, HWPSTATE,            NA,  FALSE)            \
FLAGDEF(  87, EDX, COMMON,  8,  1, TSC_INVARIANT,       NA,  FALSE)            \
FIELDDEFA(88, EAX, COMMON,  0,  8, PHYSBITS,            NA,  FALSE, PHYS_BITS) \
FIELDDEFA(88, EAX, COMMON,  8,  8, VIRTBITS,            NA,  FALSE, VIRT_BITS) \
FIELDDEFA(88, ECX, AMD,     0,  8, CORE_COUNT,          NA,  FALSE, AMD_CORE_COUNT) \
FIELDDEFA(88, ECX, AMD,    12,  4, APICID_COREID_SIZE,  NA,  FALSE, AMD_APICID_COREID_SIZE) \
FIELDDEFA(8A, EAX, AMD,     0,  8, SVM_REVISION,        YES, FALSE, SVM_REVISION) \
FLAGDEF(  8A, EAX, AMD,     8,  1, SVM_HYPERVISOR,      NO,  FALSE)            \
FIELDDEF( 8A, EAX, AMD,     9, 23, SVMEAX_RSVD,         NO,  FALSE)            \
FIELDDEF( 8A, EBX, AMD,     0, 32, SVM_N_ASIDS,         ANY, FALSE)            \
FIELDDEF( 8A, ECX, AMD,     0, 32, SVMECX_RSVD,         NO,  FALSE)            \
FLAGDEFA( 8A, EDX, AMD,     0,  1, SVM_NP,              YES, FALSE, NPT)       \
FLAGDEF(  8A, EDX, AMD,     1,  1, SVM_LBR,             NO,  FALSE)            \
FLAGDEF(  8A, EDX, AMD,     2,  1, SVM_LOCK,            NO,  FALSE)            \
FLAGDEF(  8A, EDX, AMD,     3,  1, SVM_NRIP,            NO,  FALSE)            \
FLAGDEFA( 8A, EDX, AMD,    10,  1, SVM_PAUSE_FILTER,    NO,  FALSE, PAUSE_FILTER)



#define CPUID_FIELD_DATA_LEVEL_8A_BD                                           \
FIELDDEF( 8A, EDX, AMD,     4,  6, SVMEDX_RSVD0,        NO,  FALSE)            \
FIELDDEF( 8A, EDX, AMD,    11, 21, SVMEDX_RSVD1,        NO,  FALSE)




#define CPUID_FIELD_DATA                                              \
   CPUID_FIELD_DATA_LEVEL_0                                           \
   CPUID_FIELD_DATA_LEVEL_1                                           \
   CPUID_FIELD_DATA_LEVEL_4                                           \
   CPUID_FIELD_DATA_LEVEL_5                                           \
   CPUID_FIELD_DATA_LEVEL_6                                           \
   CPUID_FIELD_DATA_LEVEL_A                                           \
   CPUID_FIELD_DATA_LEVEL_B                                           \
   CPUID_FIELD_DATA_LEVEL_400                                         \
   CPUID_FIELD_DATA_LEVEL_410                                         \
   CPUID_FIELD_DATA_LEVEL_80                                          \
   CPUID_FIELD_DATA_LEVEL_81                                          \
   CPUID_FIELD_DATA_LEVEL_8x                                          \
   CPUID_FIELD_DATA_LEVEL_8A_BD


/*
 * Define all field and flag values as an enum.  The result is a full
 * set of values taken from the table above in the form:
 *
 * CPUID_FEATURE_<vendor>_ID<level><reg>_<name> == mask for feature
 * CPUID_<vendor>_ID<level><reg>_<name>_MASK    == mask for field
 * CPUID_<vendor>_ID<level><reg>_<name>_SHIFT   == offset of field
 *
 * e.g. - CPUID_FEATURE_COMMON_ID1EDX_FPU     = 0x1
 *      - CPUID_COMMON_ID88EAX_VIRTBITS_MASK  = 0xff00
 *      - CPUID_COMMON_ID88EAX_VIRTBITS_SHIFT = 8
 *
 * Note: The FEATURE/MASK definitions must use some gymnastics to get
 * around a warning when shifting left by 32.
 */
#define VMW_BIT_MASK(shift)  (((1 << (shift - 1)) << 1) - 1)

#define FIELDDEF(lvl, reg, vend, bitpos, size, name, s, c3)             \
   CPUID_##vend##_ID##lvl##reg##_##name##_SHIFT = bitpos,               \
   CPUID_##vend##_ID##lvl##reg##_##name##_MASK  =                       \
                      VMW_BIT_MASK(size) << bitpos,                     \
   CPUID_FEATURE_##vend##_ID##lvl##reg##_##name =                       \
                      CPUID_##vend##_ID##lvl##reg##_##name##_MASK,

/* Before simplifying this take a look at bug 293638... */
#define FIELDDEFA(lvl, reg, vend, bitpos, size, name, s, c3, f)         \
   CPUID_##vend##_ID##lvl##reg##_##name##_SHIFT = bitpos,               \
   CPUID_##vend##_ID##lvl##reg##_##name##_MASK  =                       \
                      VMW_BIT_MASK(size) << bitpos,                     \
   CPUID_FEATURE_##vend##_ID##lvl##reg##_##name =                       \
                      CPUID_##vend##_ID##lvl##reg##_##name##_MASK,

#define FLAGDEFA FIELDDEFA
#define FLAGDEF FIELDDEF

enum {
   /* Define data for every CPUID field we have */
   CPUID_FIELD_DATA
};
#undef VMW_BIT_MASK
#undef FIELDDEF
#undef FLAGDEF
#undef FIELDDEFA
#undef FLAGDEFA

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
 * Macro to define GET and SET functions for various common CPUID
 * fields.  To create function for a new field, simply name it (CPUID_
 * and CPUID_SET_ are automatically prepended), and list the field
 * name that it needs to use.
 */

#define FIELD_FUNC(name, field)                                 \
   static INLINE uint32 CPUID_##name(uint32 reg)                \
   {                                                            \
      return (reg & field##_MASK) >> field##_SHIFT;             \
   }                                                            \
   static INLINE void CPUID_SET_##name(uint32 *reg, uint32 val) \
   {                                                            \
      *reg = (*reg & ~field##_MASK) | (val << field##_SHIFT);   \
   }

FIELD_FUNC(STEPPING,         CPUID_COMMON_ID1EAX_STEPPING)
FIELD_FUNC(MODEL,            CPUID_COMMON_ID1EAX_MODEL)
FIELD_FUNC(FAMILY,           CPUID_COMMON_ID1EAX_FAMILY)
FIELD_FUNC(TYPE,             CPUID_COMMON_ID1EAX_TYPE)
FIELD_FUNC(EXTENDED_MODEL,   CPUID_COMMON_ID1EAX_EXTMODEL)
FIELD_FUNC(EXTENDED_FAMILY,  CPUID_COMMON_ID1EAX_EXTFAMILY)
FIELD_FUNC(LCPU_COUNT,       CPUID_COMMON_ID1EBX_LCPU_COUNT)
FIELD_FUNC(APICID,           CPUID_COMMON_ID1EBX_APICID)
FIELD_FUNC(PA_BITS,          CPUID_COMMON_ID88EAX_PHYSBITS)
FIELD_FUNC(VIRT_BITS,        CPUID_COMMON_ID88EAX_VIRTBITS)
FIELD_FUNC(SVM_REVISION,     CPUID_AMD_ID8AEAX_SVM_REVISION)
FIELD_FUNC(SVM_N_ASIDS,      CPUID_AMD_ID8AEBX_SVM_N_ASIDS)
FIELD_FUNC(CACHE_TYPE,       CPUID_INTEL_ID4EAX_CACHE_TYPE)
FIELD_FUNC(INTEL_CORE_COUNT, CPUID_INTEL_ID4EAX_CORE_COUNT)
FIELD_FUNC(AMD_CORE_COUNT,   CPUID_AMD_ID88ECX_CORE_COUNT)
FIELD_FUNC(AMD_APICID_COREID_SIZE, CPUID_AMD_ID88ECX_APICID_COREID_SIZE)
FIELD_FUNC(AMD_EXTAPICSPC,   CPUID_AMD_ID81ECX_EXTAPICSPC)
FIELD_FUNC(NUM_PMCS,         CPUID_INTEL_IDAEAX_NUM_PMCS)
FIELD_FUNC(MWAIT_MIN_SIZE,   CPUID_COMMON_ID5EAX_MWAIT_MIN_SIZE)
FIELD_FUNC(MWAIT_MAX_SIZE,   CPUID_COMMON_ID5EBX_MWAIT_MAX_SIZE)
FIELD_FUNC(MWAIT_C0_SUBSTATE, CPUID_INTEL_ID5EDX_MWAIT_C0_SUBSTATE)
FIELD_FUNC(MWAIT_C1_SUBSTATE, CPUID_INTEL_ID5EDX_MWAIT_C1_SUBSTATE)
FIELD_FUNC(MWAIT_C2_SUBSTATE, CPUID_INTEL_ID5EDX_MWAIT_C2_SUBSTATE)
FIELD_FUNC(MWAIT_C3_SUBSTATE, CPUID_INTEL_ID5EDX_MWAIT_C3_SUBSTATE)
FIELD_FUNC(MWAIT_C4_SUBSTATE, CPUID_INTEL_ID5EDX_MWAIT_C4_SUBSTATE)
FIELD_FUNC(TOPOLOGY_MASK_WIDTH,         CPUID_INTEL_IDBEAX_MASK_WIDTH)
FIELD_FUNC(TOPOLOGY_CPUS_SHARING_LEVEL, CPUID_INTEL_IDBEBX_CPUS_SHARING_LEVEL)
FIELD_FUNC(TOPOLOGY_LEVEL_NUMBER,       CPUID_INTEL_IDBECX_LEVEL_NUMBER)
FIELD_FUNC(TOPOLOGY_LEVEL_TYPE,         CPUID_INTEL_IDBECX_LEVEL_TYPE)
FIELD_FUNC(TOPOLOGY_X2APIC_ID,          CPUID_INTEL_IDBEDX_X2APIC_ID)
#undef FIELD_FUNC


/*
 * Definitions of various fields' values and more complicated
 * macros/functions for reading cpuid fields.
 */

#define CPUID_FAMILY_EXTENDED 15

/* Effective Intel CPU Families */
#define CPUID_FAMILY_486      4
#define CPUID_FAMILY_P5       5
#define CPUID_FAMILY_P6       6
#define CPUID_FAMILY_P4       15

/* Effective AMD CPU Families */
#define CPUID_FAMILY_5x86     4
#define CPUID_FAMILY_K5       5
#define CPUID_FAMILY_K6       5
#define CPUID_FAMILY_K7       6
#define CPUID_FAMILY_K8       15
#define CPUID_FAMILY_K8L      16
#define CPUID_FAMILY_K8MOBILE 17
#define CPUID_FAMILY_BULLDOZER 21

/* Effective VIA CPU Families */
#define CPUID_FAMILY_C7       6

/* Intel model information */
#define CPUID_MODEL_PPRO       1
#define CPUID_MODEL_PII_03     3
#define CPUID_MODEL_PII_05     5
#define CPUID_MODEL_CELERON_06 6
#define CPUID_MODEL_PM_09      9
#define CPUID_MODEL_PM_0D      13
#define CPUID_MODEL_PM_0E      14    // Yonah / Sossaman
#define CPUID_MODEL_CORE_0F    15    // Conroe / Merom
#define CPUID_MODEL_CORE_17    0x17  // Penryn
#define CPUID_MODEL_NEHALEM_1A 0x1a  // Nehalem / Gainestown
#define CPUID_MODEL_ATOM_1C    0x1c  // Silverthorne / Diamondville
#define CPUID_MODEL_CORE_1D    0x1d  // Dunnington
#define CPUID_MODEL_NEHALEM_1E 0x1e  // Lynnfield
#define CPUID_MODEL_NEHALEM_1F 0x1f  // Havendale
#define CPUID_MODEL_NEHALEM_25 0x25  // Westmere / Clarkdale
#define CPUID_MODEL_NEHALEM_2C 0x2c  // Westmere-EP
#define CPUID_MODEL_NEHALEM_2E 0x2e  // Nehalem-EX

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
   return CPUID_FAMILY(v) +
      (CPUID_FAMILY(v) == CPUID_FAMILY_EXTENDED ? CPUID_EXTENDED_FAMILY(v) : 0);
}

/* Normally only used when FAMILY==CPUID_FAMILY_EXTENDED, but Intel is
 * now using the extended model field for FAMILY==CPUID_FAMILY_P6 to
 * refer to the newer Core2 CPUs
 */
static INLINE uint32
CPUID_EFFECTIVE_MODEL(uint32 v) /* %eax from CPUID with %eax=1. */
{
   return CPUID_MODEL(v) + (CPUID_EXTENDED_MODEL(v) << 4); 
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
          (
           effectiveModel == CPUID_MODEL_NEHALEM_1A ||
           effectiveModel == CPUID_MODEL_NEHALEM_1E ||
           effectiveModel == CPUID_MODEL_NEHALEM_1F ||
           effectiveModel == CPUID_MODEL_NEHALEM_25 ||
           effectiveModel == CPUID_MODEL_NEHALEM_2C ||
           effectiveModel == CPUID_MODEL_NEHALEM_2E);
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
          CPUID_EXTENDED_MODEL(eax) != 0;
}

static INLINE Bool
CPUID_FAMILY_IS_K8L(uint32 eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_K8L;
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
   return CPUID_FAMILY_IS_K8L(v) &&
          CPUID_EFFECTIVE_MODEL(v) == CPUID_MODEL_BARCELONA_02;
}


#define CPUID_TYPE_PRIMARY     0
#define CPUID_TYPE_OVERDRIVE   1
#define CPUID_TYPE_SECONDARY   2

#define CPUID_INTEL_ID4EAX_CACHE_TYPE_NULL      0
#define CPUID_INTEL_ID4EAX_CACHE_TYPE_DATA      1
#define CPUID_INTEL_ID4EAX_CACHE_TYPE_INST      2
#define CPUID_INTEL_ID4EAX_CACHE_TYPE_UNIF      3

#define CPUID_INTEL_ID4EAX_CACHE_SELF_INIT      0x00000100
#define CPUID_INTEL_ID4EAX_CACHE_FULLY_ASSOC    0x00000200

#define CPUID_INTEL_IDBECX_LEVEL_TYPE_INVALID   0
#define CPUID_INTEL_IDBECX_LEVEL_TYPE_SMT       1
#define CPUID_INTEL_IDBECX_LEVEL_TYPE_CORE      2


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
   return 1 + CPUID_INTEL_CORE_COUNT(v);
}


static INLINE uint32
CPUID_AMDCoresPerPackage(uint32 v) /* %ecx from CPUID with %eax=0x80000008. */
{
   // Note: This is not guaranteed to work on older AMD CPUs.
   return 1 + CPUID_AMD_CORE_COUNT(v);
}


/*
 * Hypervisor CPUID space is 0x400000XX.
 */
static INLINE Bool
CPUID_IsHypervisorLevel(uint32 level, uint32 *offset)
{
   *offset = level & 0xff;
   return (level & 0xffffff00) == 0x40000000;
}


#endif
