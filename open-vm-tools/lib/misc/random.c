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

/*
 * random.c --
 *
 *    Random bits generation. --hpreg
 */

#include <stdio.h>
#include <stdlib.h>

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
#include "random.h"


#if defined(_WIN32)
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
RandomBytesWin32(unsigned int size,  // IN:
                 void *buffer)       // OUT:
{
   HCRYPTPROV csp;

   if (CryptAcquireContext(&csp, NULL, NULL, PROV_RSA_FULL,
                           CRYPT_VERIFYCONTEXT) == FALSE) {
      return FALSE;
   }

   if (CryptGenRandom(csp, size, buffer) == FALSE) {
      CryptReleaseContext(csp, 0);
      return FALSE;
   }

   if (CryptReleaseContext(csp, 0) == FALSE) {
      return FALSE;
   }

   return TRUE;
}
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
RandomBytesPosix(const char *name,   // IN:
                 unsigned int size,  // IN:
                 void *buffer)       // OUT:
{
   int fd = open(name, O_RDONLY);

   if (fd == -1) {
      return FALSE;
   }

   /* Although /dev/urandom does not block, it can return short reads. */

   while (size > 0) {
      ssize_t bytesRead = read(fd, buffer, size);

      if ((bytesRead == 0) || ((bytesRead == -1) && (errno != EINTR))) {
         close(fd);

         return FALSE;
      }

      if (bytesRead > 0) {
         size -= bytesRead;
         buffer = ((uint8 *) buffer) + bytesRead; 
      }
   }

   if (close(fd) == -1) {
      return FALSE;
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

Bool
Random_Crypto(unsigned int size,  // IN:
              void *buffer)       // OUT:
{
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
   struct rqContext *rs;

   const uint32 xx[N] = {
      0x95F24DAB, 0x0B685215, 0xE76CCAE7, 0xAF3EC239, 0x715FAD23,
      0x24A590AD, 0x69E4B5EF, 0xBF456141, 0x96BC1B7B, 0xA7BDF825,
      0xC1DE75B7, 0x8858A9C9, 0x2DA87693, 0xB657F9DD, 0xFFDC8A9F,
      0x8121DA71, 0x8B823ECB, 0x885D05F5, 0x4E20CD47, 0x5A9AD5D9,
      0x512C0C03, 0xEA857CCD, 0x4CC1D30F, 0x8891A8A1, 0xA6B7AADB
   };

   rs = (struct rqContext *) malloc(sizeof *rs);

   if (rs != NULL) {
      uint32 i;

      for (i = 0; i < N; i++) {
         rs->x[i] = xx[i] ^ seed;
      }

      rs->p = N - 1;
      rs->q = N - M - 1;
   }

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


/*
 *----------------------------------------------------------------------
 *
 * Random_Simple --
 *
 *      Generates the next random number in the pseudo-random sequence
 *      defined by the multiplicative linear congruential generator
 *      S' = 33614 * S mod (2^31 - 1).  This is the ACM "minimal standard
 *      random number generator". Based on method described by D.G. Carta
 *      in CACM, January 1990.
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
