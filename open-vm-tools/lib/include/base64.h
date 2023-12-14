/*********************************************************
 * Copyright (C) 2004-2021 VMware, Inc. All rights reserved.
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
 * base64.h --
 *
 *      Functions to base64 encode/decode buffers. Implemented in
 *      lib/misc/base64.c.
 */

#ifndef _BASE64_H
#define _BASE64_H

#include "vm_basic_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

Bool Base64_Encode(uint8 const *src, size_t srcLength,
                   char *target, size_t targSize,
                   size_t *dataLength);
Bool Base64_Decode(char const *src,
                   uint8 *target, size_t targSize,
                   size_t *dataLength);
Bool Base64_ChunkDecode(char const *src, size_t inSize,
                        uint8 *target, size_t targSize,
                        size_t *dataLength);
Bool Base64_ValidEncoding(char const *src, size_t srcLength);
size_t Base64_EncodedLength(uint8 const *src, size_t srcLength);
size_t Base64_DecodedLength(char const *src, size_t srcLength);
Bool Base64_EasyEncode(const uint8 *src, size_t srcLength, char **target);
Bool Base64_EasyDecode(const char *src, uint8 **target, size_t *targSize);
Bool Base64_DecodeFixed(const char *src, char *outBuf, size_t outBufSize);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif
