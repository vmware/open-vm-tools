/*********************************************************
 * Copyright (C) 2010-2017 VMware, Inc. All rights reserved.
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

#ifndef _LOGFIXED_H_
#define _LOGFIXED_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 *  LogFixed_Base2 and LogFixed_Base10 provide their values expressed
 *  as a ration of two uint32 numbers with an accuracy of better than 1%.
 *
 * Reminder: A log, base x, of zero is undefined. These routines will assert
 * in development builds when a zero value is passed to them.
 */

void LogFixed_Base2(uint64 value,
                    uint32 *numerator,
                    uint32 *denominator);

void LogFixed_Base10(uint64 value,
                    uint32 *numerator,
                    uint32 *denominator);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // _LOGFIXED_H_
