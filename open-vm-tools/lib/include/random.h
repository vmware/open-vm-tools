/*********************************************************
 * Copyright (C) 1998-2017,2019 VMware, Inc. All rights reserved.
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
 * random.h --
 *
 *    Random bits generation. Please use CryptoRandom_GetBytes if
 *    you require a FIPS-compliant source of random data.
 */

#ifndef __RANDOM_H__
#   define __RANDOM_H__

#include "vm_basic_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * OS-native random number generator based on one-way hashes.
 *
 * Good enough for any non-cryptographic use, but slower than
 * alternative algorithms. Recommended for generating seeds.
 *
 * Period: infinite
 * Speed: slow
 */

Bool Random_Crypto(size_t size,
                   void *buffer);

/*
 * High quality - research grade - random number generator.
 *
 * Period: 2^800
 * Speed: ~23 cycles
 */

typedef struct rqContext rqContext;

rqContext *Random_QuickSeed(uint32 seed);

uint32 Random_Quick(rqContext *context);

/*
 * Good quality non-deterministic random number generator.
 *
 * This generator uses &(*state) as the seed to go beyond 64-bits without
 * additional storage space; the low-grade entropy makes seeding
 * non-deterministic. Multiple generators in the same address space
 * with the same seed will produce unique streams, but using the same
 * seed will NOT produce the same sequence (due to ASLR). See
 * Raondom_FastStream for a deterministic generator.
 *
 * Initialize by setting *state to any seed (including zero) and calling
 * Random_Fast TWICE. (Unless the seed is very good, the first two values
 * are not very random).
 *
 * Period: 2^64
 * Speed: ~10 cycles
 */

uint32 Random_Fast(uint64 *state);
uint64 Random_Fast64(uint64 *state);

static INLINE void
Random_FastSeed(uint64 *state,  // OUT:
                uint64 seed)    // IN:
{
   *state = seed;
   (void) Random_Fast(state);
   (void) Random_Fast(state);
}

/*
 * Good quality deterministic random number generator.
 *
 * Period: 2^64
 * Speed: ~10 cycles
 */

typedef struct {
   uint64 state;
   uint64 sequence;
} RandomFastContext;

uint32 Random_FastStream(RandomFastContext *rfc);
uint64 Random_FastStream64(RandomFastContext *rfc);
void Random_FastStreamSeed(RandomFastContext *rfc, uint64 seed, uint64 seq);

/*
 * Simple multiplicative congruential RNG.
 *
 * Deprecated; prefer Random_Fast for better quality.
 * Period: 2^31-1
 * Speed: ~9 cycles
 */

int Random_Simple(int seed);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* __RANDOM_H__ */
