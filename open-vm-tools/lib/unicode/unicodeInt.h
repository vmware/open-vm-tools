/*********************************************************
 * Copyright (C) 2007-2016 VMware, Inc. All rights reserved.
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
 * unicodeInt.h --
 *
 *      Internal functions common to all implementations of the
 *      Unicode library.
 */

#ifndef _UNICODE_INT_H
#define _UNICODE_INT_H

#include "unicodeTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

char *UnicodeAllocInternal(const void *buffer,
                           ssize_t lengthInBytes,
                           StringEncoding encoding,
                           Bool strict);

char *UnicodeAllocStatic(const char *asciiBytes,
                         Bool unescape);

utf16_t UnicodeSimpleCaseFold(utf16_t codeUnit);

void *UnicodeGetAllocBytesInternal(const char *src,
                                   StringEncoding encoding,
                                   ssize_t lengthInBytes,
                                   size_t *retLength);

Bool UnicodeSanityCheck(const void *buffer,
                        ssize_t lengthInBytes,
                        StringEncoding encoding);

#ifdef __cplusplus
}
#endif

#endif // _UNICODE_INT_H
