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
 * random.h --
 *
 *    Random bits generation. Please use CryptoRandom_GetBytes if
 *    you require a FIPS-compliant source of random data.
 */

#ifndef __RANDOM_H__
#   define __RANDOM_H__


#include "vm_basic_types.h"

Bool
Random_Crypto(unsigned int size, // IN
              void *buffer);     // OUT

/*
 * High quality - research grade - random number generator.
 *
 * Despite its apparent complexity this RNG is extremely fast.
 */

typedef struct rqContext rqContext;

rqContext *
Random_QuickSeed(uint32 seed);

uint32
Random_Quick(rqContext *context);

/*
 * Simple multiplicative conguential RNG.
 */

int
Random_Simple(int seed);

#endif /* __RANDOM_H__ */
