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
 * unicode.h --
 *
 *      Unicode-aware string library.
 */

#ifndef _UNICODE_H_
#define _UNICODE_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

// Start here: string creation, destruction, and encoding conversion.
#include "unicodeBase.h"

// Basic string operations: length, append, find, insert, replace.
#include "unicodeOperations.h"

// Character transformations: upper/lower/title case, case folding, etc.
#include "unicodeTransforms.h"

#ifdef USE_ICU
// Unicode functionality depending on the third-party ICU library.
#include "unicodeICU.h"
#endif // USE_ICU

#endif // _UNICODE_H_
