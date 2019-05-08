/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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
 * sha1.h --
 *
 *	SHA1 encryption
 */

#ifndef _SHA1_H_
#define _SHA1_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

/* for uint32 */
#include "vm_basic_types.h"

#if defined(__cplusplus)
extern "C" {
#endif


#if defined __APPLE__ && defined USERLEVEL

/*
 * Apple provides basic crypto functions in its system runtime that are
 * maintained for both speed and security. Use those instead.
 */

#include <CommonCrypto/CommonDigest.h>

#define SHA1_HASH_LEN CC_SHA1_DIGEST_LENGTH
#define SHA1_CTX      CC_SHA1_CTX
#define SHA1Init      CC_SHA1_Init
#define SHA1Update    CC_SHA1_Update
#define SHA1Final     CC_SHA1_Final

#else

#ifndef VMKERNEL

/*
 * Prevent linkage conflicts with the SHA1 APIs brought in from
 * OpenSSL. (Pro tip: If you're doing anything security-related, you
 * _must_ be using lib/crypto hash routines to preserve FIPS
 * compatibility.)
 */

#define SHA1Init             VMW_SHA1Init
#define SHA1Update           VMW_SHA1Update
#define SHA1Final            VMW_SHA1Final
#define SHA1RawBufferHash    VMW_SHA1RawBufferHash

#endif /* !VMKERNEL */

/*
SHA-1 in C
By Steve Reid <steve@edmweb.com>
100% Public Domain

Test Vectors (from FIPS PUB 180-1)
"abc"
  A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
  84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
A million repetitions of "a"
  34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
*/

/*
	12/15/98: JEB: Removed main and moved prototypes to sha1.h
			Made SHA1Transform a static function
*/

#define	SHA1_HASH_LEN	20

typedef struct SHA1_CTX {
    uint32 state[5];
    uint32 count[2];
    unsigned char buffer[64];
} SHA1_CTX;

void SHA1Init(SHA1_CTX* context);
void SHA1Update(SHA1_CTX* context,
                const unsigned char *data,
                size_t len);
void SHA1Final(unsigned char digest[SHA1_HASH_LEN], SHA1_CTX* context);

#if defined VMKBOOT || defined VMKERNEL
void SHA1RawBufferHash(const void *data,
                       uint32 size,
                       uint32 result[5]);
void SHA1RawTransformBlocks(uint32 state[5],
                            const unsigned char *buffer,
                            uint32 numBlocks);
void SHA1RawInit(uint32 state[5]);

#define SHA1_MULTI_MAX_BUFFERS 8

void SHA1MultiBuffer(uint32 numBuffers,
                     uint32 len,
                     const void *salt,
                     uint32 saltLen,
                     const void *data[],
                     unsigned char *digests[]);
#endif

#endif // defined __APPLE__ && defined USERLEVEL

#if !defined VMKBOOT && !defined VMKERNEL

/* OpenSSL opaque type for hashing. Opaque as of openssl-1.1.0. */
struct evp_md_ctx_st;

typedef struct {
#ifdef __APPLE__
   CC_SHA1_CTX cc_ctx;
#else
   struct evp_md_ctx_st *md;  /* OpenSSL EVP_MD_CTX */
#endif
} CryptoSHA1_CTX;

void CryptoSHA1_Init(CryptoSHA1_CTX *ctx);
void CryptoSHA1_Update(CryptoSHA1_CTX *ctx,
                       const unsigned char *data,
                       size_t len);
void CryptoSHA1_Final(unsigned char digest[SHA1_HASH_LEN],
                      CryptoSHA1_CTX *ctx);
#endif

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // ifndef _SHA1_H_
