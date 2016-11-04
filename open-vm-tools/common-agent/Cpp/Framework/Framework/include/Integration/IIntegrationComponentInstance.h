/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IIntegrationComponentInstance_h_
#define _IntegrationContracts_IIntegrationComponentInstance_h_


#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IIntegrationComponentInstance : public ICafObject
{
	CAF_DECL_UUID("70053165-1e46-4893-8e27-0e6ee8675c44")

	virtual void wire(
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IIntegrationComponentInstance);

}

#endif // #ifndef _IntegrationContracts_IIntegrationComponent_h_

