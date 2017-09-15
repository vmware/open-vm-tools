/*k
 *	Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Exception/CCafException.h"
#ifndef WIN32
#include <sys/time.h>
#endif
#include "CDateTimeUtils.h"

using namespace Caf;

#ifdef WIN32
#define DELTA_EPOCH_IN_MICROSECS 11644473600000000Ui64

int32 CDateTimeUtils::getTimeOfDay(struct timeval *tv, struct timezone *tz) {
  FILETIME ft;
  unsigned __int64 tmpres = 0;
  static int32 tzflag;
 
  if (NULL != tv)
  {
    ::GetSystemTimeAsFileTime(&ft);
 
    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;
 
    /*converting file time to unix epoch*/
    tmpres -= DELTA_EPOCH_IN_MICROSECS; 
    tmpres /= 10;  /*convert into microseconds*/
    tv->tv_sec = (int32)(tmpres / 1000000UL);
    tv->tv_usec = (int32)(tmpres % 1000000UL);
  }
 
  if (NULL != tz)
  {
    if (!tzflag)
    {
      _tzset();
      tzflag++;
    }
	long minuteswest = 0;
    ::_get_timezone(&minuteswest);
	tz->tz_minuteswest = minuteswest / 60;
	int32 dstHours = 0;
	::_get_daylight(&dstHours);
    tz->tz_dsttime = dstHours == 0 ? 0 : 1;
  }
 
  return 0;
}
#else
int32 CDateTimeUtils::getTimeOfDay(struct timeval *tv, struct timezone *tz) {
	return ::gettimeofday(tv, tz);
}
#endif

uint64 CDateTimeUtils::getTimeMs() {
	CAF_CM_STATIC_FUNC("CDateTimeUtils", "getTimeMs");

	timeval curTime;
	if(-1 == getTimeOfDay(&curTime, NULL)) {
		const int32 errorCode = errno;
		CAF_CM_EXCEPTION_VA0(errorCode, "getTimeOfDay failed");
	}

	return (curTime.tv_sec * 1000) + (curTime.tv_usec / 1000);
}

uint64 CDateTimeUtils::calcRemainingTime(
	const uint64 begTimeMs,
	const uint64 totalMs) {
	CAF_CM_STATIC_FUNC_VALIDATE("CDateTimeUtils", "calcRemainingTime");

	const uint64 diffTimeMs = CDateTimeUtils::getTimeMs() - begTimeMs;
	CAF_CM_VALIDATE_NONNEGATIVE_INT64(diffTimeMs);

	uint64 rc = 0;
	if(totalMs > diffTimeMs) {
		rc = totalMs - diffTimeMs;
	}

	return rc;
}

std::string CDateTimeUtils::getCurrentDateTime() {
	time_t now;
	::time(&now);
	char buf[sizeof "0000-00-00T00:00:00Z"];
	tm result;
#ifdef WIN32
	::gmtime_s(&result, &now);
	::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &result);
#else
	::gmtime_r(&now, &result);
    ::strftime(buf, sizeof buf, "%FT%TZ", &result);
#endif

	return buf;
}
