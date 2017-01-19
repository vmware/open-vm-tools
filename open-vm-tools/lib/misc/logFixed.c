/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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

#include "vmware.h"
#include "vm_basic_asm.h"

/*
 *-----------------------------------------------------------------------------
 *
 * LogFixed_Base2 --
 *
 *      Return log2(value) expressed as the ratio of two uint32 numbers.
 *
 * Results:
 *      As expected.
 *
 * Side effects:
 *      None
 *
 * NOTE:
 *      The log2(x) can be approximated as:
 *
 *      log2(x) = log2((1 + N) * 2^P)
 *              = P + log2(1 + N)
 *              = P + table[index]
 *
 *      where index are less significant bits than P, masked to whatever number
 *      of bits are necessary to ensure the accuracy goal. Thus, P is the bit
 *      number of the highest bit set; shifts are used to position the lower
 *      order bits to provide the requested number of index bits.
 *
 *      The number of index bits is TABLE_BITS, the size of the table,
 *      1 << TABLE_BITS.
 *
 *      The table entries are computed as the numerator of a fraction of
 *      BINARY_BASE:
 *
 *      table[i] = BINARY_BASE *
 *                        log2(1.0 + (((double) i) / (double) TABLE_SIZE)))
 *
 *      which allows log2(x) to be expressed as:
 *
 *      log2(x) = ((BINARY_BASE * P) + table[index]) / BINARY_BASE
 *
 *      maxError = 2.821500E-03; avgError = 1.935068E-05 over the range
 *      1 to 2E6. The test program is below.
 *
 *-----------------------------------------------------------------------------
 */

#define BINARY_BASE (64 * 1024)
#define TABLE_BITS 8

static const uint16 log2Table[] = {
     0,    368,    735,   1101,   1465,   1828,   2190,   2550,   2909,   3266,
  3622,   3977,   4331,   4683,   5034,   5383,   5731,   6078,   6424,   6769,
  7112,   7454,   7794,   8134,   8472,   8809,   9145,   9480,   9813,  10146,
 10477,  10807,  11136,  11463,  11790,  12115,  12440,  12763,  13085,  13406,
 13726,  14045,  14363,  14680,  14995,  15310,  15624,  15936,  16248,  16558,
 16868,  17176,  17484,  17790,  18096,  18400,  18704,  19006,  19308,  19608,
 19908,  20207,  20505,  20801,  21097,  21392,  21686,  21980,  22272,  22563,
 22854,  23143,  23432,  23720,  24007,  24293,  24578,  24862,  25146,  25429,
 25710,  25991,  26272,  26551,  26829,  27107,  27384,  27660,  27935,  28210,
 28483,  28756,  29028,  29300,  29570,  29840,  30109,  30377,  30644,  30911,
 31177,  31442,  31707,  31971,  32234,  32496,  32757,  33018,  33278,  33538,
 33796,  34054,  34312,  34568,  34824,  35079,  35334,  35588,  35841,  36093,
 36345,  36596,  36847,  37096,  37346,  37594,  37842,  38089,  38336,  38582,
 38827,  39071,  39315,  39559,  39801,  40044,  40285,  40526,  40766,  41006,
 41245,  41483,  41721,  41959,  42195,  42431,  42667,  42902,  43136,  43370,
 43603,  43836,  44068,  44299,  44530,  44760,  44990,  45219,  45448,  45676,
 45904,  46131,  46357,  46583,  46808,  47033,  47257,  47481,  47704,  47927,
 48149,  48371,  48592,  48813,  49033,  49253,  49472,  49690,  49909,  50126,
 50343,  50560,  50776,  50992,  51207,  51421,  51635,  51849,  52062,  52275,
 52487,  52699,  52910,  53121,  53331,  53541,  53751,  53960,  54168,  54376,
 54584,  54791,  54998,  55204,  55410,  55615,  55820,  56024,  56228,  56432,
 56635,  56837,  57040,  57242,  57443,  57644,  57844,  58044,  58244,  58443,
 58642,  58841,  59039,  59236,  59433,  59630,  59827,  60023,  60218,  60413,
 60608,  60802,  60996,  61190,  61383,  61576,  61768,  61960,  62152,  62343,
 62534,  62724,  62914,  63104,  63293,  63482,  63671,  63859,  64047,  64234,
 64421,  64608,  64794,  64980,  65165,  65351
};


void
LogFixed_Base2(uint64 value,         // IN:
               uint32 *numerator,    // OUT:
               uint32 *denominator)  // OUT:
{
   uint32 index;
   uint32 rawBits;
   uint32 maxBits;
   uint32 highBit;
   uint32 bitsOver;

   ASSERT_ON_COMPILE(ARRAYSIZE(log2Table) == (1 << TABLE_BITS));

   highBit = mssb64_0(value);
   ASSERT(highBit != -1);

   if (highBit <= TABLE_BITS) {
      index = (value << (TABLE_BITS - highBit)) & ((1 << TABLE_BITS) - 1);

      *numerator = (BINARY_BASE * highBit) + log2Table[index];
      *denominator = BINARY_BASE;

      return;
   }

   /*
    * If additional bits are available, use them to interpolate the table
    * to decrease the errors (especially the average). Bound the number of
    * additional bits as there is only a limited amount of bits available
    * from the interpolation table.
    */

   bitsOver = highBit - TABLE_BITS;
   if (bitsOver > 16) {
      bitsOver = 16;
   }

   maxBits = TABLE_BITS + bitsOver;

   rawBits = (value >> (highBit - maxBits)) & ((1 << maxBits) - 1);

   index = rawBits >> bitsOver;

   *numerator = (BINARY_BASE * highBit) + log2Table[index];

   if (index < (1 << TABLE_BITS) - 1) {
      uint32 extraBits = rawBits & ((1 << bitsOver) - 1);
      uint16 delta = log2Table[index + 1] - log2Table[index];

      *numerator += (extraBits * delta) / (1 << bitsOver);
   }

   *denominator = BINARY_BASE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * LogFixed_Base10 --
 *
 *      Return log10(value) expressed as the ratio of two uint32 numbers.
 *
 * Results:
 *      As expected.
 *
 * Side effects:
 *      None
 *
 * NOTE:
 *      Start with the identity:
 *
 *      log10(x) = log2(x) / log2(10)
 *
 *      Write as a fraction:
 *
 *      log2Numer / (log2Denom * log2(10))
 *
 *      log2(10) is 3.321928
 *
 *      maxError = 8.262237E-04; avgError = -1.787911E-05 over the range
 *      1 to 2E6. The test program is below.
 *
 *-----------------------------------------------------------------------------
 */

#define LOG10_BASE2 3.321928

void
LogFixed_Base10(uint64 value,         // IN:
                uint32 *numerator,    // OUT:
                uint32 *denominator)  // OUT:
{
   uint32 log2Numerator = 0;
   uint32 log2Denominator = 0;

   LogFixed_Base2(value, &log2Numerator, &log2Denominator);

   *numerator = log2Numerator;
   *denominator = LOG10_BASE2 * BINARY_BASE;
}

#if defined(TEST_PROG)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define LOG_TESTS 2000000

main()
{
   uint32 i;
   double sumLog2 = 0.0;
   double sumLog10 = 0.0;
   double maxErrorLog2 = 1E-30;
   double maxErrorLog10 = 1E-30;

   uint32 value = 1;

   for (i = 0; i < LOG_TESTS; i++) {
      double delta;
      double realLog;
      double fudgedLog;
      uint32 numerator;
      uint32 denominator;

      realLog = log2((double) value);
      LogFixed_Base2((uint64) value, &numerator, &denominator);
      fudgedLog = ((double) numerator) / ((double) denominator);

      delta = realLog - fudgedLog;

      if (delta > maxErrorLog2) {
         maxErrorLog2 = delta;
      }

      sumLog2 += delta;

      realLog = log10((double) value);
      LogFixed_Base10((uint64) value, &numerator, &denominator);
      fudgedLog = ((double) numerator) / ((double) denominator);

      delta = realLog - fudgedLog;

      if (delta > maxErrorLog10) {
         maxErrorLog10 = delta;
      }

      sumLog10 += delta;

      value++;
   }

   printf("LogFixed_BaseTwo: maxError = %E; avgError = %E\n", maxErrorLog2,
          sumLog2/((double) LOG_TESTS));

   printf("LogFixed_BaseTen: maxError = %E; avgError = %E\n", maxErrorLog10,
          sumLog10/((double) LOG_TESTS));
}
#endif
