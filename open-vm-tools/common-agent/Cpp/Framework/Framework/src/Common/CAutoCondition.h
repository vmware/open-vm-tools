/*
 *	 Author: bwilliams
 *  Created: Oct 29, 2014
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CAUTOCONDITION_H_
#define CAUTOCONDITION_H_

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
