/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CErrorHandler_h_
#define CErrorHandler_h_


#include "Integration/IErrorHandler.h"

#include "Integration/IChannelResolver.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "Integration/IThrowable.h"

namespace Caf {

class INTEGRATIONCORE_LINKAGE CErrorHandler :
	public IErrorHandler {
public:
	CErrorHandler();
	virtual ~CErrorHandler();

public:
	void initialize(
		const SmartPtrIChannelResolver& channelResolver,
		const SmartPtrIMessageChannel& errorMessageChannel);

public: // IErrorHandler
	void handleError(
		const SmartPtrIThrowable& throwable,
		const SmartPtrIIntMessage& message) const;

private:
	bool _isInitialized;
	SmartPtrIChannelResolver _channelResolver;
	SmartPtrIMessageChannel _errorMessageChannel;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CErrorHandler);
};

CAF_DECLARE_SMART_POINTER(CErrorHandler);
}

#endif // #ifndef CErrorHandler_h_
