/*
 *  Created on: Aug 6, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_ISmartLifecycle_h
#define _IntegrationContracts_ISmartLifecycle_h


#include "Integration/ILifecycle.h"

namespace Caf {

struct __declspec(novtable) ISmartLifecycle : public ILifecycle {
	CAF_DECL_UUID("312B7430-659F-48A1-AAAE-AE44D349132C")

	virtual bool isAutoStartup() const = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(ISmartLifecycle);

};

#endif
