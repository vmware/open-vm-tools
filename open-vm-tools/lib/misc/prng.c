/*********************************************************
 * Copyright (C) 1998-2016,2019 VMware, Inc. All rights reserved.
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

/*
 * prng.c --
 *
 *    Pseudo-random bits generation. --hpreg
 *    (freestanding / no library dependencies)
 */

#ifdef VMKERNEL
#include "vmk_exports.h"
#endif
#include "vmware.h"
#include "random.h"


/*
 *-----------------------------------------------------------------------------
 *
 * RandomFastImpl --
 *
 *      Return uniformly distributed pseudo-random numbers in the range of 0
 *      and 2^32-1 using the algorithm PCG-XSH-RR by M. O'Neill.
 *      See http://www.pcg-random.org/
 *
 *      *** NOTE: THIS ALGORITHM IS SUBMITTED BUT NOT (YET) PUBLISHED
 *      *** IN A PEER-REVIEWED JOURNAL. It looks quite good (certainly better
 *      *** than Random_Simple), but is subject to change until standardized.
 *      *** If accepted, will likely replace Random_Quick and Random_Simple.
 *
 *      PCG-XSH-RR is an LCG:
 *         S' = (N * S + C) mod M
 *      with N = 6364136223846793005, M = 2^64, and C any odd number
 *      (thus making M and C relatively prime, as required for LCGs).
 *      PCG applies an output permutation "xorshift high, random rotate"
 *         output = rotate32((state ^ (state >> 18)) >> 27, state >> 59)
 *      The xorshift improves the quality of lower-order bits, and the
 *      random rotate uses highest-quality bits to further randomize
 *      lower-order bits; these permutations produce much higher quality
 *      random numbers than the underlying LCG.
 *
 *      The period of this RNG is 2^64, and takes 3.5-7 cycles depending
 *      on optimization.
 *
 *      Generated x86_64 assembly (for Random_Fast):
 *          mov    (%rdi),%rcx
 *          movabs $0x5851f42d4c957f2d,%rax
 *          mov    %rdi,%rdx
 *          or     $0x1,%rdx
 *          imul   %rcx,%rax     \
 *          add    %rdx,%rax      } LCG
 *          mov    %rax,(%rdi)   /
 *          mov    %rcx,%rax
 *          shr    $0x12,%rax   \
 *          xor    %rcx,%rax     \
 *          shr    $0x3b,%rcx     } rotate32
 *          shr    $0x1b,%rax    /
 *          ror    %cl,%eax     /
 *          retq
 *
 * Results:
 *      A random number in the specified range is returned.
 *
 * Side Effects:
 *      The RNG context is modified for later use by Random_Fast.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint32
RandomRor(uint32 val,    // IN:
          unsigned rot)  // IN: rotation
{
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
   // Processor has barrel roll right instruction
   __asm__("rorl %%cl, %0": "=rm"(val): "0"(val), "c"(rot));
   return val;
#else
   // Emulate barrel roll right with two shifts
   return (val >> rot) | (val << ((-rot) & 31));
#endif
}

static INLINE uint32
RandomFastImpl(uint64 *rs,  // IN/OUT:
               uint64 inc)  // IN:
{
   uint64 state = *rs;
   uint32 xorshift, rot;

   *rs = state * CONST64U(6364136223846793005) + inc;
   xorshift = ((state >> 18) ^ state) >> 27;
   rot = state >> 59;
   return RandomRor(xorshift, rot);
}

/*
 *-----------------------------------------------------------------------------
 *
 * Random_Fast --
 * Random_Fast64 --
 * Random_FastStream --
 * Random_FastStreamSeed --
 *
 *      Wrappers around RandomFastImpl to generate specific behaviors.
 *         Random_Fast: non-deterministic self-seeding based on address
 *         Random_FastStream: deterministic seeding
 *
 *      The self-seeding generator has two quirks worth mentioning.
 *      (1) PCG generates the random value and manipulates the state for
 *          the next use in parallel. (See the disassembly for details).
 *          The downside of doing so is that the first generated value is
 *          a mere permutation of the seed, so proper seeding requires running
 *          the generator once to distribute the seed across all bits.
 *      (2) PCG discards the 27 least significant state bits as low-quality.
 *          A naive seed which does not populate the upper bits (including the
 *          very common choices of 0, getpid(), or time()) effectively starts
 *          the 2^64 period at 0x00000000. This is statisically valid (we
 *          would expect 2^32 sequences to generate zero as their first value),
 *          but unexpected. To work around weak seeds, always run the generator
 *          once to bypass a potential 0x00000000. This DOES have the
 *          statistical flaw that 0x00000000 will be slightly less likely.
 *       Thus, when using Random_Fast, be sure to discard the first TWO values
 *       to ensure good seeding. If the statistical imbalance in doing so is
 *       of concern, use Random_FastStream with good (e.g. Random_Crypto)
 *       seeding, or use Random_Quick which has a stronger seed algorithm.
 *
 * Results:
 *      A random number in the specified range is returned.
 *
 * Side Effects:
 *      The RNG context is modified for later use by Random_Fast[Stream].
 *
 *-----------------------------------------------------------------------------
 */

uint32
Random_Fast(uint64 *rs)  // IN/OUT:
{
   uint64 inc = (uintptr_t)(void *)rs | 0x1;  // stream selector, must be odd
   return RandomFastImpl(rs, inc);
}

uint64
Random_Fast64(uint64 *rs)  // IN/OUT:
{
   return QWORD(Random_Fast(rs), Random_Fast(rs)) ;
}

uint32
Random_FastStream(RandomFastContext *rfc)  // IN/OUT:
{
   return RandomFastImpl(&rfc->state, rfc->sequence);
}

uint64
Random_FastStream64(RandomFastContext *rfc)  // IN/OUT:
{
   return QWORD(RandomFastImpl(&rfc->state, rfc->sequence),
                RandomFastImpl(&rfc->state, rfc->sequence));
}

void
Random_FastStreamSeed(RandomFastContext *rfc,  // OUT:
                      uint64 seed,             // IN:
                      uint64 seq)              // IN:
{
   rfc->state = 0;
   rfc->sequence = (seq << 1) | 0x1;  // stream selector, must be odd
   Random_FastStream(rfc);
   rfc->state += seed;
   Random_FastStream(rfc);
}


/*
 *----------------------------------------------------------------------
 *
 * Random_Simple --
 *
 *      Generates the next random number in the pseudo-random sequence
 *      defined by the multiplicative linear congruential generator
 *      S' = 16807 * S mod (2^31 - 1).  This is the ACM "minimal standard
 *      random number generator". Based on method described by D.G. Carta
 *      in CACM, January 1990, optimization to avoid modulo from
 *      Carl Waldspurger (OSDI 1994).
 *
 *      Usage: provide previous random number as the seed for next one.
 *
 * Results:
 *      A random integer number is returned.
 *
 * Side Effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Random_Simple(int seed)  // IN:
{
   uint64 product    = 33614 * (uint64) seed;
   uint32 product_lo = (uint32) (product & 0xFFFFFFFF) >> 1;
   uint32 product_hi = product >> 32;
   int32  test       = product_lo + product_hi;

   return test > 0 ? test : (test & 0x7FFFFFFF) + 1;
}

#ifdef VMKERNEL
VMK_KERNEL_EXPORT(Random_Fast);
VMK_KERNEL_EXPORT(Random_FastStream);
VMK_KERNEL_EXPORT(Random_FastStreamSeed);
VMK_KERNEL_EXPORT(Random_Simple);
#endif
