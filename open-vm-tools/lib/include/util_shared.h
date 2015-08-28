/*********************************************************
 * Copyright (C) 2002-2015 VMware, Inc. All rights reserved.
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

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vm_assert.h"
#include "vm_basic_types.h"

/*
 * Defines util functions shared between the userlevel code and the monitor.
 */

/*
 *----------------------------------------------------------------------
 *
 * Util_Throttle --
 * 
 *   Use for throttling of warnings. 
 *
 * Results:
 *    Will return TRUE for an increasingly sparse set of counter values: 
 *    1, 2, ..., 100, 200, 300, ..., 10000, 20000, 30000, ..., . 
 *
 * Side effects:
 *   None.
 *
 *----------------------------------------------------------------------
 */

Bool
Util_Throttle(uint32 count)  // IN:
{
   return count <     100                          ||
         (count <   10000 && count %     100 == 0) ||
         (count < 1000000 && count %   10000 == 0) ||
                             count % 1000000 == 0;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_FastRand --
 *
 *	Generates the next random number in the pseudo-random sequence 
 *	defined by the multiplicative linear congruential generator 
 *	S' = 16807 * S mod (2^31 - 1).
 *	This is the ACM "minimal standard random number generator".  
 *	Based on method described by D.G. Carta in CACM, January 1990. 
 *	Usage: provide previous random number as the seed for next one.
 *
 * Precondition:
 *      0 < seed && seed < UTIL_FASTRAND_SEED_MAX
 *
 * Results:
 *	A random number.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

uint32
Util_FastRand(uint32 seed)
{
   uint64 product    = 33614 * (uint64)seed;
   uint32 product_lo = (uint32)(product & 0xffffffff) >> 1;
   uint32 product_hi = product >> 32;
   int32  test       = product_lo + product_hi;

   ASSERT(0 < seed && seed < UTIL_FASTRAND_SEED_MAX);

   return (test > 0) ? test : (test & UTIL_FASTRAND_SEED_MAX) + 1;
}


/*
 *----------------------------------------------------------------------
 * Util_Checksum64 --
 *    64-bit implementation of Fletcher's checksum.  Fast and simple.
 *    Guarantees a non-zero checksum (so 0 can mean "uninitialized").
 *    One known weakness is that the 32-bit value of 0 is
 *    indistinguishable from ~0.  The magic number 92680 was computed
 *    to be the most iterations before sum2 could overflow 64 bits.
 *----------------------------------------------------------------------
 */
uint64
Util_CheckSum64(uint32 *data, unsigned numWords)
{
   uint64 sum1 = 0xffffffff, sum2 = 0xffffffff;

   while (numWords > 0) {
      unsigned tlen = numWords > 92680 ? 92680 : numWords;
      numWords -= tlen;
      do {
         sum1 += *data++;
         sum2 += sum1;
      } while (--tlen > 0);
      sum1 = (sum1 & 0xffffffff) + (sum1 >> 32);
      sum2 = (sum2 & 0xffffffff) + (sum2 >> 32);
   }
   /* Second reduction step to reduce sums to 32 bits */
   sum1 = (sum1 & 0xffffffff) + (sum1 >> 32);
   sum2 = (sum2 & 0xffffffff) + (sum2 >> 32);
   return (sum2 << 32) | sum1;
}


#if defined(USERLEVEL) || defined(VMX86_DEBUG)
static uint32 crcTable[256];

static void 
UtilCRCMakeTable(void)
{
   uint32 c;
   int n, k;
   
   for (n = 0; n < 256; n++) {
      c = (uint32) n;
      for (k = 0; k < 8; k++) {
         if (c & 1) {
	    c = 0xedb88320L ^ (c >> 1);
	 } else {
	    c = c >> 1;
	 }
      }
      crcTable[n] = c;
   }
}
   
static INLINE_SINGLE_CALLER uint32 
UtilCRCUpdate(uint32 crc,
              const uint8 *buf,
              int len)
{
   uint32 c = crc;
   int n;
   static int crcTableComputed = 0;
   
   if (!crcTableComputed) {
      UtilCRCMakeTable();

      crcTableComputed = 1;
  }
    
   for (n = 0; n < len; n++) {
      c = crcTable[(c ^ buf[n]) & 0xff] ^ (c >> 8);
   }

   return c;
}

   
/*
 *----------------------------------------------------------------------
 *
 *  CRC_Compute --
 *
 *      computes the CRC of a block of data
 *
 * Results:
 *      
 *      CRC code
 *
 * Side effects:
 *      Sets up the crc table if it hasn't already been computed.
 *
 *----------------------------------------------------------------------
 */

uint32 
CRC_Compute(const uint8 *buf,
            int len)
{
   return UtilCRCUpdate(0xffffffffL, buf, len) ^ 0xffffffffL;
}

#endif /* defined (USERLEVEL) || defined(VMX86_DEBUG) */
