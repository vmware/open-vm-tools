/*
 *	 Author: bwilliams
 *  Created: Oct 29, 2014
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CAUTOMUTEX_H_
#define CAUTOMUTEX_H_

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CAutoMutex {
public:
	CAutoMutex();
	~CAutoMutex();

	void initialize();
	bool isInitialized() const;

	void lock(const char* className = NULL, const char* funcName = NULL, const int32 lineNumber = 0);
	void unlock(const char* className = NULL, const char* funcName = NULL, const int32 lineNumber = 0);

	GMutex* getNonConstPtr();

private:
	GMutex _mutex;
	bool _isInitialized;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CAutoMutex);
};
CAF_DECLARE_SMART_POINTER(CAutoMutex);

}

#endif /* CAUTOMUTEX_H_ */
