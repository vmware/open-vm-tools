/*
 *	 Author: bwilliams
 *  Created: Nov 17, 2014
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CTimeUnit_H_
#define CTimeUnit_H_

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CTimeUnit {
public:
	class COMMONAGGREGATOR_LINKAGE MILLISECONDS {
	public:
		static int32 toDays(const int32 milliseconds);
		static int32 toHours(const int32 milliseconds);
		static int32 toMinutes(const int32 milliseconds);
		static int32 toSeconds(const int32 milliseconds);
	};

	class COMMONAGGREGATOR_LINKAGE SECONDS {
	public:
		static int32 toDays(const int32 seconds);
		static int32 toHours(const int32 seconds);
		static int32 toMinutes(const int32 seconds);
		static int32 toMilliseconds(const int32 seconds);
	};

	class COMMONAGGREGATOR_LINKAGE MINUTES {
	public:
		static int32 toDays(const int32 minutes);
		static int32 toHours(const int32 minutes);
		static int32 toSeconds(const int32 minutes);
		static int32 toMilliseconds(const int32 minutes);
	};

	class COMMONAGGREGATOR_LINKAGE HOURS {
	public:
		static int32 toDays(const int32 hours);
		static int32 toMinutes(const int32 hours);
		static int32 toSeconds(const int32 hours);
		static int32 toMilliseconds(const int32 hours);
	};

	class COMMONAGGREGATOR_LINKAGE DAYS {
	public:
		static int32 toHours(const int32 days);
		static int32 toMinutes(const int32 days);
		static int32 toSeconds(const int32 days);
		static int32 toMilliseconds(const int32 days);
	};

private:
	CAF_CM_DECLARE_NOCREATE(CTimeUnit);
};

}

#endif /* CTimeUnit_H_ */
