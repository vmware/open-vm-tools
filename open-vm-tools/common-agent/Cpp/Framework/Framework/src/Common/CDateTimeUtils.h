/*
 *	 Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CDateTimeUtils_H_
#define CDateTimeUtils_H_

#ifndef WIN32
#include <sys/time.h>
#else
#include <winsock.h>
#endif

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CDateTimeUtils {
public:
	static uint64 getTimeMs();

	static uint64 calcRemainingTime(
		const uint64 begTimeMs,
		const uint64 totalMs);

	static int32 getTimeOfDay(struct timeval *tv, struct timezone *tz);

	static std::string getCurrentDateTime();

private:
	CAF_CM_DECLARE_NOCREATE(CDateTimeUtils);
};

}

#endif /* CDateTimeUtils_H_ */
