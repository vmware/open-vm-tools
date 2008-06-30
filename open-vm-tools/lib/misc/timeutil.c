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
   static unsigned int monthdays[13] = { 0,
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
   unsigned int i;

   /*
    * Initialize the table
    */

   if (   (d->year % 4) == 0
       && (   (d->year % 100) != 0
           || (d->year % 400) == 0)) {
      /* Leap year */
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

            if (   (d->year % 4) == 0
                && (   (d->year % 100) != 0
                    || (d->year % 400) == 0)) {
               /* Leap year */
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
 *    On Linux, calls localtime() which sets the system globals
 *    'timezone' and 'tzname'
 *
 *----------------------------------------------------------------------
 */
int
TimeUtil_GetLocalWindowsTimeZoneIndex(void)
{
   int utcOffsetMins = 0;
   int winTimeZoneIndex = (-1);

#if defined(_WIN32)

   TIME_ZONE_INFORMATION tz;
   char name[256] = { 0 };
   size_t nameLen = 0, nc = 0;
   if (GetTimeZoneInformation(&tz) == TIME_ZONE_ID_INVALID) {
      return (-1);
   }

   /* 'Bias' = diff between UTC and local standard time */
   utcOffsetMins = 0-tz.Bias; // already in minutes

   /* Get TZ name */
   nameLen = wcslen(tz.StandardName);
   if (wcstombs(name, tz.StandardName, 255) <= 0) {
         return (-1);
   }

   /* Convert name to Windows TZ index */
   winTimeZoneIndex = Win32TimeUtilLookupZoneIndex(name);

#else // NOT _WIN32

   /*
    * Use localtime(), but we only need its side effects:
    * external 'timezone' = diff between UTC and local standard time
    * external 'tzname[0]' contains Std timezone name
    * see 'man localtime'
    */

   time_t now = time(NULL);

   #ifdef __FreeBSD__
   struct tm* tmp = localtime(&now);
   utcOffsetMins = tmp->tm_gmtoff/60; // secs->mins
   #elif defined __APPLE__
   struct tm* tmp = localtime(&now);
   utcOffsetMins = tmp->tm_gmtoff/60; // secs->mins
   #else
   localtime(&now);
   utcOffsetMins = timezone/60; // secs->mins
   #endif

   /* can't figure this out directly for non-Win32 */
   winTimeZoneIndex = (-1);

#endif

   /* If we don't have it yet, look up windowsCode. */
   if (winTimeZoneIndex < 0) {
      winTimeZoneIndex = TimeUtilFindIndexByUTCOffset(utcOffsetMins);
   }

   return winTimeZoneIndex;
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
