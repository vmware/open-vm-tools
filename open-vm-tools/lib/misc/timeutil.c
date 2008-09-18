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
 * timeutil.c --
 *
 *   Miscellaneous time related utility functions.
 */


#include "safetime.h"

#if defined(N_PLAT_NLM)
#  include <sys/timeval.h>
#  include <nwtime.h>
#elif defined(_WIN32)
#  include <wtypes.h>
#else
#  include <sys/time.h>
#endif
#include <ctype.h>

#include "vmware.h"
/* For HARD_EXPIRE --hpreg */
#include "vm_version.h"
#include "vm_basic_asm.h"
#include "timeutil.h"
#include "str.h"
#include "util.h"
#ifdef _WIN32
#include "win32u.h"
#endif


/*
 * NT time of the Unix epoch:
 * midnight January 1, 1970 UTC
 */
#define UNIX_EPOCH ((((uint64)369 * 365) + 89) * 24 * 3600 * 10000000)

/*
 * NT time of the Unix 32 bit signed time_t wraparound:
 * 03:14:07 January 19, 2038 UTC
 */
#define UNIX_S32_MAX (UNIX_EPOCH + (uint64)0x80000000 * 10000000)

/*
 * Days in month defines. The month is 1 - 12.
 * Notice that there are 28 days in february. It will updated for leap year
 * as necessary by the function using this defines.
 */
#define DAYS_IN_MONTH { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }


/*
 * Local Definitions
 */

static void TimeUtilInit(TimeUtil_Date *d);
static Bool TimeUtilLoadDate(TimeUtil_Date *d, const char *date);
static Bool TimeUtilIsLeapYear(unsigned int year);
static Bool TimeUtilIsValidDate(unsigned int year, unsigned int month, unsigned int day);


/*
 * Function to guess Windows TZ Index by using time offset in
 * a lookup table
 */
static int TimeUtilFindIndexByUTCOffset(int utcToStdOffsetMins);

#if defined(_WIN32)
/*
 * Function to find Windows TZ Index by scanning registry
 */
static int Win32TimeUtilLookupZoneIndex(const char* targetName);
#endif


/*
 * Local substitution for localtime_r() for those platforms
 * that don't have one.
 */
#ifdef SOL9
#include <synch.h>
static mutex_t LT_MUTEX = DEFAULTMUTEX;
static struct tm* localtime_r(time_t* secs, struct tm* tp)
{
   if (!secs || !tp) {
      return NULL;
   }
   mutex_lock(&LT_MUTEX);
   memcpy(tp, localtime(secs), sizeof *tp);
   mutex_unlock(&LT_MUTEX);
   return tp;
}
#endif



/*
 *----------------------------------------------------------------------
 *
 * TimeUtil_StringToDate --
 *
 *    Initialize the date object with value from the string argument,
 *    while the time will be left unmodified.
 *    The string 'date' needs to be in the format of 'YYYYMMDD' or
 *    'YYYY/MM/DD' or 'YYYY-MM-DD'.
 *    Unsuccesful initialization will leave the 'd' argument unmodified.
 *
 * Results:
 *    TRUE or FALSE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

Bool
TimeUtil_StringToDate(TimeUtil_Date *d,  // IN/OUT
                      const char *date)  // IN
{
   /*
    * Reduce the string to a known and handled format: YYYYMMDD.
    * Then, passed to internal function TimeUtilLoadDate.
    */
   if (strlen(date) == 8) {
      /* 'YYYYMMDD' */
      return TimeUtilLoadDate(d, date);
   } else if (strlen(date) == 10) {
      /* 'YYYY/MM/DD' */
      char temp[16] = { 0 };

      if (!(((date[4] != '/') || (date[7] != '/')) ||
           ((date[4] != '-') || (date[7] != '-')))) {
         return FALSE;
      }

      Str_Strcpy(temp, date, sizeof(temp));
      temp[4] = date[5];
      temp[5] = date[6];
      temp[6] = date[8];
      temp[7] = date[9];
      temp[8] = '\0';

      return TimeUtilLoadDate(d, temp);
   } else {
      return FALSE;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * TimeUtil_DeltaDays --
 *
 *    Seach for number of days differences between the two date
 *    arguments.
 *    This function is ignoring the time. It will be as if the time
 *    is midnight (00:00:00).
 *
 * Results:
 *    number of days:
 *    - 0 (if 'left' and 'right' are of the same date (ignoring the time).
 *    - negative, if 'left' is of a later date than 'right'
 *    - positive, if 'right' is of a later date than 'left'
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

int
TimeUtil_DeltaDays(TimeUtil_Date *left,  // IN
                   TimeUtil_Date *right) // IN
{
   TimeUtil_Date temp1;
   TimeUtil_Date temp2;
   TimeUtil_Date temp;

   int days = 0;
   Bool inverted = FALSE;

   ASSERT(left);
   ASSERT(right);
   ASSERT(TimeUtilIsValidDate(left->year, left->month, left->day));
   ASSERT(TimeUtilIsValidDate(right->year, right->month, right->day));

   TimeUtilInit(&temp1);
   TimeUtilInit(&temp2);
   TimeUtilInit(&temp);

   temp1.year = left->year;
   temp1.month = left->month;
   temp1.day = left->day;
   temp2.year = right->year;
   temp2.month = right->month;
   temp2.day = right->day;

   if (!TimeUtil_DateLowerThan(&temp1, &temp2) &&
       !TimeUtil_DateLowerThan(&temp2, &temp1)) {
      return 0;
   } else if (TimeUtil_DateLowerThan(&temp1, &temp2)) {
      inverted = FALSE;
   } else if (TimeUtil_DateLowerThan(&temp2, &temp1)) {
      inverted = TRUE;
      temp = temp1;
      temp1 = temp2;
      temp2 = temp;
   }

   days = 1;
   TimeUtil_DaysAdd(&temp1, 1);
   while (TimeUtil_DateLowerThan(&temp1, &temp2)) {
      days++;
      TimeUtil_DaysAdd(&temp1, 1);
   }

   if (inverted) {
      return -days;
   } else {
      return days;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * TimeUtil_DaysSubstract --
 *
 *    substracts 'nr' days from 'd'.
 *
 *    Simple algorithm - which can be improved as necessary:
 *    - get rough days estimation, also guarantee that the estimation is
 *      lower than the actual result.
 *    - 'add' a day-by-day to arrive at actual result.
 *    'd' will be unchanged if the function failed.
 *
 * TODO:
 *    This function can be combined with DaysAdd(), where it
 *    accepts integer (positive for add, negative for substract).
 *    But, that cannot be done without changing the DaysAdd function
 *    signature.
 *    When this utility get rewritten, this can be updated.
 *
 * Results:
 *    TRUE or FALSE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

Bool
TimeUtil_DaysSubstract(TimeUtil_Date *d,   // IN
                       unsigned int nr)    // IN
{
   TimeUtil_Date temp;
   int subYear = 0;
   int subMonth = 0;
   int subDay = 0;

   TimeUtil_Date estRes;
   int estYear = 0;
   int estMonth = 0;
   int estDay = 0;

   unsigned int dayCount = nr;

   ASSERT(d);

   TimeUtilInit(&temp);
   TimeUtilInit(&estRes);

   /*
    * Use lower bound for the following conversion:
    * 365 (instead of 366) days in a year
    * 30 (instead of 31) days in a month.
    *
    *   To account that feb has fewer #days than 30, we will
    *   intentionally substract an additional 2 days for each year
    *   and an additional 3 days.
    */
   dayCount = dayCount + 3 + 2 * (dayCount / 365);

   subYear = dayCount / 365;
   dayCount = dayCount % 365;
   subMonth = dayCount / 30;
   subDay = dayCount % 30;

   estDay = d->day - subDay;
   while (estDay <= 0) {
      estDay = estDay + 30;
      subMonth++;
   }
   estMonth = d->month - subMonth;
   while (estMonth <= 0) {
      estMonth = estMonth + 12;
      subYear++;
   }
   estYear = d->year - subYear;
   if (estYear <= 0) {
      return FALSE;
   }

   /*
    * making sure on the valid range, without checking
    * for leap year, etc.
    */
   if ((estDay > 28) && (estMonth == 2)) {
      estDay = 28;
   }

   estRes.year = estYear;
   estRes.month = estMonth;
   estRes.day = estDay;

   /*
    * we also copy the time from the original argument in making
    * sure that it does not play role in the comparison.
    */
   estRes.hour = d->hour;
   estRes.minute = d->minute;
   estRes.second = d->second;

   /*
    * At this point, we should have an estimated result which
    * guaranteed to be lower than the actual result. Otherwise,
    * infinite loop will happen.
    */
   ASSERT(TimeUtil_DateLowerThan(&estRes, d));

   /*
    * Perform the actual precise adjustment
    * Done by moving up (moving forward) the estimated a day at a time
    *    until they are the correct one (i.e. estDate + arg #day = arg date)
    */
   temp = estRes;
   TimeUtil_DaysAdd(&temp, nr);
   while (TimeUtil_DateLowerThan(&temp, d)) {
      TimeUtil_DaysAdd(&temp, 1);
      TimeUtil_DaysAdd(&estRes, 1);
   }

   d->year = estRes.year;
   d->month = estRes.month;
   d->day = estRes.day;

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * TimeUtil_DaysAdd --
 *
 *    Add 'nr' days to a date.
 *    This function can be optimized a lot if needed.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
TimeUtil_DaysAdd(TimeUtil_Date *d, // IN/OUT
                 unsigned int nr)  // IN
{
   static unsigned int monthdays[13] = DAYS_IN_MONTH;
   unsigned int i;

   /*
    * Initialize the table
    */

   if (TimeUtilIsLeapYear(d->year)) {
      monthdays[2] = 29;
   } else {
      monthdays[2] = 28;
   }

   for (i = 0; i < nr; i++) {
      /*
       * Add 1 day to the date
       */

      d->day++;
      if (d->day > monthdays[d->month]) {
         d->day = 1;
         d->month++;
         if (d->month > 12) {
            d->month = 1;
            d->year++;

            /*
             * Update the table
             */

            if (TimeUtilIsLeapYear(d->year)) {
               monthdays[2] = 29;
            } else {
               monthdays[2] = 28;
            }
         }
      }
   }
}


/*
 *----------------------------------------------------------------------
 *
 * TimeUtil_PopulateWithCurrent --
 *
 *    Populate the given date object with the current date and time.
 *
 *    If 'local' is TRUE, the time will be expressed in the local time
 *    zone. Otherwise, the time will be expressed in UTC.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
TimeUtil_PopulateWithCurrent(Bool local,       // IN
                             TimeUtil_Date *d) // OUT
{
#ifdef _WIN32
   SYSTEMTIME currentTime;

   ASSERT(d);

   if (local) {
      GetLocalTime(&currentTime);
   } else {
      GetSystemTime(&currentTime);
   }
   d->year   = currentTime.wYear;
   d->month  = currentTime.wMonth;
   d->day    = currentTime.wDay;
   d->hour   = currentTime.wHour;
   d->minute = currentTime.wMinute;
   d->second = currentTime.wSecond;
#else
   struct tm *currentTime;
   time_t utcTime;

   ASSERT(d);

   utcTime = time(NULL);
   if (local) {
      currentTime = localtime(&utcTime);
   } else {
      currentTime = gmtime(&utcTime);
   }
   ASSERT_NOT_IMPLEMENTED(currentTime);
   d->year   = 1900 + currentTime->tm_year;
   d->month  = currentTime->tm_mon + 1;
   d->day    = currentTime->tm_mday;
   d->hour   = currentTime->tm_hour;
   d->minute = currentTime->tm_min;
   d->second = currentTime->tm_sec;
#endif // _WIN32
}


/*
 *----------------------------------------------------------------------
 *
 * TimeUtil_DaysLeft --
 *
 *    Computes the number of days left before a given date
 *
 * Results:
 *    0: the given date is in the past
 *    1 to MAX_DAYSLEFT: if there are 1 to MAX_DAYSLEFT days left
 *    MAX_DAYSLEFT+1 if there are more than MAX_DAYSLEFT days left
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

unsigned int
TimeUtil_DaysLeft(TimeUtil_Date const *d) // IN
{
   TimeUtil_Date c;
   unsigned int i;

   /* Get the current local date. */
   TimeUtil_PopulateWithCurrent(TRUE, &c);

   /* Compute how many days we can add to the current date before reaching
      the given date */
   for (i = 0; i < MAX_DAYSLEFT + 1; i++) {
      if (    c.year > d->year
          || (c.year == d->year && c.month > d->month)
          || (c.year == d->year && c.month == d->month && c.day >= d->day)) {
         /* current date >= given date */
         return i;
      }

      TimeUtil_DaysAdd(&c, 1);
   }

   /* There are at least MAX_DAYSLEFT+1 days left */
   return MAX_DAYSLEFT + 1;
}


/*
 *----------------------------------------------------------------------
 *
 * TimeUtil_ExpirationLowerThan --
 *
 *    Determine if 'left' is lower than 'right'
 *
 * Results:
 *    TRUE if yes
 *    FALSE if no
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

Bool
TimeUtil_ExpirationLowerThan(TimeUtil_Expiration const *left,  // IN
                             TimeUtil_Expiration const *right) // IN
{
   if (left->expires == FALSE) {
      return FALSE;
   }

   if (right->expires == FALSE) {
      return TRUE;
   }

   if (left->when.year < right->when.year) {
      return TRUE;
   }

   if (left->when.year > right->when.year) {
      return FALSE;
   }

   if (left->when.month < right->when.month) {
      return TRUE;
   }

   if (left->when.month > right->when.month) {
      return FALSE;
   }

   if (left->when.day < right->when.day) {
      return TRUE;
   }

   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * TimeUtil_DateLowerThan --
 *
 *    Determine if 'left' is lower than 'right'
 *
 * Results:
 *    TRUE if yes
 *    FALSE if no
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

Bool
TimeUtil_DateLowerThan(TimeUtil_Date const *left,  // IN
                       TimeUtil_Date const *right) // IN
{
   ASSERT(left);
   ASSERT(right);

   if (left->year < right->year) {
      return TRUE;
   }

   if (left->year > right->year) {
      return FALSE;
   }

   if (left->month < right->month) {
      return TRUE;
   }

   if (left->month > right->month) {
      return FALSE;
   }

   if (left->day < right->day) {
      return TRUE;
   }

   if (left->day > right->day) {
      return FALSE;
   }

   if (left->hour < right->hour) {
      return TRUE;
   }

   if (left->hour > right->hour) {
      return FALSE;
   }

   if (left->minute < right->minute) {
      return TRUE;
   }

   if (left->minute > right->minute) {
      return FALSE;
   }

   if (left->second < right->second) {
      return TRUE;
   }

   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * TimeUtil_ProductExpiration --
 *
 *    Retrieve the expiration information associated to the product in 'e'
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
TimeUtil_ProductExpiration(TimeUtil_Expiration *e) // OUT
{

   /*
    * The hard_expire string is used by post-build processing scripts to
    * determine if a build is set to expire or not.
    */
#ifdef HARD_EXPIRE
   static char *hard_expire = "Expire";
   (void)hard_expire;

   ASSERT(e);

   e->expires = TRUE;

   /*
    * Decode the hard-coded product expiration date.
    */

   e->when.day = HARD_EXPIRE;
   e->when.year = e->when.day / ((DATE_MONTH_MAX + 1) * (DATE_DAY_MAX + 1));
   e->when.day -= e->when.year * ((DATE_MONTH_MAX + 1) * (DATE_DAY_MAX + 1));
   e->when.month = e->when.day / (DATE_DAY_MAX + 1);
   e->when.day -= e->when.month * (DATE_DAY_MAX + 1);

   e->daysLeft = TimeUtil_DaysLeft(&e->when);
#else
   static char *hard_expire = "No Expire";
   (void)hard_expire;

   ASSERT(e);

   e->expires = FALSE;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * TimeUtil_GetTimeFormat --
 *
 *    Converts a UTC time value to a human-readable string.
 *
 * Results:
 *    Returns the a formatted string of the given UTC time.  It is the
 *    caller's responsibility to free this string.  May return NULL.
 *
 *    If Win32, the time will be formatted according to the current
 *    locale.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

char *
TimeUtil_GetTimeFormat(int64 utcTime,  // IN
                       Bool showDate,  // IN
                       Bool showTime)  // IN
{
#ifdef _WIN32
   SYSTEMTIME systemTime = { 0 };
   char dateStr[100] = "";
   char timeStr[100] = "";

   if (!showDate && !showTime) {
      return NULL;
   }

   if (!TimeUtil_UTCTimeToSystemTime((const __time64_t) utcTime, &systemTime)) {
      return NULL;
   }

   Win32U_GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE,
                        &systemTime, NULL, dateStr, ARRAYSIZE(dateStr));

   Win32U_GetTimeFormat(LOCALE_USER_DEFAULT, 0, &systemTime, NULL,
                        timeStr, ARRAYSIZE(timeStr));

   if (showDate && showTime) {
      return Str_Asprintf(NULL, "%s %s", dateStr, timeStr);
   } else {
      return Str_Asprintf(NULL, "%s", showDate ? dateStr : timeStr);
   }

#else
   char *str;
   str = Util_SafeStrdup(ctime((const time_t *) &utcTime));
   str[strlen(str)-1] = '\0';
   return str;
#endif // _WIN32
}

#if !defined _WIN32 && !defined N_PLAT_NLM
/*
 *-----------------------------------------------------------------------------
 *
 * TimeUtil_NtTimeToUnixTime --
 *
 *    Convert from Windows NT time to Unix time. If NT time is outside of
 *    Unix time range (1970-2038), returned time is nearest time valid in
 *    Unix.
 *
 * Results:
 *    0        on success
 *    non-zero if NT time is outside of valid range for UNIX
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
TimeUtil_NtTimeToUnixTime(struct timespec *unixTime,   // OUT: Time in Unix format
                          VmTimeType ntTime)           // IN: Time in Windows NT format
{
#ifndef VM_X86_64
   uint32 sec;
   uint32 nsec;

   ASSERT(unixTime);
   /* We assume that time_t is 32bit */
   ASSERT(sizeof (unixTime->tv_sec) == 4);

   /* Cap NT time values that are outside of Unix time's range */

   if (ntTime >= UNIX_S32_MAX) {
      unixTime->tv_sec = 0x7FFFFFFF;
      unixTime->tv_nsec = 0;
      return 1;
   }
#else
   ASSERT(unixTime);
#endif // VM_X86_64

   if (ntTime < UNIX_EPOCH) {
      unixTime->tv_sec = 0;
      unixTime->tv_nsec = 0;
      return -1;
   }

#ifndef VM_X86_64
   Div643232(ntTime - UNIX_EPOCH, 10000000, &sec, &nsec);
   unixTime->tv_sec = sec;
   unixTime->tv_nsec = nsec * 100;
#else
   unixTime->tv_sec = (ntTime - UNIX_EPOCH) / 10000000;
   unixTime->tv_nsec = ((ntTime - UNIX_EPOCH) % 10000000) * 100;
#endif // VM_X86_64

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * TimeUtil_UnixTimeToNtTime --
 *
 *    Convert from Unix time to Windows NT time.
 *
 * Results:
 *    The time in Windows NT format.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VmTimeType
TimeUtil_UnixTimeToNtTime(struct timespec unixTime) // IN: Time in Unix format
{
   return (VmTimeType)unixTime.tv_sec * 10000000 +
      unixTime.tv_nsec / 100 + UNIX_EPOCH;
}
#endif // _WIN32 && N_PLAT_NLM

#ifdef _WIN32
/*
 *----------------------------------------------------------------------
 *
 * TimeUtil_UTCTimeToSystemTime --
 *
 *    Converts the time from UTC time to SYSTEMTIME
 *
 * Results:
 *    TRUE if the time was converted successfully, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

Bool
TimeUtil_UTCTimeToSystemTime(const __time64_t utcTime,   // IN
                             SYSTEMTIME *systemTime)     // OUT
{
   int atmYear;
   int atmMonth;

   struct tm *atm;

   /*
    * _localtime64 support years up through 3000.  At least it says
    * so.  I'm getting garbage only after reaching year 4408.
    */
   if (utcTime < 0 || utcTime > (60LL * 60 * 24 * 365 * (3000 - 1970))) {
      return FALSE;
   }

   atm = _localtime64(&utcTime);
   if (atm == NULL) {
      return FALSE;
   }

   atmYear = atm->tm_year + 1900;
   atmMonth = atm->tm_mon + 1;

   /*
    * Windows's SYSTEMTIME documentation says that these are limits...
    * Main reason for this test is to cut out negative values _localtime64
    * likes to return for some inputs.
    */
   if (atmYear < 1601 || atmYear > 30827 ||
       atmMonth < 1 || atmMonth > 12 ||
       atm->tm_wday < 0 || atm->tm_wday > 6 ||
       atm->tm_mday < 1 || atm->tm_mday > 31 ||
       atm->tm_hour < 0 || atm->tm_hour > 23 ||
       atm->tm_min < 0 || atm->tm_min > 59 ||
       /* Allow leap second, just in case... */
       atm->tm_sec < 0 || atm->tm_sec > 60) {
      return FALSE;
   }

   systemTime->wYear         = (WORD) atmYear;
   systemTime->wMonth        = (WORD) atmMonth;
   systemTime->wDayOfWeek    = (WORD) atm->tm_wday;
   systemTime->wDay          = (WORD) atm->tm_mday;
   systemTime->wHour         = (WORD) atm->tm_hour;
   systemTime->wMinute       = (WORD) atm->tm_min;
   systemTime->wSecond       = (WORD) atm->tm_sec;
   systemTime->wMilliseconds = 0;

   return TRUE;
}
#endif // _WIN32


/*
 *----------------------------------------------------------------------
 *
 * TimeUtil_GetLocalWindowsTimeZoneIndex --
 *
 *    Gets Windows TZ index for local time zone.
 *
 * Results:
 *    -1 if there is any error, else the Windows Time Zone ID of the
 *    current timezone (non-negative value).
 *
 * Side effects:
 *    On non-Win32 platforms, calls localtime_r() which sets globals
 *    variables (e.g. 'timezone' and 'tzname' for Linux)
 *
 *----------------------------------------------------------------------
 */
int
TimeUtil_GetLocalWindowsTimeZoneIndex(void)
{
   int utcStdOffMins = 0;
   int winTimeZoneIndex = (-1);

#if defined(_WIN32)

   TIME_ZONE_INFORMATION tz;
   char name[256] = { 0 };
   size_t nameLen = 0, nc = 0;
   if (GetTimeZoneInformation(&tz) == TIME_ZONE_ID_INVALID) {
      return (-1);
   }

   /* 'Bias' = diff between UTC and local standard time */
   utcStdOffMins = 0-tz.Bias; // already in minutes

   /* Get TZ name */
   nameLen = wcslen(tz.StandardName);
   if (wcstombs(name, tz.StandardName, 255) <= 0) {
         return (-1);
   }

   /* Convert name to Windows TZ index */
   winTimeZoneIndex = Win32TimeUtilLookupZoneIndex(name);

#else // NOT _WIN32

   /*
    * Use localtime_r() to get offset between our local
    * time and UTC. This varies by platform. Also, the structure
    * fields are named "*gmt*" but the man pages claim offsets are
    * to UTC, not GMT.
    */

   time_t now = time(NULL);
   struct tm tim;
   localtime_r(&now, &tim);

   #if defined SOL9 || defined SOL10 || defined N_PLAT_NLM
      /*
       * Offset is to standard (no need for DST adjustment).
       * Negative is east of prime meridian.
       */
      utcStdOffMins = 0 - timezone/60;
   #else
      /*
       * FreeBSD, Apple, Linux only:
       * Offset is to local (need to adjust for DST).
       * Negative is west of prime meridian.
       */
      utcStdOffMins = tim.tm_gmtoff/60;
      if (tim.tm_isdst) {
         utcStdOffMins -= 60;
      }
   #endif

   /* can't figure this out directly for non-Win32 */
   winTimeZoneIndex = (-1);

#endif

   /* If we don't have it yet, look up windowsCode. */
   if (winTimeZoneIndex < 0) {
      winTimeZoneIndex = TimeUtilFindIndexByUTCOffset(utcStdOffMins);
   }

   return winTimeZoneIndex;
}


/*
 ***********************************************************************
 *
 * Local Functions
 *
 ***********************************************************************
 */

/*
 *----------------------------------------------------------------------
 *
 * TimeUtilInit --
 *
 *    Initialize everything to zero
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static void
TimeUtilInit(TimeUtil_Date *d)
{
   ASSERT(d);

   d->year = 0;
   d->month = 0;
   d->day = 0;
   d->hour = 0;
   d->minute = 0;
   d->second = 0;

   return;
}


/*
 *----------------------------------------------------------------------
 *
 * TimeUtilIsValidDate --
 *
 *    Check whether the args represent a valid date.
 *
 * Results:
 *    TRUE or FALSE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static Bool
TimeUtilIsValidDate(unsigned int year,   // IN
                    unsigned int month,  // IN
                    unsigned int day)    // IN
{
   static unsigned int monthdays[13] = DAYS_IN_MONTH;

   /*
    * Initialize the table
    */
   if (TimeUtilIsLeapYear(year)) {
      monthdays[2] = 29;
   } else {
      monthdays[2] = 28;
   }

   if ((year >= 1) &&
       (month >= 1) && (month <= 12) &&
       (day >= 1) && (day <= monthdays[month])) {
      return TRUE;
   }

   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * TimeUtilIsLeapYear --
 *
 *    Check whether the argument represents a leap year.
 *
 * Results:
 *    TRUE or FALSE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static Bool
TimeUtilIsLeapYear(unsigned int year)  // IN
{
   if ((year % 4) == 0 &&
       ((year % 100) != 0 ||
        (year % 400) == 0)) {
      return TRUE;
   }

   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * TimeUtilLoadDate --
 *
 *    Initialize the date object with value from the string argument,
 *    while the time will be left unmodified.
 *    The string 'date' needs to be in the format of 'YYYYMMDD'.
 *    Unsuccesful initialization will leave the 'd' argument unmodified.
 *
 * Results:
 *    TRUE or FALSE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static Bool
TimeUtilLoadDate(TimeUtil_Date *d,  // IN/OUT
                 const char *date)  // IN
{
   char temp[16] = { 0 };
   int i = 0;
   char *end = NULL;

   int32 year = 0;
   int32 month = 0;
   int32 day = 0;

   ASSERT(d);
   ASSERT(date);

   if (strlen(date) != 8) {
      return FALSE;
   }
   for (i = 0; i < strlen(date); i++) {
      if (isdigit((int) date[i]) == 0) {
         return FALSE;
      }
   }

   temp[0] = date[0];
   temp[1] = date[1];
   temp[2] = date[2];
   temp[3] = date[3];
   temp[4] = '\0';
   year = strtol(temp, &end, 10);
   if (*end != '\0') {
      return FALSE;
   }

   temp[0] = date[4];
   temp[1] = date[5];
   temp[2] = '\0';
   month = strtol(temp, &end, 10);
   if (*end != '\0') {
      return FALSE;
   }

   temp[0] = date[6];
   temp[1] = date[7];
   temp[2] = '\0';
   day = strtol(temp, &end, 10);
   if (*end != '\0') {
      return FALSE;
   }

   if (!TimeUtilIsValidDate((unsigned int) year, (unsigned int) month, (unsigned int) day)) {
      return FALSE;
   }

   d->year = (unsigned int) year;
   d->month = (unsigned int) month;
   d->day = (unsigned int) day;

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * TimeUtilFindIndexByUTCOffset --
 *
 *    Private function. Scans a table for a given UTC-to-Standard
 *    offset and returns the Windows TZ Index of the first match
 *    found.
 *
 * Parameters:
 *    utcStdOffMins   Offset to look for (in minutes)
 *                    <0 is west of PM, >0 is east of PM.
 *
 * Results:
 *    Returns Windows TZ Index (>=0) if found, else -1.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int TimeUtilFindIndexByUTCOffset(int utcStdOffMins)
{
   static struct _tzinfo {
      int winTzIndex;
      int utcStdOffMins;
   } TABLE[] = {
      {   0, /* "Dateline Standard Time",*/        -720 }, // -12
      {   1, /* "Samoa Standard Time",*/           -660 }, // -11
      {   2, /* "Hawaiian Standard Time",*/        -600 }, // -10
      {   3, /* "Alaskan Standard Time",*/         -540 }, // -9
      {   4, /* "Pacific Standard Time",*/         -480 }, // -8
      {  10, /* "Mountain Standard Time",*/        -420 }, // -7
      {  13, /* "Mexico Standard Time 2",*/        -420 }, // -7
      {  15, /* "U.S. Mountain Standard Time",*/   -420 }, // -7
      {  20, /* "Central Standard Time",*/         -360 }, // -6
      {  25, /* "Canada Central Standard Time",*/  -360 }, // -6
      {  30, /* "Mexico Standard Time",*/          -360 }, // -6
      {  33, /* "Central America Standard Time",*/ -360 }, // -6
      {  35, /* "Eastern Standard Time",*/         -300 }, // -5
      {  40, /* "U.S. Eastern Standard Time",*/    -300 }, // -5
      {  45, /* "S.A. Pacific Standard Time",*/    -300 }, // -5
      {  50, /* "Atlantic Standard Time",*/        -240 }, // -4
      {  55, /* "S.A. Western Standard Time",*/    -240 }, // -4
      {  56, /* "Pacific S.A. Standard Time",*/    -240 }, // -4
      {  60, /* "Newfoundland Standard Time",*/    -210 }, // -3.5
      {  65, /* "E. South America Standard Time",*/-180 }, // -3
      {  70, /* "S.A. Eastern Standard Time",*/    -180 }, // -3
      {  73, /* "Greenland Standard Time",*/       -180 }, // -3
      {  75, /* "Mid-Atlantic Standard Time",*/    -120 }, // -2
      {  80, /* "Azores Standard Time",*/           -60 }, // -1
      {  83, /* "Cape Verde Standard Time",*/       -60 }, // -1
      {  85, /* "GMT Standard Time",*/                0 }, // 0
      {  90, /* "Greenwich Standard Time",*/          0 }, // 0
      {  95, /* "Central Europe Standard Time",*/    60 }, // +1
      { 100, /* "Central European Standard Time",*/  60 }, // +1
      { 105, /* "Romance Standard Time",*/           60 }, // +1
      { 110, /* "W. Europe Standard Time",*/         60 }, // +1
      { 113, /* "W. Central Africa Standard Time",*/ 60 }, // +1
      { 115, /* "E. Europe Standard Time",*/        120 }, // +2
      { 120, /* "Egypt Standard Time",*/            120 }, // +2
      { 125, /* "FLE Standard Time",*/              120 }, // +2
      { 130, /* "GTB Standard Time",*/              120 }, // +2
      { 135, /* "Israel Standard Time",*/           120 }, // +2
      { 140, /* "South Africa Standard Time",*/     120 }, // +2
      { 145, /* "Russian Standard Time",*/          180 }, // +3
      { 150, /* "Arab Standard Time",*/             180 }, // +3
      { 155, /* "E. Africa Standard Time",*/        180 }, // +3
      { 158, /* "Arabic Standard Time",*/           180 }, // +3
      { 160, /* "Iran Standard Time",*/             210 }, // +3.5
      { 165, /* "Arabian Standard Time",*/          240 }, // +4
      { 170, /* "Caucasus Standard Time",*/         240 }, // +4
      { 175, /* "Afghanistan Standard Time",*/      270 }, // +4.5
      { 180, /* "Ekaterinburg Standard Time",*/     300 }, // +5
      { 185, /* "West Asia Standard Time",*/        300 }, // +5
      { 190, /* "India Standard Time",*/            330 }, // +5.5
      { 193, /* "Nepal Standard Time",*/            345 }, // +5.75
      { 195, /* "Central Asia Standard Time",*/     360 }, // +6
      { 200, /* "Sri Lanka Standard Time",*/        360 }, // +6
      { 201, /* "N. Central Asia Standard Time",*/  360 }, // +6
      { 203, /* "Myanmar Standard Time",*/          390 }, // +6.5
      { 205, /* "S.E. Asia Standard Time",*/        420 }, // +7
      { 207, /* "North Asia Standard Time",*/       420 }, // +7
      { 210, /* "China Standard Time",*/            480 }, // +8
      { 215, /* "Singapore Standard Time",*/        480 }, // +8
      { 220, /* "Taipei Standard Time",*/           480 }, // +8
      { 225, /* "W. Australia Standard Time",*/     480 }, // +8
      { 227, /* "North Asia East Standard Time",*/  480 }, // +8
      { 230, /* "Korea Standard Time",*/            540 }, // +9
      { 235, /* "Tokyo Standard Time",*/            540 }, // +9
      { 240, /* "Yakutsk Standard Time",*/          540 }, // +9
      { 245, /* "A.U.S. Central Standard Time",*/   570 }, // +9.5
      { 250, /* "Cen. Australia Standard Time",*/   570 }, // +9.5
      { 255, /* "A.U.S. Eastern Standard Time",*/   600 }, // +10
      { 260, /* "E. Australia Standard Time",*/     600 }, // +10
      { 265, /* "Tasmania Standard Time",*/         600 }, // +10
      { 270, /* "Vladivostok Standard Time",*/      600 }, // +10
      { 275, /* "West Pacific Standard Time",*/     600 }, // +10
      { 280, /* "Central Pacific Standard Time",*/  660 }, // +11
      { 285, /* "Fiji Islands Standard Time",*/     720 }, // +12
      { 290, /* "New Zealand Standard Time",*/      720 }, // +12
      { 300, /* "Tonga Standard Time",*/            780 }};// +13

   int tableSize = sizeof(TABLE) / sizeof(TABLE[0]);
   int look;
   int tzIndex = (-1);

   /* XXX Finds the first match, not necessariy the right match! */
   for (look = 0; look < tableSize && tzIndex < 0; look++) {
      if (TABLE[look].utcStdOffMins == utcStdOffMins) {
         tzIndex = TABLE[look].winTzIndex;
      }
   }

   return tzIndex;
}


#ifdef _WIN32
/*
 *----------------------------------------------------------------------
 *
 * Win32TimeUtilLookupZoneIndex --
 *
 *    Private function. Gets current Std time zone name using Windows
 *    API, then scans the registry to find the information about that zone,
 *    and extracts the TZ Index.
 *
 * Parameters:
 *    targetName   Standard-time zone name to look for.
 *
 * Results:
 *    Returns Windows TZ Index (>=0) if found.
 *    Returns -1 if not found or if any error was encountered.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int Win32TimeUtilLookupZoneIndex(const char* targetName)
{
   int timeZoneIndex = (-1);
   HKEY parentKey, childKey;
   char childKeyName[255];
   int keyIndex, childKeyLen=255;
   DWORD rv;

   /* Open parent key containing timezone child keys */
   if (Win32U_RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                           "SOFTWARE\\"
                           "Microsoft\\"
                           "Windows NT\\"
                           "CurrentVersion\\"
                           "Time Zones",
                           0,
                           KEY_READ,
                           &parentKey) != ERROR_SUCCESS) {
      /* Failed to open registry */
      return (-1);
   }

   /* Scan child keys, stopping if name is found */
   keyIndex = 0;
   while (
         timeZoneIndex < 0 &&
         Win32U_RegEnumKeyEx(parentKey,
                             keyIndex,
                             childKeyName,
                             &childKeyLen,
                             0,0,0,0) == ERROR_SUCCESS) {

      char* std;
      DWORD stdSize;

      /* Open child key */
      rv = Win32U_RegOpenKeyEx(parentKey, childKeyName, 0, KEY_READ, &childKey);
      if (rv != ERROR_SUCCESS) {
         continue;
      }

      /* Get size of "Std" value */
      if (Win32U_RegQueryValueEx(childKey,
                                 "Std", 0, 0,
                                 NULL, &stdSize) == ERROR_SUCCESS) {

         /* Get value of "Std" */
         std = (char*) calloc(stdSize+1, sizeof(char));
         if (std != NULL) {
            if (Win32U_RegQueryValueEx(childKey,
                                       "Std", 0, 0,
                                       (LPBYTE) std, &stdSize) == ERROR_SUCCESS) {

               /* Make sure there is at least one EOS */
               std[stdSize] = '\0';

               /* Is this the name we want? */
               if (!strcmp(std, targetName)) {

                  /* yes: look up value of "Index" */
                  DWORD val = 0;
                  DWORD valSize = sizeof(val);
                  if (Win32U_RegQueryValueEx(childKey,
                                             "Index", 0, 0,
                                             (LPBYTE) &val,
                                             &valSize) == ERROR_SUCCESS) {
                     timeZoneIndex = val;
                  }
              }
           }
           free(std);
        }
     }

      /* close this child key */
      RegCloseKey(childKey);

      /* reset for next child key */
      childKeyLen = 255;
      keyIndex++;
   }

   /* Close registry parent key */
   RegCloseKey(parentKey);

   return timeZoneIndex;
}
#endif // _WIN32
