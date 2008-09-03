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
 * timeutil.h --
 *
 *   Miscellaneous time related utility functions.
 *
 */

#ifndef _TIMEUTIL_H_
#define _TIMEUTIL_H_

#include <time.h>

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vm_assert.h"


#define MAX_DAYSLEFT     1024

struct timeval;


typedef struct TimeUtil_Date {
   unsigned int year;
   unsigned int month;
   unsigned int day;
   unsigned int hour;
   unsigned int minute;
   unsigned int second;
} TimeUtil_Date;


typedef struct TimeUtil_Expiration {
   /*
    * Does it expire?
    */

   Bool expires;

   /*
    * When does it expire? (valid only if 'expires' == TRUE)
    *
    * Note: TimeUtil_Expiration only uses the 'year', 'month'
    * and 'day' fields of 'when'.
    */

   TimeUtil_Date when;

   /*
    * Compute this once for all, to avoid problems when the current day changes
    * (valid only if 'expires' == TRUE).
    */

   unsigned int daysLeft;
} TimeUtil_Expiration;


EXTERN Bool TimeUtil_StringToDate(TimeUtil_Date *d,    // IN/OUT
                                  const char *date);   // IN: 'YYYYMMDD' or 'YYYY/MM/DD' or 'YYYY-MM-DD'

EXTERN Bool TimeUtil_DaysSubstract(TimeUtil_Date *d,  // IN/OUT
                                   unsigned int nr);  // IN

EXTERN int TimeUtil_DeltaDays(TimeUtil_Date *left,   // IN
                              TimeUtil_Date *right); // IN

EXTERN void TimeUtil_DaysAdd(TimeUtil_Date *d, // IN/OUT
                             unsigned int nr); // IN

EXTERN void TimeUtil_PopulateWithCurrent(Bool local,        // IN
                                         TimeUtil_Date *d); // OUT

EXTERN unsigned int TimeUtil_DaysLeft(TimeUtil_Date const *d); // IN

EXTERN Bool TimeUtil_ExpirationLowerThan(TimeUtil_Expiration const *left,   // IN
                                         TimeUtil_Expiration const *right); // IN

EXTERN Bool TimeUtil_DateLowerThan(TimeUtil_Date const *left,   // IN
                                   TimeUtil_Date const *right); // IN

EXTERN void TimeUtil_ProductExpiration(TimeUtil_Expiration *e); // OUT

EXTERN char * TimeUtil_GetTimeFormat(int64 utcTime,  // IN
                                     Bool showDate,  // IN
                                     Bool showTime); // IN

#if !defined _WIN32 && !defined N_PLAT_NLM
EXTERN int TimeUtil_NtTimeToUnixTime(struct timespec *unixTime, // OUT
                                     VmTimeType ntTime);        // IN
EXTERN VmTimeType TimeUtil_UnixTimeToNtTime(struct timespec unixTime); // IN
#endif


#ifdef _WIN32
EXTERN Bool TimeUtil_UTCTimeToSystemTime(const __time64_t utcTime,    // IN
                                         SYSTEMTIME *systemTime);     // OUT
#endif

EXTERN int TimeUtil_GetLocalWindowsTimeZoneIndex(void);

#endif // _TIMEUTIL_H_
