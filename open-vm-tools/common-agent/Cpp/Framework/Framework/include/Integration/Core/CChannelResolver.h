/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CChannelResolver_h_
#define CChannelResolver_h_


#include "Integration/IChannelResolver.h"

#include "Integration/IIntegrationObject.h"
#include "Integration/IMessageChannel.h"

namespace Caf {

class INTEGRATIONCORE_LINKAGE CChannelResolver :
	public IChannelResolver {
public:
	CChannelResolver();
	virtual ~CChannelResolver();

public:
	void initialize(
		const SmartPtrCIntegrationObjectCollection& integrationObjectCollection);

public: // IChannelResolver
	SmartPtrIMessageChannel resolveChannelName(
		const std::string& channelName) const;

	SmartPtrIIntegrationObject resolveChannelNameToObject(
		const std::string& channelName) const;

private:
	bool _isInitialized;
	SmartPtrCIntegrationObjectCollection _integrationObjectCollection;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CChannelResolver);
};

CAF_DECLARE_SMART_POINTER(CChannelResolver);

}

#endif // #ifndef CChannelResolver_h_
