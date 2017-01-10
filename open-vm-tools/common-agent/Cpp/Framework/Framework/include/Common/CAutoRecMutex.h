/*
 *	 Author: bwilliams
 *  Created: Oct 29, 2014
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CAUTORECMUTEX_H_
#define CAUTORECMUTEX_H_

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CAutoRecMutex {
public:
	CAutoRecMutex();
	~CAutoRecMutex();

	void initialize();
	bool isInitialized() const;

	void lock(const char* className = NULL, const char* funcName = NULL, const int32 lineNumber = 0);
	void unlock(const char* className = NULL, const char* funcName = NULL, const int32 lineNumber = 0);

	GRecMutex* getNonConstPtr();

private:
	GRecMutex _mutex;
	bool _isInitialized;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CAutoRecMutex);
};
CAF_DECLARE_SMART_POINTER(CAutoRecMutex);

}

#endif /* CAUTORECMUTEX_H_ */
