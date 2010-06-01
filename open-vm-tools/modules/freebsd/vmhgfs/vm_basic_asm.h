/*********************************************************
 * Copyright (C) 2003 VMware, Inc. All rights reserved.
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
 * vm_basic_asm.h
 *
 *	Basic asm macros
 *
 *        ARM not implemented.
 */

#ifndef _VM_BASIC_ASM_H_
#define _VM_BASIC_ASM_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMNIXMOD
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMIROM
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "x86cpuid.h"

#if defined VM_X86_64
#include "vm_basic_asm_x86_64.h"
#elif defined __i386__
#include "vm_basic_asm_x86.h"
#endif


/*
 * x86-64 windows doesn't support inline asm so we have to use these
 * intrinsic functions defined in the compiler.  Not all of these are well
 * documented.  There is an array in the compiler dll (c1.dll) which has
 * an array of the names of all the intrinsics minus the leading
 * underscore.  Searching around in the ntddk.h file can also be helpful.
 *
 * The declarations for the intrinsic functions were taken from the DDK. 
 * Our declarations must match the ddk's otherwise the 64-bit c++ compiler
 * will complain about second linkage of the intrinsic functions.
 * We define the intrinsic using the basic types corresponding to the 
 * Windows typedefs. This avoids having to include windows header files
 * to get to the windows types.
 */
#ifdef _MSC_VER
#ifdef __cplusplus
extern "C" {
#endif
/*
 * It seems x86 & x86-64 windows still implements these intrinsic
 * functions.  The documentation for the x86-64 suggest the
 * __inbyte/__outbyte intrinsics eventhough the _in/_out work fine and
 * __inbyte/__outbyte aren't supported on x86.
 */
int            _inp(unsigned short);
unsigned short _inpw(unsigned short);
unsigned long  _inpd(unsigned short);

int            _outp(unsigned short, int);
unsigned short _outpw(unsigned short, unsigned short);
unsigned long  _outpd(uint16, unsigned long);
#pragma intrinsic(_inp, _inpw, _inpd, _outp, _outpw, _outpw, _outpd)
void _ReadWriteBarrier(void);
#pragma intrinsic(_ReadWriteBarrier)

#ifdef VM_X86_64
/*
 * intrinsic functions only supported by x86-64 windows as of 2k3sp1
 */
void             __cpuid(unsigned int*, unsigned int);
unsigned __int64 __rdtsc(void);
void             __stosw(unsigned short*, unsigned short, size_t);
void             __stosd(unsigned long*, unsigned long, size_t);
#pragma intrinsic(__cpuid, __rdtsc, __stosw, __stosd)

/*
 * intrinsic functions supported by x86-64 windows and newer x86
 * compilers (13.01.2035 for _BitScanForward).
 */
unsigned char  _BitScanForward(unsigned long*, unsigned long);
void           _mm_pause(void);
#pragma intrinsic(_BitScanForward, _mm_pause)
#endif /* VM_X86_64 */

#ifdef __cplusplus
}
#endif
#endif /* _MSC_VER */


#ifdef __GNUC__ // {
#if defined(__i386__) || defined(__x86_64__) // Only on x86*

/*
 * Checked against the Intel manual and GCC --hpreg
 *
 * volatile because reading from port can modify the state of the underlying
 * hardware.
 *
 * Note: The undocumented %z construct doesn't work (internal compiler error)
 *       with gcc-2.95.1
 */

#define __GCC_IN(s, type, name) \
static INLINE type              \
name(uint16 port)               \
{                               \
   type val;                    \
                                \
   __asm__ __volatile__(        \
      "in" #s " %w1, %0"        \
      : "=a" (val)              \
      : "Nd" (port)             \
   );                           \
                                \
   return val;                  \
}

__GCC_IN(b, uint8, INB)
__GCC_IN(w, uint16, INW)
__GCC_IN(l, uint32, IN32)


/*
 * Checked against the Intel manual and GCC --hpreg
 *
 * Note: The undocumented %z construct doesn't work (internal compiler error)
 *       with gcc-2.95.1
 */

#define __GCC_OUT(s, s2, port, val) do { \
   __asm__(                              \
      "out" #s " %" #s2 "1, %w0"         \
      :                                  \
      : "Nd" (port), "a" (val)           \
   );                                    \
} while (0)

#define OUTB(port, val) __GCC_OUT(b, b, port, val)
#define OUTW(port, val) __GCC_OUT(w, w, port, val)
#define OUT32(port, val) __GCC_OUT(l, , port, val)


#define GET_CURRENT_EIP(_eip) \
      __asm__ __volatile("call 0\n\tpopl %0" : "=r" (_eip): );


/*
 * Checked against the Intel manual and GCC --hpreg
 * 
 * Need __volatile__ and "memory" since CPUID has a synchronizing effect.
 * The CPUID may also change at runtime (APIC flag, etc).
 *
 */

/*
 * %ebx is reserved on i386 PIC.  Apple's gcc-5493 (gcc 4.0) compiling
 * for x86_64 incorrectly errors out saying %ebx is reserved.  This is
 * Apple bug 7304232.
 */
#if vm_x86_64 ? (defined __APPLE_CC__ && __APPLE_CC__ == 5493) : defined __PIC__
#if vm_x86_64
/*
 * Note that this generates movq %rbx,%rbx; cpuid; xchgq %rbx,%rbx ...
 * Unfortunately Apple's assembler does not have .ifnes, and I cannot
 * figure out how to do that with .if.   If we ever enable this code
 * on other 64bit systems, both movq & xchgq should be surrounded by
 * .ifnes \"%%rbx\", \"%q1\" & .endif
 */
#define VM_CPUID_BLOCK  "movq %%rbx, %q1\n\t" \
                        "cpuid\n\t"           \
                        "xchgq %%rbx, %q1\n\t"
#define VM_EBX_OUT(reg) "=&r"(reg)
#else
#define VM_CPUID_BLOCK  "movl %%ebx, %1\n\t" \
                        "cpuid\n\t"          \
                        "xchgl %%ebx, %1\n\t"
#define VM_EBX_OUT(reg) "=&rm"(reg)
#endif
#else
#define VM_CPUID_BLOCK  "cpuid"
#define VM_EBX_OUT(reg) "=b"(reg)
#endif

static INLINE void
__GET_CPUID(int eax,         // IN
            CPUIDRegs *regs) // OUT
{
   __asm__ __volatile__(
      VM_CPUID_BLOCK
      : "=a" (regs->eax), VM_EBX_OUT(regs->ebx), "=c" (regs->ecx), "=d" (regs->edx)
      : "a" (eax)
      : "memory"
   );
}

static INLINE void
__GET_CPUID2(int eax,         // IN
             int ecx,         // IN
             CPUIDRegs *regs) // OUT
{
   __asm__ __volatile__(
      VM_CPUID_BLOCK
      : "=a" (regs->eax), VM_EBX_OUT(regs->ebx), "=c" (regs->ecx), "=d" (regs->edx)
      : "a" (eax), "c" (ecx)
      : "memory"
   );
}

static INLINE uint32
__GET_EAX_FROM_CPUID(int eax) // IN
{
   uint32 ebx;

   __asm__ __volatile__(
      VM_CPUID_BLOCK
      : "=a" (eax), VM_EBX_OUT(ebx)
      : "a" (eax)
      : "memory", "%ecx", "%edx"
   );

   return eax;
}

static INLINE uint32
__GET_EBX_FROM_CPUID(int eax) // IN
{
   uint32 ebx;

   __asm__ __volatile__(
      VM_CPUID_BLOCK
      : "=a" (eax), VM_EBX_OUT(ebx)
      : "a" (eax)
      : "memory", "%ecx", "%edx"
   );

   return ebx;
}

static INLINE uint32
__GET_ECX_FROM_CPUID(int eax) // IN
{
   uint32 ecx;
   uint32 ebx;

   __asm__ __volatile__(
      VM_CPUID_BLOCK
      : "=a" (eax), VM_EBX_OUT(ebx), "=c" (ecx)
      : "a" (eax)
      : "memory", "%edx"
   );

   return ecx;
}

static INLINE uint32
__GET_EDX_FROM_CPUID(int eax) // IN
{
   uint32 edx;
   uint32 ebx;

   __asm__ __volatile__(
      VM_CPUID_BLOCK
      : "=a" (eax), VM_EBX_OUT(ebx), "=d" (edx)
      : "a" (eax)
      : "memory", "%ecx"
   );

   return edx;
}


static INLINE uint32
__GET_EAX_FROM_CPUID4(int ecx) // IN
{
   uint32 eax;
   uint32 ebx;

   __asm__ __volatile__(
      VM_CPUID_BLOCK
      : "=a" (eax), VM_EBX_OUT(ebx), "=c" (ecx)
      : "a" (4), "c" (ecx)
      : "memory", "%edx"
   );

   return eax;
}

#undef VM_CPUID_BLOCK
#undef VM_EBX_OUT

#endif // x86*

#elif defined(_MSC_VER) // } {
static INLINE  uint8
INB(uint16 port)
{
   return (uint8)_inp(port);
}
static INLINE void
OUTB(uint16 port, uint8 value)
{
   _outp(port, value);
}
static INLINE uint16
INW(uint16 port)
{
   return _inpw(port);
}
static INLINE void
OUTW(uint16 port, uint16 value)
{
   _outpw(port, value);
}
static INLINE  uint32
IN32(uint16 port)
{
   return _inpd(port);
}
static INLINE void
OUT32(uint16 port, uint32 value)
{
   _outpd(port, value);
}

#ifndef VM_X86_64
#ifdef NEAR
#undef NEAR
#endif

#define GET_CURRENT_EIP(_eip) do { \
   __asm call NEAR PTR $+5 \
   __asm pop eax \
   __asm mov _eip, eax \
} while (0)
#endif

static INLINE void
__GET_CPUID(int input, CPUIDRegs *regs)
{
#ifdef VM_X86_64
   __cpuid((unsigned int *)regs, input);
#else
   __asm push esi
   __asm push ebx
   __asm push ecx
   __asm push edx

   __asm mov  eax, input
   __asm mov  esi, regs
   __asm _emit 0x0f __asm _emit 0xa2
   __asm mov 0x0[esi], eax
   __asm mov 0x4[esi], ebx
   __asm mov 0x8[esi], ecx
   __asm mov 0xC[esi], edx

   __asm pop edx
   __asm pop ecx
   __asm pop ebx
   __asm pop esi
#endif
}

#ifdef VM_X86_64

/*
 * No inline assembly in Win64. Implemented in bora/lib/user in
 * cpuidMasm64.asm.
 */

extern void
__GET_CPUID2(int inputEax, int inputEcx, CPUIDRegs *regs);

#else // VM_X86_64

static INLINE void
__GET_CPUID2(int inputEax, int inputEcx, CPUIDRegs *regs)
{
   __asm push esi
   __asm push ebx
   __asm push ecx
   __asm push edx

   __asm mov  eax, inputEax
   __asm mov  ecx, inputEcx
   __asm mov  esi, regs
   __asm _emit 0x0f __asm _emit 0xa2
   __asm mov 0x0[esi], eax
   __asm mov 0x4[esi], ebx
   __asm mov 0x8[esi], ecx
   __asm mov 0xC[esi], edx

   __asm pop edx
   __asm pop ecx
   __asm pop ebx
   __asm pop esi
}
#endif

static INLINE uint32
__GET_EAX_FROM_CPUID(int input)
{
#ifdef VM_X86_64
   CPUIDRegs regs;
   __cpuid((unsigned int *)&regs, input);
   return regs.eax;
#else
   uint32 output;

   //NOT_TESTED();
   __asm push ebx
   __asm push ecx
   __asm push edx

   __asm mov  eax, input
   __asm _emit 0x0f __asm _emit 0xa2
   __asm mov  output, eax

   __asm pop edx
   __asm pop ecx
   __asm pop ebx

   return output;
#endif
}

static INLINE uint32
__GET_EBX_FROM_CPUID(int input)
{
#ifdef VM_X86_64
   CPUIDRegs regs;
   __cpuid((unsigned int *)&regs, input);
   return regs.ebx;
#else
   uint32 output;

   //NOT_TESTED();
   __asm push ebx
   __asm push ecx
   __asm push edx

   __asm mov  eax, input
   __asm _emit 0x0f __asm _emit 0xa2
   __asm mov  output, ebx

   __asm pop edx
   __asm pop ecx
   __asm pop ebx

   return output;
#endif
}

static INLINE uint32
__GET_ECX_FROM_CPUID(int input)
{
#ifdef VM_X86_64
   CPUIDRegs regs;
   __cpuid((unsigned int *)&regs, input);
   return regs.ecx;
#else
   uint32 output;

   //NOT_TESTED();
   __asm push ebx
   __asm push ecx
   __asm push edx

   __asm mov  eax, input
   __asm _emit 0x0f __asm _emit 0xa2
   __asm mov  output, ecx

   __asm pop edx
   __asm pop ecx
   __asm pop ebx

   return output;
#endif
}

static INLINE uint32
__GET_EDX_FROM_CPUID(int input)
{
#ifdef VM_X86_64
   CPUIDRegs regs;
   __cpuid((unsigned int *)&regs, input);
   return regs.edx;
#else
   uint32 output;

   //NOT_TESTED();
   __asm push ebx
   __asm push ecx
   __asm push edx

   __asm mov  eax, input
   __asm _emit 0x0f __asm _emit 0xa2
   __asm mov  output, edx

   __asm pop edx
   __asm pop ecx
   __asm pop ebx

   return output;
#endif
}

#ifdef VM_X86_64

/*
 * No inline assembly in Win64. Implemented in bora/lib/user in
 * cpuidMasm64.asm.
 */

extern uint32
__GET_EAX_FROM_CPUID4(int inputEcx);

#else // VM_X86_64

static INLINE uint32
__GET_EAX_FROM_CPUID4(int inputEcx)
{
   uint32 output;

   //NOT_TESTED();
   __asm push ebx
   __asm push ecx
   __asm push edx

   __asm mov  eax, 4
   __asm mov  ecx, inputEcx
   __asm _emit 0x0f __asm _emit 0xa2
   __asm mov  output, eax

   __asm pop edx
   __asm pop ecx
   __asm pop ebx

   return output;
}

#endif // VM_X86_64

#else // }
#error 
#endif

#define CPUID_FOR_SIDE_EFFECTS() ((void)__GET_EAX_FROM_CPUID(0))

static INLINE void
__GET_CPUID4(int inputEcx, CPUIDRegs *regs)
{
   __GET_CPUID2(4, inputEcx, regs);
}

/* The first parameter is used as an rvalue and then as an lvalue. */
#define GET_CPUID(_ax, _bx, _cx, _dx) { \
   CPUIDRegs regs;                      \
   __GET_CPUID(_ax, &regs);             \
   _ax = regs.eax;                      \
   _bx = regs.ebx;                      \
   _cx = regs.ecx;                      \
   _dx = regs.edx;                      \
}


/* Sequence recommended by Intel for the Pentium 4. */
#define INTEL_MICROCODE_VERSION() (             \
   __SET_MSR(MSR_BIOS_SIGN_ID, 0),              \
   __GET_EAX_FROM_CPUID(1),                     \
   __GET_MSR(MSR_BIOS_SIGN_ID))


#ifdef _MSC_VER   
static INLINE int
ffs(uint32 bitVector)
{
   int idx;
   if (!bitVector) {
      return 0;
   }
#ifdef VM_X86_64
   _BitScanForward((unsigned long*)&idx, (unsigned long)bitVector);
#else
   __asm bsf eax, bitVector
   __asm mov idx, eax
#endif
   return idx+1;
}
#endif

#ifdef __GNUC__
static INLINE void *
uint16set(void *dst, uint16 val, size_t count)
{
   int dummy0;
   int dummy1;

   __asm__ __volatile__("\t"
                        "cld"            "\n\t"
                        "rep ; stosw"    "\n"
                        : "=c" (dummy0), "=D" (dummy1)
                        : "0" (count), "1" (dst), "a" (val)
                        : "memory", "cc"
      );

   return dst;
}

static INLINE void *
uint32set(void *dst, uint32 val, size_t count)
{
   int dummy0;
   int dummy1;

   __asm__ __volatile__("\t"
                        "cld"            "\n\t"
                        "rep ; stosl"    "\n"
                        : "=c" (dummy0), "=D" (dummy1)
                        : "0" (count), "1" (dst), "a" (val)
                        : "memory", "cc"
      );

   return dst;
}

#elif defined(_MSC_VER)

static INLINE void *
uint16set(void *dst, uint16 val, size_t count)
{
#ifdef VM_X86_64
   __stosw((uint16*)dst, val, count);
#else
   __asm { pushf;
           mov ax, val;
           mov ecx, count;
           mov edi, dst;
           cld;
           rep stosw;
           popf;
   }
#endif
   return dst;
}

static INLINE void *
uint32set(void *dst, uint32 val, size_t count)
{
#ifdef VM_X86_64
   __stosd((unsigned long*)dst, (unsigned long)val, count);
#else
   __asm { pushf;
           mov eax, val;
           mov ecx, count;
           mov edi, dst;
           cld;
           rep stosd;
           popf;
   }
#endif
   return dst;
}

#else
#error "No compiler defined for uint*set"
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Bswap32 --
 *
 *      Swap the 4 bytes of "v" as follows: 3210 -> 0123.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint32
Bswap32(uint32 v) // IN
{
#ifdef __GNUC__ // {
   /* Checked against the Intel manual and GCC. --hpreg */
   __asm__(
      "bswap %0"
      : "=r" (v)
      : "0" (v)
   );
   return v;
#else // } {
   return    (v >> 24)
          | ((v >>  8) & 0xFF00)
          | ((v & 0xFF00) <<  8)
          |  (v << 24)          ;
#endif // }
}
#define Bswap Bswap32


/*
 *-----------------------------------------------------------------------------
 *
 * Bswap64 --
 *
 *      Swap the 8 bytes of "v" as follows: 76543210 -> 01234567.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint64
Bswap64(uint64 v) // IN
{
   return ((uint64)Bswap((uint32)v) << 32) | Bswap((uint32)(v >> 32));
}


#ifdef __GNUC__ // {
/*
 * COMPILER_MEM_BARRIER prevents the compiler from re-ordering memory
 * references accross the barrier.  NOTE: It does not generate any
 * instruction, so the CPU is free to do whatever it wants to...
 */
#define COMPILER_MEM_BARRIER() __asm__ __volatile__ ("": : :"memory")
#elif defined(_MSC_VER) // } {
#define COMPILER_MEM_BARRIER() _ReadWriteBarrier()
#endif // }


/*
 * PAUSE is a P4 instruction that improves spinlock power+performance;
 * on non-P4 IA32 systems, the encoding is interpreted as a REPZ-NOP.
 * Use volatile to avoid NOP removal.
 */
static INLINE void
PAUSE(void)
#ifdef __GNUC__
{
   __asm__ __volatile__( "pause" :);
}
#elif defined(_MSC_VER)
#ifdef VM_X86_64
{
   _mm_pause();
}
#else /* VM_X86_64 */
#pragma warning( disable : 4035)
{
   __asm _emit 0xf3 __asm _emit 0x90
}
#pragma warning (default: 4035)
#endif /* VM_X86_64 */
#else  /* __GNUC__  */
#error No compiler defined for PAUSE
#endif


/*
 * Checked against the Intel manual and GCC --hpreg
 *
 * volatile because the tsc always changes without the compiler knowing it.
 */
static INLINE uint64
RDTSC(void)
#ifdef __GNUC__
{
#ifdef VM_X86_64
   uint64 tscLow;
   uint64 tscHigh;

   __asm__ __volatile__(
      "rdtsc"
      : "=a" (tscLow), "=d" (tscHigh)
   );

   return tscHigh << 32 | tscLow;
#else
   uint64 tim;

   __asm__ __volatile__(
      "rdtsc"
      : "=A" (tim)
   );

   return tim;
#endif
}
#elif defined(_MSC_VER)
#ifdef VM_X86_64
{
   return __rdtsc();
}
#else
#pragma warning( disable : 4035)
{
   __asm _emit 0x0f __asm _emit 0x31
}
#pragma warning (default: 4035)
#endif /* VM_X86_64 */
#else  /* __GNUC__  */
#error No compiler defined for RDTSC
#endif /* __GNUC__  */

/*
 *-----------------------------------------------------------------------------
 *
 * DEBUGBREAK --
 *
 *    Does an int3 for MSVC / GCC. This is a macro to make sure int3 is
 *    always inlined.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef _MSC_VER
#define DEBUGBREAK()   __debugbreak()
#else
#define DEBUGBREAK()   __asm__ (" int $3 ")
#endif

#endif
