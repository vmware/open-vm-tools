/*
 *  Created on: Jun 13, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IINTEGRATIONAPPCONTEXTAWARE_H_
#define _IntegrationContracts_IINTEGRATIONAPPCONTEXTAWARE_H_


#include "ICafObject.h"

#include "Integration/IIntegrationAppContext.h"

namespace Caf {

struct __declspec(novtable) IIntegrationAppContextAware : public ICafObject {
	CAF_DECL_UUID("9BC34EB5-AEFF-4384-86DE-421DE89AB6E8")

	virtual void setIntegrationAppContext(
			SmartPtrIIntegrationAppContext context) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IIntegrationAppContextAware);
}

#endif
