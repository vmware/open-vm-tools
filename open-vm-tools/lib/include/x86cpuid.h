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

#ifndef _X86CPUID_H_
#define _X86CPUID_H_

/* http://www.sandpile.org/ia32/cpuid.htm */

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMNIXMOD
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "vm_basic_types.h"

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
 * Results of calling cpuid(eaxArg, ecxArg) on all logical processors. 
 */
#ifdef _MSC_VER
#pragma warning (disable :4200) // non-std extension: zero-sized array in struct
#endif

typedef struct CPUIDResult {
   uint32 numLogicalCPUs;
   uint32 eaxArg, ecxArg;
   CPUIDRegs regs[0];
} CPUIDResult;

/*
 * CPUID levels the monitor caches and ones that are not cached, but
 * have fields defined below (short name and actual value).
 * 
 * The first parameter defines whether the level is masked/tested
 * during power-on/migration.  Any level which is marked as FALSE here
 * *must* have all field masks defined as IGNORE in CPUID_FIELD_DATA.
 * A static assert in lib/cpuidcompat/cpuidcompat.c will check this.
 *
 * IMPORTANT: WHEN ADDING A NEW FIELD TO THE CACHED LEVELS, make sure
 * you update vmcore/vmm/cpu/priv.c:Priv_CPUID() and vmcore/vmm64/bt/
 * cpuid_shared.S (and geninfo) to include the new level.
 */

#define CPUID_CACHED_LEVELS                     \
   CPUIDLEVEL(TRUE,  0,  0)                     \
   CPUIDLEVEL(TRUE,  1,  1)                     \
   CPUIDLEVEL(FALSE,400, 0x40000000)            \
   CPUIDLEVEL(FALSE,410, 0x40000010)            \
   CPUIDLEVEL(FALSE, 80, 0x80000000)            \
   CPUIDLEVEL(TRUE,  81, 0x80000001)            \
   CPUIDLEVEL(FALSE, 88, 0x80000008)            \
   CPUIDLEVEL(TRUE,  8A, 0x8000000A)

#define CPUID_UNCACHED_LEVELS                   \
   CPUIDLEVEL(FALSE, 4, 4)                      \
   CPUIDLEVEL(FALSE, 5, 5)                      \
   CPUIDLEVEL(FALSE, 6, 6)                      \
   CPUIDLEVEL(FALSE, A, 0xA)                    \
   CPUIDLEVEL(FALSE, 86, 0x80000006)            \
   CPUIDLEVEL(FALSE, 87, 0x80000007)            \

#define CPUID_ALL_LEVELS                        \
   CPUID_CACHED_LEVELS                          \
   CPUID_UNCACHED_LEVELS

/* Define cached CPUID levels in the form: CPUID_LEVEL_<ShortName> */
typedef enum {
#define CPUIDLEVEL(t, s, v) CPUID_LEVEL_##s,
   CPUID_CACHED_LEVELS
#undef CPUIDLEVEL
   CPUID_NUM_LEVELS
} CpuidLevels;

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
} CpuidRegs;

/*
 * CPU vendors
 */

typedef enum {
   CPUID_VENDOR_UNKNOWN,
   CPUID_VENDOR_COMMON,
   CPUID_VENDOR_INTEL,
   CPUID_VENDOR_AMD,
   CPUID_VENDOR_CYRIX,
   CPUID_NUM_VENDORS
} CpuidVendors;

#define CPUID_INTEL_VENDOR_STRING       "GenuntelineI"
#define CPUID_AMD_VENDOR_STRING         "AuthcAMDenti"
#define CPUID_CYRIX_VENDOR_STRING       "CyriteadxIns"
#define CPUID_INTEL_VENDOR_STRING_FIXED "GenuineIntel"
#define CPUID_AMD_VENDOR_STRING_FIXED   "AuthenticAMD"
#define CPUID_CYRIX_VENDOR_STRING_FIXED "CyrixInstead"


/*
 * FIELDDEF can be defined to process the CPUID information provided
 * in the following CPUID_FIELD_DATA macro.  The first parameter is
 * the CPUID level of the feature (must be defined in CPUID_*_LEVELS.
 * The second parameter is the register the field is contained in
 * (defined in CPUID_REGS).  The third field is the vendor this
 * feature applies to.  "COMMON" means all vendors apply.  UNKNOWN may
 * not be used here.  The fourth and fifth parameters are the bit
 * position of the field and the width, respectively.  The sixth is
 * the text name of the field.
 *
 * The seventh and eighth parameters specify the default CPUID
 * behavior for power-on, guest view, and migration tests (cpt/rsm &
 * vmotion).  The eighth parameter is ignored for types other than
 * MASK & TEST, and must be zero in this case.
 *
 * When adding a new field, be sure to consider its purpose.  The
 * following list of types is provided in order of likely use.
 *
 * NOTE: this form of representation is separate from the masking
 * system specified via the config file.  That is because this
 * representation must take into account multi-bit fields.
 *
 * HOST    - Passthrough host value and cannot change during migration.
 * MASK, 0 - Hide from the guest, because we don't support it or we
 *           don't want the guest to know that it exists.
 * IGNORE  - Ignore this field for all tests
 *
 *    (Think twice before using the below mask types/combinations)
 *
 * MASK, x - Force the guest to always see x, and don't compare for
 *           migration -- only APIC as of today; it is controlled by
 *           software and we know how to toggle it
 * TEST, x - Require host CPUID field to be x for power-on
 * RSVD    - Hidden from the guest, but compared during migration
 *
 *
 * Table to explain mask type meanings:
 *
 *                         IGNR   MASK   TEST   HOST   RSVD
 * --------------------------------------------------------
 * Req'd val for power-on   -      -      x      -      -
 * Value guest sees         *      x      *      *      0
 * Checked on migration?    N      N      Y      Y      Y
 *
 * * - initial host's power-on CPUID value
 *
 * FIELDDEFA takes a ninth parameter, the name used when creating
 * accessor functions in lib/public/cpuidInfoFuncs.h.
 *
 * FLAGDEF/FLAGDEFA is defined identically to fields, but their
 * accessors are more appropriate for 1-bit flags.
 */

typedef enum {
   CPUID_FIELD_MASK_IGNORE,
   CPUID_FIELD_MASK_MASK,
   CPUID_FIELD_MASK_TEST,
   CPUID_FIELD_MASK_HOST,
   CPUID_FIELD_MASK_RSVD,
   CPUID_NUM_FIELD_MASKS
} CpuidFieldMasks;

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,   MASK TYPE, SET TO, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_0                                               \
FIELDDEF(  0, EAX, COMMON,  0, 32, NUMLEVELS,           IGNORE,  0, FALSE)     \
FIELDDEF(  0, EBX, COMMON,  0, 32, VENDOR1,             HOST,    0, TRUE)      \
FIELDDEF(  0, ECX, COMMON,  0, 32, VENDOR3,             HOST,    0, TRUE)      \
FIELDDEF(  0, EDX, COMMON,  0, 32, VENDOR2,             HOST,    0, TRUE)
                                                        
/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,   MASK TYPE, SET TO, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_1                                               \
FIELDDEFA( 1, EAX, COMMON,  0,  4, STEPPING,            IGNORE,  0, FALSE, STEPPING)  \
FIELDDEFA( 1, EAX, COMMON,  4,  4, MODEL,               IGNORE,  0, FALSE, MODEL)     \
FIELDDEFA( 1, EAX, COMMON,  8,  4, FAMILY,              HOST,    0, FALSE, FAMILY)    \
FIELDDEF(  1, EAX, COMMON, 12,  2, TYPE,                IGNORE,  0, FALSE)            \
FIELDDEFA( 1, EAX, COMMON, 16,  4, EXTMODEL,            IGNORE,  0, FALSE, EXT_MODEL) \
FIELDDEFA( 1, EAX, COMMON, 20,  8, EXTFAMILY,           HOST,    0, FALSE, EXT_FAMILY) \
FIELDDEF(  1, EBX, COMMON,  0,  8, BRAND_ID,            IGNORE,  0, FALSE)            \
FIELDDEF(  1, EBX, COMMON,  8,  8, CLFL_SIZE,           IGNORE,  0, FALSE)            \
FIELDDEFA( 1, EBX, COMMON, 16,  8, LCPU_COUNT,          IGNORE,  0, FALSE, LCPU_COUNT) \
FIELDDEFA( 1, EBX, COMMON, 24,  8, APICID,              IGNORE,  0, FALSE, APICID)    \
FLAGDEFA(  1, ECX, COMMON, 0,   1, SSE3,                HOST,    0, TRUE,  SSE3)      \
FLAGDEF(   1, ECX, INTEL,  2,   1, NDA2,                MASK,    0, FALSE)            \
FLAGDEFA(  1, ECX, COMMON, 3,   1, MWAIT,               MASK,    0, FALSE, MWAIT)     \
FLAGDEFA(  1, ECX, INTEL,  4,   1, DSCPL,               MASK,    0, FALSE, DSCPL)     \
FLAGDEFA(  1, ECX, INTEL,  5,   1, VMX,                 MASK,    0, FALSE, VMX)       \
FLAGDEF(   1, ECX, INTEL,  6,   1, SMX,                 MASK,    0, FALSE)            \
FLAGDEF(   1, ECX, INTEL,  7,   1, EST,                 MASK,    0, FALSE)            \
FLAGDEF(   1, ECX, INTEL,  8,   1, TM2,                 MASK,    0, FALSE)            \
FLAGDEFA(  1, ECX, COMMON, 9,   1, SSSE3,               HOST,    0, TRUE,  SSSE3)     \
FLAGDEF(   1, ECX, INTEL,  10,  1, HTCACHE,             MASK,    0, FALSE)            \
FLAGDEFA(  1, ECX, COMMON, 13,  1, CMPX16,              HOST,    0, TRUE,  CMPX16)    \
FLAGDEF(   1, ECX, INTEL,  14,  1, xPPR,                MASK,    0, FALSE)            \
FLAGDEF(   1, ECX, INTEL,  15,  1, PERF_MSR,            MASK,    0, FALSE)            \
FLAGDEF(   1, ECX, INTEL,  18,  1, DCA,                 MASK,    0, FALSE)            \
FLAGDEFA(  1, ECX, INTEL,  19,  1, SSE41,               HOST,    0, TRUE,  SSE41)     \
FLAGDEFA(  1, ECX, INTEL,  20,  1, SSE42,               HOST,    0, TRUE,  SSE42)     \
FLAGDEF(   1, ECX, INTEL,  21,  1, X2APIC,              MASK,    0, FALSE)            \
FLAGDEF(   1, ECX, INTEL,  22,  1, MOVBE,               RSVD,    0, TRUE)             \
FLAGDEFA(  1, ECX, COMMON, 23,  1, POPCNT,              HOST,    0, TRUE,  POPCNT)    \
FLAGDEF(   1, ECX, INTEL,  24,  1, ULE,                 RSVD,    0, TRUE)             \
FLAGDEF(   1, ECX, INTEL,  26,  1, XSAVE,               MASK,    0, FALSE)            \
FLAGDEF(   1, ECX, INTEL,  27,  1, OSXSAVE,             RSVD,    0, TRUE)             \
FLAGDEFA(  1, ECX, COMMON, 31,  1, HYPERVISOR,          IGNORE,  0, FALSE, HYPERVISOR)\
FLAGDEFA(  1, EDX, COMMON, 0,   1, FPU,                 HOST,    0, TRUE, FPU)        \
FLAGDEFA(  1, EDX, COMMON, 1,   1, VME,                 HOST,    0, FALSE, VME)       \
FLAGDEF(   1, EDX, COMMON, 2,   1, DBGE,                HOST,    0, FALSE)            \
FLAGDEF(   1, EDX, COMMON, 3,   1, PGSZE,               HOST,    0, FALSE)            \
FLAGDEFA(  1, EDX, COMMON, 4,   1, TSC,                 HOST,    0, TRUE, TSC)        \
FLAGDEF(   1, EDX, COMMON, 5,   1, MSR,                 HOST,    0, FALSE)            \
FLAGDEFA(  1, EDX, COMMON, 6,   1, PAE,                 HOST,    0, FALSE, PAE)       \
FLAGDEF(   1, EDX, COMMON, 7,   1, MCK,                 HOST,    0, FALSE)            \
FLAGDEF(   1, EDX, COMMON, 8,   1, CPMX,                HOST,    0, TRUE)             \
FLAGDEFA(  1, EDX, COMMON, 9,   1, APIC,                MASK,    1, FALSE, APIC)      \
FLAGDEFA(  1, EDX, COMMON, 11,  1, SEP,                 HOST,    0, TRUE,  SEP)       \
FLAGDEFA(  1, EDX, COMMON, 12,  1, MTRR,                HOST,    0, FALSE, MTRR)      \
FLAGDEFA(  1, EDX, COMMON, 13,  1, PGE,                 HOST,    0, FALSE, PGE)       \
FLAGDEFA(  1, EDX, COMMON, 14,  1, MCA,                 HOST,    0, FALSE, MCA)       \
FLAGDEFA(  1, EDX, COMMON, 15,  1, CMOV,                HOST,    0, TRUE,  CMOV)      \
FLAGDEFA(  1, EDX, COMMON, 16,  1, PAT,                 HOST,    0, FALSE, PAT)       \
FLAGDEF(   1, EDX, COMMON, 17,  1, 36PG,                HOST,    0, FALSE)            \
FLAGDEF(   1, EDX, INTEL,  18,  1, PSN,                 HOST,    0, FALSE)            \
FLAGDEFA(  1, EDX, COMMON, 19,  1, CLFL,                HOST,    0, TRUE,  CLFL)      \
FLAGDEF(   1, EDX, INTEL,  21,  1, DTES,                HOST,    0, FALSE)            \
FLAGDEF(   1, EDX, INTEL,  22,  1, ACPI,                HOST,    0, FALSE)            \
FLAGDEFA(  1, EDX, COMMON, 23,  1, MMX,                 HOST,    0, TRUE,  MMX)       \
FLAGDEFA(  1, EDX, COMMON, 24,  1, FXSAVE,              HOST,    0, TRUE,  FXSAVE)    \
FLAGDEFA(  1, EDX, COMMON, 25,  1, SSE,                 HOST,    0, TRUE,  SSE)       \
FLAGDEFA(  1, EDX, COMMON, 26,  1, SSE2,                HOST,    0, TRUE,  SSE2)      \
FLAGDEF(   1, EDX, INTEL,  27,  1, SS,                  HOST,    0, FALSE)            \
FLAGDEFA(  1, EDX, COMMON, 28,  1, HT,                  MASK,    0, FALSE, HT)        \
FLAGDEF(   1, EDX, INTEL,  29,  1, TM,                  MASK,    0, FALSE)            \
FLAGDEF(   1, EDX, INTEL,  30,  1, IA64,                MASK,    0, FALSE)            \
FLAGDEF(   1, EDX, INTEL,  31,  1, PBE,                 MASK,    0, FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,   MASK TYPE, SET TO, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_4                                               \
FIELDDEF(  4, EAX, INTEL,   0,  5, CACHE_TYPE,          IGNORE,  0, FALSE)            \
FIELDDEF(  4, EAX, INTEL,   5,  3, CACHE_LEVEL,         IGNORE,  0, FALSE)            \
FIELDDEF(  4, EAX, INTEL,  14, 12, CACHE_NUMHT_SHARING, IGNORE,  0, FALSE)            \
FIELDDEFA( 4, EAX, INTEL,  26,  6, CORE_COUNT,          IGNORE,  0, FALSE, INTEL_CORE_COUNT)  \
FIELDDEF(  4, EBX, INTEL,   0, 12, CACHE_LINE,          IGNORE,  0, FALSE)            \
FIELDDEF(  4, EBX, INTEL,  12, 10, CACHE_PART,          IGNORE,  0, FALSE)            \
FIELDDEF(  4, EBX, INTEL,  22, 10, CACHE_WAYS,          IGNORE,  0, FALSE)

/*     LEVEL, REG, VENDOR, POS, SIZE, NAME,   MASK TYPE, SET TO, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_5                                           \
FIELDDEF(  5, EAX, COMMON,  0, 16, MWAIT_MIN_SIZE,      IGNORE,  0, FALSE) \
FIELDDEF(  5, EBX, COMMON,  0, 16, MWAIT_MAX_SIZE,      IGNORE,  0, FALSE) \
FLAGDEF(   5, ECX, COMMON,  0,  1, MWAIT_EXTENSIONS,    IGNORE,  0, FALSE) \
FLAGDEF(   5, ECX, COMMON,  1,  1, MWAIT_INTR_BREAK,    IGNORE,  0, FALSE) \
FIELDDEF(  5, EDX, INTEL,   0,  4, MWAIT_C0_SUBSTATE,   IGNORE,  0, FALSE) \
FIELDDEF(  5, EDX, INTEL,   4,  4, MWAIT_C1_SUBSTATE,   IGNORE,  0, FALSE) \
FIELDDEF(  5, EDX, INTEL,   8,  4, MWAIT_C2_SUBSTATE,   IGNORE,  0, FALSE) \
FIELDDEF(  5, EDX, INTEL,  12,  4, MWAIT_C3_SUBSTATE,   IGNORE,  0, FALSE) \
FIELDDEF(  5, EDX, INTEL,  16,  4, MWAIT_C4_SUBSTATE,   IGNORE,  0, FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,   MASK TYPE, SET TO, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_6                                               \
FLAGDEF(   6, EAX, INTEL,   0,  1, THERMAL_SENSOR,      IGNORE,  0, FALSE)     \
FLAGDEF(   6, EAX, INTEL,   1,  1, TURBO_MODE,          IGNORE,  0, FALSE)     \
FIELDDEF(  6, EBX, INTEL,   0,  4, NUM_INTR_THRESHOLDS, IGNORE,  0, FALSE)     \
FLAGDEF(   6, ECX, INTEL,   0,  1, HW_COORD_FEEDBACK,   IGNORE,  0, FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,   MASK TYPE, SET TO, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_A                                               \
FIELDDEF(  A, EAX, INTEL,   0,  8, PMC_VERSION,         IGNORE,  0, FALSE)            \
FIELDDEFA( A, EAX, INTEL,   8,  8, NUM_PMCS,            IGNORE,  0, FALSE, NUM_PMCS)  \
FIELDDEF(  A, EAX, INTEL,  16,  8, PMC_BIT_WIDTH,       IGNORE,  0, FALSE)            \
FIELDDEF(  A, EAX, INTEL,  24,  8, PMC_EBX_LENGTH,      IGNORE,  0, FALSE)            \
FLAGDEF(   A, EBX, INTEL,   0,  1, PMC_CORE_CYCLE,      IGNORE,  0, FALSE)            \
FLAGDEF(   A, EBX, INTEL,   1,  1, PMC_INSTR_RETIRED,   IGNORE,  0, FALSE)            \
FLAGDEF(   A, EBX, INTEL,   2,  1, PMC_REF_CYCLES,      IGNORE,  0, FALSE)            \
FLAGDEF(   A, EBX, INTEL,   3,  1, PMC_LAST_LVL_CREF,   IGNORE,  0, FALSE)            \
FLAGDEF(   A, EBX, INTEL,   4,  1, PMC_LAST_LVL_CMISS,  IGNORE,  0, FALSE)            \
FLAGDEF(   A, EBX, INTEL,   5,  1, PMC_BR_INST_RETIRED, IGNORE,  0, FALSE)            \
FLAGDEF(   A, EBX, INTEL,   6,  1, PMC_BR_MISS_RETIRED, IGNORE,  0, FALSE)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,   MASK TYPE, SET TO, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_80                                              \
FIELDDEF( 80, EAX, COMMON,  0, 32, NUM_EXT_LEVELS,      IGNORE,  0, FALSE)            \
FIELDDEF( 80, EBX, AMD,     0, 32, AMD_VENDOR1,         IGNORE,  0, FALSE)            \
FIELDDEF( 80, ECX, AMD,     0, 32, AMD_VENDOR3,         IGNORE,  0, FALSE)            \
FIELDDEF( 80, EDX, AMD,     0, 32, AMD_VENDOR2,         IGNORE,  0, FALSE)
                                                        
/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,   MASK TYPE, SET TO, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_81                                              \
FIELDDEF( 81, EAX, INTEL,   0, 32, UNKNOWN81EAX,        IGNORE,  0, FALSE)            \
FIELDDEF( 81, EAX, AMD,     0,  4, STEPPING,            IGNORE,  0, FALSE)            \
FIELDDEF( 81, EAX, AMD,     4,  4, MODEL,               IGNORE,  0, FALSE)            \
FIELDDEF( 81, EAX, AMD,     8,  4, FAMILY,              IGNORE,  0, FALSE)            \
FIELDDEF( 81, EAX, AMD,    12,  2, TYPE,                IGNORE,  0, FALSE)            \
FIELDDEF( 81, EAX, AMD,    16,  4, EXTMODEL,            IGNORE,  0, FALSE)            \
FIELDDEF( 81, EAX, AMD,    20,  8, EXTFAMILY,           IGNORE,  0, FALSE)            \
FIELDDEF( 81, EBX, INTEL,   0, 32, UNKNOWN81EBX,        IGNORE,  0, FALSE)            \
FIELDDEF( 81, EBX, AMD,     0, 16, BRAND_ID,            IGNORE,  0, FALSE)            \
FIELDDEF( 81, EBX, AMD,    16, 16, UNDEF,               IGNORE,  0, FALSE)            \
FLAGDEFA( 81, ECX, COMMON,  0,  1, LAHF,                HOST,    0, TRUE,  LAHF64)    \
FLAGDEFA( 81, ECX, AMD,     1,  1, CMPLEGACY,           MASK,    0, FALSE, CMPLEGACY) \
FLAGDEFA( 81, ECX, AMD,     2,  1, SVM,                 MASK,    0, FALSE, SVM)       \
FLAGDEFA( 81, ECX, AMD,     3,  1, EXTAPICSPC,          HOST,    0, FALSE, EXTAPICSPC) \
FLAGDEFA( 81, ECX, AMD,     4,  1, CR8AVAIL,            MASK,    0, FALSE, CR8AVAIL)  \
FLAGDEFA( 81, ECX, AMD,     5,  1, ABM,                 HOST,    0, TRUE,  ABM)       \
FLAGDEFA( 81, ECX, AMD,     6,  1, SSE4A,               HOST,    0, TRUE,  SSE4A)     \
FLAGDEF(  81, ECX, AMD,     7,  1, MISALIGNED_SSE,      HOST,    0, TRUE)             \
FLAGDEF(  81, ECX, AMD,     8,  1, 3DNPREFETCH,         HOST,    0, TRUE)             \
FLAGDEF(  81, ECX, AMD,     9,  1, OSVW,                MASK,    0, FALSE)            \
FLAGDEF(  81, ECX, AMD,    10,  1, IBS,                 MASK,    0, FALSE)            \
FLAGDEF(  81, ECX, AMD,    11,  1, SSE5,                RSVD,    0, TRUE)             \
FLAGDEF(  81, ECX, AMD,    12,  1, SKINIT,              MASK,    0, FALSE)            \
FLAGDEF(  81, ECX, AMD,    13,  1, WATCHDOG,            MASK,    0, FALSE)            \
FLAGDEF(  81, EDX, AMD,     0,  1, FPU,                 HOST,    0, TRUE)             \
FLAGDEF(  81, EDX, AMD,     1,  1, VME,                 HOST,    0, FALSE)            \
FLAGDEF(  81, EDX, AMD,     2,  1, DBGE,                HOST,    0, FALSE)            \
FLAGDEF(  81, EDX, AMD,     3,  1, PGSZE,               HOST,    0, FALSE)            \
FLAGDEF(  81, EDX, AMD,     4,  1, TSC,                 HOST,    0, TRUE)             \
FLAGDEF(  81, EDX, AMD,     5,  1, MSR,                 HOST,    0, FALSE)            \
FLAGDEF(  81, EDX, AMD,     6,  1, PAE,                 HOST,    0, FALSE)            \
FLAGDEF(  81, EDX, AMD,     7,  1, MCK,                 HOST,    0, FALSE)            \
FLAGDEF(  81, EDX, AMD,     8,  1, CPMX,                HOST,    0, TRUE)             \
FLAGDEF(  81, EDX, AMD,     9,  1, APIC,                MASK,    1, FALSE)            \
FLAGDEFA( 81, EDX, COMMON, 11,  1, SYSC,                IGNORE,  0, TRUE, SYSC)       \
FLAGDEF(  81, EDX, AMD,    12,  1, MTRR,                HOST,    0, FALSE)            \
FLAGDEF(  81, EDX, AMD,    13,  1, PGE,                 HOST,    0, FALSE)            \
FLAGDEF(  81, EDX, AMD,    14,  1, MCA,                 HOST,    0, FALSE)            \
FLAGDEF(  81, EDX, AMD,    15,  1, CMOV,                HOST,    0, TRUE)             \
FLAGDEF(  81, EDX, AMD,    16,  1, PAT,                 HOST,    0, FALSE)            \
FLAGDEF(  81, EDX, AMD,    17,  1, 36PG,                HOST,    0, FALSE)            \
FLAGDEFA( 81, EDX, COMMON, 20,  1, NX,                  HOST,    0, FALSE, NX)        \
FLAGDEFA( 81, EDX, AMD,    22,  1, MMXEXT,              HOST,    0, TRUE,  MMXEXT)    \
FLAGDEF(  81, EDX, AMD,    23,  1, MMX,                 HOST,    0, TRUE)             \
FLAGDEF(  81, EDX, AMD,    24,  1, FXSAVE,              HOST,    0, TRUE)             \
FLAGDEFA( 81, EDX, AMD,    25,  1, FFXSR,               HOST,    0, FALSE, FFXSR)     \
FLAGDEF(  81, EDX, AMD,    26,  1, PDPE1GB,             MASK,    0, FALSE)            \
FLAGDEFA( 81, EDX, COMMON, 27,  1, RDTSCP,              HOST,    0, TRUE,  RDTSCP)    \
FLAGDEFA( 81, EDX, COMMON, 29,  1, LM,                  TEST,    1, FALSE, LM)        \
FLAGDEFA( 81, EDX, AMD,    30,  1, 3DNOWPLUS,           HOST,    0, TRUE,  3DNOWPLUS) \
FLAGDEFA( 81, EDX, AMD,    31,  1, 3DNOW,               HOST,    0, TRUE,  3DNOW)

/*    LEVEL, REG, VENDOR, POS, SIZE, NAME,   MASK TYPE, SET TO, CPL3, [FUNC] */
#define CPUID_FIELD_DATA_LEVEL_8x                                              \
FIELDDEF( 86, ECX, AMD,     0,  8, L2CACHE_LINE,        IGNORE,  0, FALSE)            \
FIELDDEF( 86, ECX, AMD,     8,  4, L2CACHE_LINE_PER_TAG, IGNORE, 0, FALSE)            \
FIELDDEF( 86, ECX, AMD,    12,  4, L2CACHE_WAYS,        IGNORE,  0, FALSE)            \
FIELDDEF( 86, ECX, AMD,    16, 16, L2CACHE_SIZE,        IGNORE,  0, FALSE)            \
FIELDDEF( 86, EDX, AMD,     0,  8, L3CACHE_LINE,        IGNORE,  0, FALSE)            \
FIELDDEF( 86, EDX, AMD,     8,  4, L3CACHE_LINE_PER_TAG,IGNORE,  0, FALSE)            \
FIELDDEF( 86, EDX, AMD,    12,  4, L3CACHE_WAYS,        IGNORE,  0, FALSE)            \
FIELDDEF( 86, EDX, AMD,    18, 14, L3CACHE_SIZE,        IGNORE,  0, FALSE)            \
FLAGDEF(  87, EDX, AMD,     0,  1, TS,                  IGNORE,  0, FALSE)            \
FLAGDEF(  87, EDX, AMD,     1,  1, FID,                 IGNORE,  0, FALSE)            \
FLAGDEF(  87, EDX, AMD,     2,  1, VID,                 IGNORE,  0, FALSE)            \
FLAGDEF(  87, EDX, AMD,     3,  1, TTP,                 IGNORE,  0, FALSE)            \
FLAGDEF(  87, EDX, AMD,     4,  1, TM,                  IGNORE,  0, FALSE)            \
FLAGDEF(  87, EDX, AMD,     5,  1, STC,                 IGNORE,  0, FALSE)            \
FLAGDEF(  87, EDX, AMD,     6,  1, 100MHZSTEPS,         IGNORE,  0, FALSE)            \
FLAGDEF(  87, EDX, AMD,     7,  1, HWPSTATE,            IGNORE,  0, FALSE)            \
FLAGDEF(  87, EDX, AMD,     8,  1, TSC_INVARIANT,       IGNORE,  0, FALSE)            \
FIELDDEFA(88, EAX, COMMON,  0,  8, PHYSBITS,            IGNORE,  0, FALSE, PHYS_BITS) \
FIELDDEFA(88, EAX, COMMON,  8,  8, VIRTBITS,            IGNORE,  0, FALSE, VIRT_BITS) \
FIELDDEFA(88, ECX, AMD,     0,  8, CORE_COUNT,          IGNORE,  0, FALSE, AMD_CORE_COUNT) \
FIELDDEF( 88, ECX, AMD,    12,  4, APICID_COREID_SIZE,  IGNORE,  0, FALSE)            \
FIELDDEFA(8A, EAX, AMD,     0,  8, SVM_REVISION,        MASK,    0, FALSE, SVM_REVISION) \
FLAGDEF(  8A, EAX, AMD,     8,  1, SVM_HYPERVISOR,      MASK,    0, FALSE)            \
FIELDDEF( 8A, EAX, AMD,     9, 23, SVMEAX_RSVD,         MASK,    0, FALSE)            \
FIELDDEF( 8A, EBX, AMD,     0, 32, SVM_N_ASIDS,         MASK,    0, FALSE)            \
FIELDDEF( 8A, ECX, AMD,     0, 32, SVMECX_RSVD,         MASK,    0, FALSE)            \
FLAGDEFA( 8A, EDX, AMD,     0,  1, SVM_NP,              MASK,    0, FALSE, NPT)       \
FLAGDEF(  8A, EDX, AMD,     1,  1, SVM_LBR,             MASK,    0, FALSE)            \
FLAGDEF(  8A, EDX, AMD,     2,  1, SVM_LOCK,            MASK,    0, FALSE)            \
FLAGDEF(  8A, EDX, AMD,     3,  1, SVM_NRIP,            MASK,    0, FALSE)            \
FIELDDEF( 8A, EDX, AMD,     4, 28, SVMEDX_RSVD,         MASK,    0, FALSE)

#define CPUID_FIELD_DATA                                              \
   CPUID_FIELD_DATA_LEVEL_0                                           \
   CPUID_FIELD_DATA_LEVEL_1                                           \
   CPUID_FIELD_DATA_LEVEL_4                                           \
   CPUID_FIELD_DATA_LEVEL_5                                           \
   CPUID_FIELD_DATA_LEVEL_6                                           \
   CPUID_FIELD_DATA_LEVEL_A                                           \
   CPUID_FIELD_DATA_LEVEL_80                                          \
   CPUID_FIELD_DATA_LEVEL_81                                          \
   CPUID_FIELD_DATA_LEVEL_8x

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

#define FIELDDEF(lvl, reg, vend, bitpos, size, name, m, v, c3)          \
   CPUID_##vend##_ID##lvl##reg##_##name##_SHIFT = bitpos,               \
   CPUID_##vend##_ID##lvl##reg##_##name##_MASK  =                       \
                      VMW_BIT_MASK(size) << bitpos,                     \
   CPUID_FEATURE_##vend##_ID##lvl##reg##_##name =                       \
                      CPUID_##vend##_ID##lvl##reg##_##name##_MASK,

#define FIELDDEFA(lvl, reg, vend, bitpos, size, name, m, v, c3, f)      \
         FIELDDEF(lvl, reg, vend, bitpos, size, name, m, v, c3)

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
 * If a level is listed as not masked/tested in CPUID_LEVELS above,
 * use all "don't care" values for its mask.
 */

#define CPT_DFLT_UNDEFINED_MASK "XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX"

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
#undef FIELD_FUNC


/*
 * Definitions of various fields' values and more complicated
 * macros/functions for reading cpuid fields.
 */

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
#define CPUID_FAMILY_EXTENDED 15

/* Intel model information */
#define CPUID_MODEL_PPRO       1
#define CPUID_MODEL_PII_3      3
#define CPUID_MODEL_PII_5      5
#define CPUID_MODEL_CELERON_6  6
#define CPUID_MODEL_PIII_7     7
#define CPUID_MODEL_PIII_8     8
#define CPUID_MODEL_PM         9
#define CPUID_MODEL_PIII_A     10
#define CPUID_MODEL_CORE       14
#define CPUID_MODEL_CORE2      15
#define CPUID_MODEL_PENRYN     0x17  // Effective model

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
CPUID_FAMILY_IS_486(uint32 _eax)
{
   return CPUID_EFFECTIVE_FAMILY(_eax) == CPUID_FAMILY_486;
}

static INLINE Bool
CPUID_FAMILY_IS_P5(uint32 _eax)
{
   return CPUID_EFFECTIVE_FAMILY(_eax) == CPUID_FAMILY_P5;
}

static INLINE Bool
CPUID_FAMILY_IS_P6(uint32 _eax)
{
   return CPUID_EFFECTIVE_FAMILY(_eax) == CPUID_FAMILY_P6;
}

static INLINE Bool
CPUID_FAMILY_IS_PENTIUM4(uint32 _eax)
{
   return CPUID_EFFECTIVE_FAMILY(_eax) == CPUID_FAMILY_P4;
}

/* Intel Core processors are Yonah, Merom, Conroe, Woodcrest,
 * Clovertown, Penryn, ...
 */
static INLINE Bool
CPUID_FAMILY_IS_CORE(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          CPUID_EFFECTIVE_MODEL(v) >= CPUID_MODEL_CORE;
}

static INLINE Bool
CPUID_FAMILY_IS_CORE2(uint32 v) // IN: %eax from CPUID with %eax=1.
{
   /* Assumes the CPU manufacturer is Intel. */
   return CPUID_FAMILY_IS_P6(v) &&
          CPUID_EFFECTIVE_MODEL(v) >= CPUID_MODEL_CORE2;
}

static INLINE Bool
CPUID_FAMILY_IS_K7(uint32 _eax)
{
   return CPUID_EFFECTIVE_FAMILY(_eax) == CPUID_FAMILY_K7;
}

static INLINE Bool
CPUID_FAMILY_IS_K8(uint32 _eax)
{
   return CPUID_EFFECTIVE_FAMILY(_eax) == CPUID_FAMILY_K8;
}

static INLINE Bool
CPUID_FAMILY_IS_K8EXT(uint32 _eax)
{
   /*
    * We check for this pattern often enough that it's
    * worth a separate function, for syntactic sugar.
    */
   return CPUID_FAMILY_IS_K8(_eax) &&
          CPUID_EXTENDED_MODEL(_eax) != 0;
}

static INLINE Bool
CPUID_FAMILY_IS_K8L(uint32 _eax)
{
   return CPUID_EFFECTIVE_FAMILY(_eax) == CPUID_FAMILY_K8L;
}

static INLINE Bool
CPUID_FAMILY_IS_K8MOBILE(uint32 _eax)
{
   /* Essentially a K8 (not K8L) part, but with mobile features. */
   return CPUID_EFFECTIVE_FAMILY(_eax) == CPUID_FAMILY_K8MOBILE;
}

static INLINE Bool
CPUID_FAMILY_IS_K8STAR(uint32 _eax)
{
   /*
    * Read function name as "K8*", as in wildcard.
    * Matches K8 or K8L or K8MOBILE
    */
   return CPUID_FAMILY_IS_K8(_eax) || CPUID_FAMILY_IS_K8L(_eax) ||
          CPUID_FAMILY_IS_K8MOBILE(_eax);
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


/*
 * On AMD chips before Opteron and Intel chips before P4 model 3,
 * WRMSR(TSC) clears the upper half of the TSC instead of using %edx.
 */
static INLINE Bool
CPUID_FullyWritableTSC(Bool isIntel, // IN
                       uint32 v)     // IN: %eax from CPUID with %eax=1.
{
   /*
    * Returns FALSE if:
    *   - Intel && P6 (pre-core) or
    *   - Intel && P4 (model < 3) or
    *   - !Intel && pre-K8 Opteron
    * Otherwise, returns TRUE.
    */
   return !((isIntel &&
             ((CPUID_FAMILY_IS_P6(v) &&
               CPUID_EFFECTIVE_MODEL(v) < CPUID_MODEL_CORE) ||
              (CPUID_FAMILY_IS_PENTIUM4(v) &&
               CPUID_EFFECTIVE_MODEL(v) < 3))) ||
            (!isIntel &&
             CPUID_FAMILY(v) < CPUID_FAMILY_K8));
}


/*
 * For certain AMD processors, an lfence instruction is necessary at various
 * places to ensure ordering.
 */

static INLINE Bool
CPUID_VendorRequiresFence(CpuidVendors vendor)
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
   // hard to get strcmp() in some environments, so do it in the raw
   return (id0->ebx == *(uint32 *) (CPUID_AMD_VENDOR_STRING + 0) &&
           id0->ecx == *(uint32 *) (CPUID_AMD_VENDOR_STRING + 4) &&
           id0->edx == *(uint32 *) (CPUID_AMD_VENDOR_STRING + 8));
}

static INLINE Bool
CPUID_ID1RequiresFence(CPUIDRegs *id1)
{
   return CPUID_VersionRequiresFence(id1->eax);
}

static INLINE Bool
CPUID_RequiresFence(CpuidVendors vendor, // IN
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
