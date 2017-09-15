/*
 *	 Author: bwilliams
 *  Created: Oct 29, 2014
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CAUTOCONDITION_H_
#define CAUTOCONDITION_H_

#include "Common/CAutoCondition.h"

#include "Common/CAutoMutex.h"

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CAutoCondition {
public:
	CAutoCondition();
	~CAutoCondition();

	void initialize(const std::string& name);
	bool isInitialized() const;

	void close();

	std::string getName() const;

	void signal();
	void wait(SmartPtrCAutoMutex& mutex);
	bool waitUntil(SmartPtrCAutoMutex& mutex, gint64 endTime);

private:
	GCond _condition;
	std::string _name;
	bool _isInitialized;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CAutoCondition);
};
CAF_DECLARE_SMART_POINTER(CAutoCondition);
}

#endif /* CAUTOCONDITION_H_ */
