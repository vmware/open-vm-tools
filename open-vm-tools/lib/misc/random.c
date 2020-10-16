/*********************************************************
 * Copyright (C) 1998-2020 VMware, Inc. All rights reserved.
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
 * random.c --
 *
 *    Random bits generation. --hpreg
 *    (Also see prng.c for freestanding generators)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#   include <windows.h>
#   include <wincrypt.h>
#else
#   include <errno.h>
#   include <fcntl.h>
#   include <unistd.h>

#   define GENERIC_RANDOM_DEVICE "/dev/urandom"
#endif

#include "vmware.h"
#include "vm_basic_asm.h"  // RDTSC()
#include "log.h"
#include "random.h"
#include "util.h"

#if !defined(VMX86_RELEASE)
#include "vm_atomic.h"
#endif


#if defined(_WIN32)
#if !defined(VM_WIN_UWP)
/*
 *-----------------------------------------------------------------------------
 *
 * RandomBytesWin32 --
 *
 *      Generate 'size' bytes of cryptographically strong random bits in
 *      'buffer'.
 *
 * Results:
 *      TRUE   success
 *      FALSE  failure
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RandomBytesWin32(size_t size,   // IN:
                 void *buffer)  // OUT:
{
   HCRYPTPROV csp;

   if (size != (DWORD) size) {
      Log("%s: Overflow: %"FMTSZ"d\n", __FUNCTION__, size);
      return FALSE;
   }

   if (CryptAcquireContext(&csp, NULL, NULL, PROV_RSA_FULL,
                           CRYPT_VERIFYCONTEXT) == FALSE) {
      Log("%s: CryptAcquireContext failed: %d\n", __FUNCTION__, GetLastError());
      return FALSE;
   }

   if (CryptGenRandom(csp, (DWORD) size, buffer) == FALSE) {
      Log("%s: CryptGenRandom failed: %d\n", __FUNCTION__, GetLastError());
      CryptReleaseContext(csp, 0);
      return FALSE;
   }

   if (CryptReleaseContext(csp, 0) == FALSE) {
      Log("%s: CryptReleaseContext failed: %d\n", __FUNCTION__, GetLastError());
      return FALSE;
   }

   return TRUE;
}
#endif // !VM_WIN_UWP
#else
/*
 *-----------------------------------------------------------------------------
 *
 * RandomBytesPosix --
 *
 *      Generate 'size' bytes of cryptographically strong random bits in
 *      'buffer'.
 *
 * Results:
 *      TRUE   success
 *      FALSE  failure
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RandomBytesPosix(const char *name,  // IN:
                 size_t size,       // IN:
                 void *buffer)      // OUT:
{
   int fd = open(name, O_RDONLY);

   if (fd == -1) {
      Log("%s: failed to open %s: %s\n", __FUNCTION__, name, strerror(errno));
      return FALSE;
   }

   /*
    * Although /dev/urandom does not block, it can return short reads. That
    * said, reads returning nothing should not happen. Just in case, track
    * those any that do appear.
    */

   while (size > 0) {
      ssize_t bytesRead = read(fd, buffer, size);

      if ((bytesRead == 0) || ((bytesRead == -1) && (errno != EINTR))) {
         close(fd);

         if (bytesRead == 0) {
            Log("%s: zero length read while reading from %s\n",
                __FUNCTION__, name);
         } else {
            Log("%s: %"FMTSZ"u byte read failed while reading from %s: %s\n",
                __FUNCTION__, size, name, strerror(errno));
         }

         return FALSE;
      }

      if (bytesRead > 0) {
         size -= bytesRead;
         buffer = ((uint8 *) buffer) + bytesRead; 
      }
   }

   if (close(fd) == -1) {
      Log("%s: failed to close %s: %s\n", __FUNCTION__, name, strerror(errno));
   }

   return TRUE;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Random_Crypto --
 *
 *      Generate 'size' bytes of cryptographically strong random bits in
 *      'buffer'. Use this function when you need non-predictable random
 *      bits, typically in security applications, where the bits are generated
 *      external to the application.
 *
 *      DO NOT USE THIS FUNCTION UNLESS YOU HAVE AN ABSOLUTE, EXPLICIT
 *      NEED FOR CRYPTOGRAPHICALLY VALID RANDOM NUMBERS.
 *
 * Results:
 *      TRUE   success
 *      FALSE  failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

#if !defined(VMX86_RELEASE)
static Atomic_uint32 forceFail;
#endif

Bool
Random_Crypto(size_t size,   // IN:
              void *buffer)  // OUT:
{
#if !defined(VMX86_RELEASE)
   if (Atomic_ReadIfEqualWrite32(&forceFail, 1, 0) == 1) {
      return FALSE;
   }
#endif

#if defined(_WIN32)
   return RandomBytesWin32(size, buffer);
#else
   /*
    * We use /dev/urandom and not /dev/random because it is good enough and
    * because it cannot block. --hpreg
    */

   return RandomBytesPosix(GENERIC_RANDOM_DEVICE, size, buffer);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Random_CryptoFail --
 *
 *      This function will cause the next call to Random_Crypto to fail.
 *
 *      NOTE: This function does nothing in a release build.
 *
 * Results:
 *      TRUE   success
 *      FALSE  failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
Random_CryptoFail(void)
{
#if !defined(VMX86_RELEASE)
   Atomic_Write32(&forceFail, 1);
#endif
}



/*
 *-----------------------------------------------------------------------------
 *
 * Random_QuickSeed --
 *
 *      Creates a context for the quick random number generator and returns it.
 *
 * Results:
 *      A pointer to the context used for the random number generator. The
 *      context is dynamically allocated memory that must be freed by caller.
 *
 * Side Effects:
 *      None
 *
 * NOTE:
 *      Despite the look of the code this RNG is extremely fast.
 *
 *-----------------------------------------------------------------------------
 */

#define N 25
#define M 18

#define A 0x8EBFD028
#define S 7
#define B 0x2B5B2500
#define T 15
#define C 0xDB8B0000
#define L 16

struct rqContext {
  uint32 x[N];
  int p, q;
};

rqContext *
Random_QuickSeed(uint32 seed)  // IN:
{
   uint32 i;
   struct rqContext *rs = Util_SafeMalloc(sizeof *rs);

   const uint32 xx[N] = {
      0x95F24DAB, 0x0B685215, 0xE76CCAE7, 0xAF3EC239, 0x715FAD23,
      0x24A590AD, 0x69E4B5EF, 0xBF456141, 0x96BC1B7B, 0xA7BDF825,
      0xC1DE75B7, 0x8858A9C9, 0x2DA87693, 0xB657F9DD, 0xFFDC8A9F,
      0x8121DA71, 0x8B823ECB, 0x885D05F5, 0x4E20CD47, 0x5A9AD5D9,
      0x512C0C03, 0xEA857CCD, 0x4CC1D30F, 0x8891A8A1, 0xA6B7AADB
   };


   for (i = 0; i < N; i++) {
      rs->x[i] = xx[i] ^ seed;
   }

   rs->p = N - 1;
   rs->q = N - M - 1;

   return rs;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Random_Quick --
 *
 *      Return uniformly distributed pseudo-random numbers in the range of 0
 *      and 2^32-1 using the (research grade) tGFSR algorithm tt800. The period
 *      of this RNG is 2^(32*N) - 1 while generally having lower overhead than
 *      Random_Crypto().
 *
 * Results:
 *      A random number in the specified range is returned.
 *
 * Side Effects:
 *      The RNG context is modified for later use by Random_Quick.
 *
 * NOTE:
 *      Despite the look of the code this RNG is extremely fast.
 *
 *-----------------------------------------------------------------------------
 */

uint32
Random_Quick(rqContext *rs)  // IN/OUT:
{
   uint32 y, z;

   ASSERT(rs);

   if (rs->p == N - 1) {
      rs->p = 0;
   } else {
      (rs->p)++;
   }

   if (rs->q == N - 1) {
      rs->q = 0;
   } else {
      (rs->q)++;
   }

   z = rs->x[(rs->p)];
   y = rs->x[(rs->q)] ^ ( z >> 1 );

   if (z % 2) {
      y ^= A;
   }

   if (rs->p == N - 1) {
      rs->x[0] = y;
   } else {
      rs->x[(rs->p) + 1] = y;
   }

   y ^= ( ( y << S ) & B );
   y ^= ( ( y << T ) & C );

   y ^= ( y >> L ); // improves bits

   return y;
}


#if 0
/*
 *----------------------------------------------------------------------
 *
 * Random_SpeedTest --
 *
 *      Benchmarks the speed of various random number generators.
 *      (Intended for debugging).
 *
 * Results:
 *      Populates *out with cycle counts.
 *
 * Side Effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

typedef struct {
   uint64 nop;
   uint64 simple;
   uint64 fast;
   uint64 quick;
} RandomSpeedTestResults;
void Random_SpeedTest(uint64 iters, RandomSpeedTestResults *out);

static int ABSOLUTELY_NOINLINE
RandomNop(int *seed)
{
   return *(volatile int *)seed;
}

void
Random_SpeedTest(uint64 iters,                 // IN:
                 RandomSpeedTestResults *out)  // OUT:
{
   int i;
   uint64 start;
   int nop, simple;
   uint64 fast;
   rqContext *rq;

   simple = nop = getpid();
   Random_FastSeed(&fast, simple);
   rq = Random_QuickSeed(nop);

   start = RDTSC();
   for (i = 0; i < iters; i++) {
      RandomNop(&nop);
   }
   out->nop = RDTSC() - start;

   start = RDTSC();
   for (i = 0; i < iters; i++) {
      simple = Random_Simple(simple);
   }
   out->simple = RDTSC() - start;

   start = RDTSC();
   for (i = 0; i < iters; i++) {
      Random_Fast(&fast);
   }
   out->fast = RDTSC() - start;

   start = RDTSC();
   for (i = 0; i < iters; i++) {
      Random_Quick(rq);
   }
   out->quick = RDTSC() - start;

   free(rq);
}
#endif
