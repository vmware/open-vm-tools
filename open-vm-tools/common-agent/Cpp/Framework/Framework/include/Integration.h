/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef COMMON_SYS_INC_INTEGRATION_H_
#define COMMON_SYS_INC_INTEGRATION_H_

#include <CommonDefines.h>
#include <IntegrationContracts.h>

#include "../src/Integration/Core/IntegrationCoreLink.h"
#include "../src/Integration/Caf/IntegrationCafLink.h"

namespace Caf {
	// Integration Contracts
	extern INTEGRATIONCORE_LINKAGE const char* _sObjIdIntegrationObjectFactory;
	extern INTEGRATIONCORE_LINKAGE const char* _sObjIdErrorChannel;
	extern INTEGRATIONCORE_LINKAGE const char* _sObjIdNullChannel;
	extern INTEGRATIONCORE_LINKAGE const char* _sObjIdHeaderExpressionInvoker;
}

#endif /* COMMON_SYS_INC_INTEGRATION_H_ */
