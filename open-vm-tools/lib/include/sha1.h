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

typedef struct {
    uint32 state[5];
    uint32 count[2];
    unsigned char buffer[64];
} SHA1_CTX;

#define SHA1HANDSOFF /* Copies data before messing with it. */

void SHA1Init(SHA1_CTX* context);
#ifdef SHA1HANDSOFF
void SHA1Update(SHA1_CTX* context,
                const unsigned char *data,
                size_t len);
#else
void SHA1Update(SHA1_CTX* context,
                unsigned char *data,
                size_t len);
#endif
void SHA1Final(unsigned char digest[SHA1_HASH_LEN], SHA1_CTX* context);

void SHA1Transform(uint32 state[5], const unsigned char buffer[64]);

#endif // ifndef _SHA1_H_
