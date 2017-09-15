/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IChannelResolver_h_
#define _IntegrationContracts_IChannelResolver_h_


#include "Integration/IIntegrationObject.h"
#include "Integration/IMessageChannel.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IChannelResolver : public ICafObject
{
	CAF_DECL_UUID("32361862-a312-4cab-a978-45b7059ca102")

	virtual SmartPtrIMessageChannel resolveChannelName(
		const std::string& channelName) const = 0;

	virtual SmartPtrIIntegrationObject resolveChannelNameToObject(
		const std::string& channelName) const = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IChannelResolver);

}

#endif // #ifndef _IntegrationContracts_IChannelResolver_h_

