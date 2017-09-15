/*
 *	 Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CThreadUtils_H_
#define CThreadUtils_H_

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CThreadUtils {
public:
	typedef void* (*threadFunc) (void* data);

public:
	static uint32 getThreadStackSizeKb();
	static GThread* startJoinable(threadFunc func, void* data);
	static void join(GThread* thread);
	static void sleep(const uint32 milliseconds);

private:
	CAF_CM_DECLARE_NOCREATE(CThreadUtils);
};

}

#endif /* CThreadUtils_H_ */
