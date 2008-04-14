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
#endif

#include "vmware.h"
#include "random.h"


/*
 *-----------------------------------------------------------------------------
 *
 * Random_Crypto --
 *
 *      Generate 'size' bytes of cryptographically strong random bits in
 *      'buffer'. Use this function when you need non-predictable random
 *      bits, typically in security applications. Use Random_Quick below
 *      otherwise.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Random_Crypto(unsigned int size, // IN
              void *buffer)      // OUT
{
#if defined(_WIN32)
   HCRYPTPROV csp;
   DWORD error;

   if (CryptAcquireContext(&csp, NULL, NULL, PROV_RSA_FULL,
                           CRYPT_VERIFYCONTEXT) == FALSE) {
      error = GetLastError();
      Log("Random_Crypto: CryptAcquireContext failed %d\n", error);
      return FALSE;
   }

   if (CryptGenRandom(csp, size, buffer) == FALSE) {
      CryptReleaseContext(csp, 0);
      error = GetLastError();
      Log("Random_Crypto: CryptGenRandom failed %d\n", error);
      return FALSE;
   }

   if (CryptReleaseContext(csp, 0) == FALSE) {
      error = GetLastError();
      Log("Random_Crypto: CryptReleaseContext failed %d\n", error);
      return FALSE;
   }
#else
   int fd;
   int error;

   /*
    * We use /dev/urandom and not /dev/random because it is good enough and
    * because it cannot block. --hpreg
    */
   fd = open("/dev/urandom", O_RDONLY);
   if (fd < 0) {
      error = errno;
      Log("Random_Crypto: Failed to open: %d\n", error);
      return FALSE;
   }

   /* Although /dev/urandom does not block, it can return short reads. */
   while (size > 0) {
      ssize_t bytesRead = read(fd, buffer, size);
      if (bytesRead == 0 || (bytesRead == -1 && errno != EINTR)) {
         error = errno;
         close(fd);
         Log("Random_Crypto: Short read: %d\n", error);
         return FALSE;
      }
      if (bytesRead > 0) {
         size -= bytesRead;
         buffer = ((uint8 *) buffer) + bytesRead; 
      }
   }

   if (close(fd) < 0) {
      error = errno;
      Log("Random_Crypto: Failed to close: %d\n", error);
      return FALSE;
   }
#endif

   return TRUE;
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

struct rngstate {
  uint32 x[N];
  int p, q;
};

void *
Random_QuickSeed(uint32 seed) // IN:
{
   struct rngstate *rs;

   const uint32 xx[N] = {
      0x95F24DAB, 0x0B685215, 0xE76CCAE7, 0xAF3EC239, 0x715FAD23,
      0x24A590AD, 0x69E4B5EF, 0xBF456141, 0x96BC1B7B, 0xA7BDF825,
      0xC1DE75B7, 0x8858A9C9, 0x2DA87693, 0xB657F9DD, 0xFFDC8A9F,
      0x8121DA71, 0x8B823ECB, 0x885D05F5, 0x4E20CD47, 0x5A9AD5D9,
      0x512C0C03, 0xEA857CCD, 0x4CC1D30F, 0x8891A8A1, 0xA6B7AADB
   };

   rs = (struct rngstate *) malloc(sizeof *rs);

   if (rs != NULL) {
      uint32 i;

      for (i = 0; i < N; i++) {
         rs->x[i] = xx[i] ^ seed;
      }

      rs->p = N - 1;
      rs->q = N - M - 1;
   }

   return (void *) rs;
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
 *-----------------------------------------------------------------------------
 */

uint32
Random_Quick(void *context)
{
   uint32 y, z;

   struct rngstate *rs = (struct rngstate *) context;

   ASSERT(context);

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

