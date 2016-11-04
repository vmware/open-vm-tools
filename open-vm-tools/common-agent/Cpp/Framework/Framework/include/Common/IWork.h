/*
 *  Author: bwilliams
 *  Created: June 29, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _CommonAggregator_IWork_h_
#define _CommonAggregator_IWork_h_


#include "ICafObject.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IWork : public ICafObject {
	CAF_DECL_UUID("76c269db-691f-439d-b47d-87ce55639c8f")

public: // Read operations
	virtual void doWork() = 0;
	virtual void stopWork() = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IWork);

}

#endif // #ifndef _CommonAggregator_IWork_h_

