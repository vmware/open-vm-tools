/*********************************************************
 * Copyright (C) 1998-2015 VMware, Inc. All rights reserved.
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

/*
 * SHA-1 in C
 * Originally released by Steve Reid <steve@edmweb.com> into the public domain
 *
 * Test Vectors (from FIPS PUB 180-1)
 * "abc"
 *   A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
 * "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
 *   84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
 * A million repetitions of "a"
 *   34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
 *
 * Major changes:
 *
 *    12/98: JEB: Removed main and moved prototypes to sha1.h
 *                Made SHA1Transform a static function
 *
 *    10/14: rberinde: Added SSE3 code and test, cleaned up a bit.
 *
 * If any changes are made to this file, please run:
 *    test-esx -n misc/sha1.sh
*/

#if defined(USERLEVEL) || defined(_WIN32)
#   include <string.h>
#   if defined(_WIN32)
#      include <memory.h>
#   endif
#endif

#if defined(sun) && !defined(SOL9)
#include <memory.h>
#endif

#if defined(__FreeBSD__)
#   if defined(_KERNEL)
#      include <sys/libkern.h>
#      include <sys/systm.h>
#   else
#      include <string.h>
#   endif
#endif

#if defined(__APPLE__)
#      include <string.h>
#endif

#include "vmware.h"
#ifdef VMKBOOT
#include "vm_libc.h"
#endif
#if defined(VMKERNEL)
#include "vmkernel.h"
#include "vm_libc.h"
#endif /* VMKERNEL */
#include "sha1.h"
#include "vm_basic_asm.h"
#include "vmk_exports.h"


/*
 * The SSSE3 implementation is only 64-bit and it is excluded from monitor,
 * tools, vmx, workstation, and boot components, as well as Windows, Mac, and
 * FreeBSD builds. Also disabled for clang builds.
 */
#if defined(VM_X86_64) && !defined(VMCORE) && !defined(VMKBOOT) && \
    !defined(VMX86_DESKTOP) && !defined(VMX86_TOOLS) && !defined(__APPLE__) && \
    !defined(_WIN32) && !defined(__FreeBSD__) && !defined(__clang__)

#include <x86sse.h>

/*
 * In the kernel we disable preemption during SSE sections, so we limit each
 * section to 16 block updates (where each block is 64 bytes); this is on the
 * order of 1-2 microseconds.
 */
#define SHA1_SSE_BLOCKS_PER_ITERATION 16

void SHA1_Transform_SSSE3_ASM(uint32 *hash,
                              const uint8 *input,
                              uint64 numBlocks);

/*
 * Returns TRUE if we were able to use SSSE3 to apply the transform. See
 * SHA1Transform for the function semantics.
 */
static INLINE Bool
SHA1TransformSSSE3(uint32 state[5],              // IN/OUT
                   const unsigned char *buffer,  // IN
                   uint32 numBlocks)             // IN
{
   static int useSSE = -1;
   X86SSE_SaveState save;

   ASSERT(useSSE == -1 || useSSE == 0 || useSSE == 1);

   /* This is safe even if multiple threads race here. */
   if (useSSE == -1) {
      useSSE = X86SSE_IsSSSE3Supported();
   }

   if (useSSE == 0) {
      return FALSE;
   }

   /*
    * In debug mode, don't use SSE some of the time to make
    * sure the non-SSE version is tested as well.
    */
   if (vmx86_debug && RDTSC() % 101 < 20) {
      return FALSE;
   }

#ifdef VMKERNEL
   if (!INTERRUPTS_ENABLED()) {
      return FALSE;
   }
#endif

   while (numBlocks > 0) {
      uint32 blocksInIter = MIN(numBlocks, SHA1_SSE_BLOCKS_PER_ITERATION);

      X86SSE_Prologue(&save);
      SHA1_Transform_SSSE3_ASM(state, buffer, blocksInIter);
      X86SSE_Epilogue(&save);

      numBlocks -= blocksInIter;
      buffer += blocksInIter * 64;
   }

   return TRUE;
}

#else

static INLINE Bool
SHA1TransformSSSE3(uint32 state[5],
                   const unsigned char *buffer,
                   uint32 numBlocks)
{
   return FALSE;
}

#endif


/* If the endianess is not defined (it is done in string.h of glibc 2.1.1), we
   default to LE --hpreg */
#ifndef LITTLE_ENDIAN
# define LITTLE_ENDIAN /* This should be #define'd if true. */
#endif

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

#define F0(w,x,y) ((w&(x^y))^y)
#define F1(w,x,y) (w^x^y)
#define F2(w,x,y) (((w|x)&y)|(w&x))
#define F3(w,x,y) (w^x^y)

/* Initialization vectors. */
static const uint32 sha1InitVec[5] = { 0x67452301,
                                       0xEFCDAB89,
                                       0x98BADCFE,
                                       0x10325476,
                                       0xC3D2E1F0 };

typedef union {
   unsigned char c[64];
   uint32 l[16];
} CHAR64LONG16;

static void
R(CHAR64LONG16 *block, uint32 *f, int i)
{
   uint32 a, b, c, d, e, round, blk;
   a = f[0];
   b = f[1];
   c = f[2];
   d = f[3];
   e = f[4];
   f[1] = a;
   f[2] = rol(b, 30);
   f[3] = c;
   f[4] = d;
   if (i < 20) {
      round = 0x5A827999 + F0(b,c,d);
   } else if (i < 40) {
      round = 0x6ED9EBA1 + F1(b,c,d);
   } else if (i < 60) {
      round = 0x8F1BBCDC + F2(b,c,d);
   } else {
      round = 0xCA62C1D6 + F3(b,c,d);
   }
   if (i < 16) {
#ifdef LITTLE_ENDIAN
      blk = Bswap(block->l[i]);
#else
      blk = block->l[i];
#endif
   } else {
      blk = rol(block->l[(i+13) & 15] ^ block->l[(i+8) & 15] ^
                block->l[(i+2) & 15] ^ block->l[i & 15], 1);
   }
   block->l[i & 15] = blk;
   f[0] = e + round + blk + rol(a, 5);
}

/* Hash a single 512-bit block. This is the core of the algorithm. */

static void
SHA1TransformNoSSE(uint32 state[5], unsigned char buffer[64])
{
    int i;
    uint32 f[5];
    CHAR64LONG16* block = (CHAR64LONG16*)buffer;

    /* Copy context->state[] to working vars */
    for (i = 0; i < 5; i++) {
       f[i] = state[i];
    }

    for (i = 0; i < 80; i++) {
       R(block, f, i);
    }

    /* Add the working vars back into context.state[] */
    for (i = 0; i < 5; i++) {
       state[i] += f[i];
    }
}


/*
 *-----------------------------------------------------------------------------
 *
 * SHA1Transform --
 *
 *    Applies the SHA-1 transform on one or more 64-byte block buffers.
 *
 * Results:
 *    state is updated.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
SHA1Transform(uint32 state[5],              // IN/OUT
              const unsigned char *buffer,  // IN
              uint32 numBlocks)             // IN
{
    uint32 i;

    if (SHA1TransformSSSE3(state, buffer, numBlocks)) {
       return;
    }

    for (i = 0; i < numBlocks; i++) {
       unsigned char workspace[64];

       /*
        * Do not do that work in SHA1TransformNoSSE, otherwise gcc 2.7.2.3 will
        * go south and allocate a stack frame of 0x9c8 bytes, that immediately
        * leads to a stack smash and a host reset
        */
       memcpy(workspace, buffer, sizeof(workspace));
       SHA1TransformNoSSE(state, workspace);
       buffer += 64;
    }
}

/* SHA1Init - Initialize new context */

void SHA1Init(SHA1_CTX* context)
{
    uint32 i;
    for (i = 0; i < 5; i++) {
       context->state[i] = sha1InitVec[i];
    }
    context->count[0] = context->count[1] = 0;
}
VMK_KERNEL_EXPORT(SHA1Init);


/* Run your data through this. */

void SHA1Update(SHA1_CTX* context,
                const unsigned char *data,
                size_t len)
{
    size_t curOfs, numRemaining;

    /* Current offset inside the current buffer. */
    curOfs = (context->count[0] >> 3) & 63;
    if ((context->count[0] += (uint32) (len << 3)) < (len << 3))
       context->count[1]++;
    context->count[1] += (uint32) (len >> 29);

    numRemaining = 64 - curOfs;

    if (len >= numRemaining) {
        /* Complete the current buffer and update. */
        memcpy(&context->buffer[curOfs], data, numRemaining);
        SHA1Transform(context->state, context->buffer, 1);
        data += numRemaining;
        len -= numRemaining;
        curOfs = 0;

        /* Update with any complete 64-byte buffers. */
        if (len >= 64) {
           size_t numBlocks = len / 64;

           SHA1Transform(context->state, data, numBlocks);
           data += 64 * numBlocks;
           len -= 64 * numBlocks;
        }
    }

    /* Copy over whatever is left. */
    ASSERT(len + curOfs < 64);
    memcpy(&context->buffer[curOfs], data, len);
}
VMK_KERNEL_EXPORT(SHA1Update);


/* Add padding and return the message digest. */

void SHA1Final(unsigned char digest[SHA1_HASH_LEN], SHA1_CTX* context)
{
    size_t i, j;
    unsigned char finalcount[8];

    for (i = 0; i < 8; i++) {
        finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)]
         >> ((3-(i & 3)) * 8) ) & 255);  /* Endian independent */
    }
    SHA1Update(context, (unsigned char *)"\200", 1);
    while ((context->count[0] & 504) != 448) {
        SHA1Update(context, (unsigned char *)"\0", 1);
    }
    SHA1Update(context, finalcount, 8);  /* Should cause a SHA1Transform() */
    for (i = 0; i < SHA1_HASH_LEN; i++) {
        digest[i] = (unsigned char)
         ((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
    }
    /* Wipe variables */
    i = j = 0;
    memset(context->buffer, 0, 64);
    memset(context->state, 0, SHA1_HASH_LEN);
    memset(context->count, 0, 8);
    memset(&finalcount, 0, 8);
}
VMK_KERNEL_EXPORT(SHA1Final);


/*
 *-----------------------------------------------------------------------------
 *
 * SHA1RawBufferHash --
 *
 *    Finds the SHA-1 of a "raw" buffer, without doing any preprocessing (does
 *    NOT add the 0x80 byte, 0x00 padding, and length encoding required for
 *    the result to be a proper message digest).
 *
 *    Useful if the buffer already contains the preprocessed data, or if
 *    we are computing and comparing hashes for fixed-size blocks.
 *
 *    The buffer size must be a multiple of 64 bytes.
 *
 *    !! WARNING !! Do not use unless you know what you are doing! This
 *    will NOT compute the "usual" digest of the given buffer.
 *
 * Results:
 *    result contains the 120-bit SHA-1 value.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void SHA1RawBufferHash(const void *data,  // IN
                       uint32 size,       // IN
                       uint32 result[5])  // OUT
{
   uint32 i;

   ASSERT(size % 64 == 0);

   for (i = 0; i < 5; i++) {
      result[i] = sha1InitVec[i];
   }
   SHA1Transform(result, (const uint8 *) data, size / 64);
}
VMK_KERNEL_EXPORT(SHA1RawBufferHash);
