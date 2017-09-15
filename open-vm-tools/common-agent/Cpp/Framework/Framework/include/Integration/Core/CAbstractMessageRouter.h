/*
 *  Created on: Aug 9, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CAbstractMessageRouter_h
#define CAbstractMessageRouter_h


#include "Integration/IMessageRouter.h"

#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"

namespace Caf {

class INTEGRATIONCORE_LINKAGE CAbstractMessageRouter : public IMessageRouter {
public:
	CAbstractMessageRouter();
	virtual ~CAbstractMessageRouter();

	void init();

	void init(
			const SmartPtrIMessageChannel& defaultOutputChannel,
			const bool ignoreSendFailures,
			const int32 sendTimeout);

	virtual void routeMessage(
			const SmartPtrIIntMessage& message);

protected:
	typedef std::deque<SmartPtrIMessageChannel> ChannelCollection;
	virtual ChannelCollection getTargetChannels(
			const SmartPtrIIntMessage& message) = 0;

private:
	SmartPtrIMessageChannel _defaultOutputChannel;
	bool _ignoreSendFailures;
	int32 _sendTimeout;

private:
	bool _isInitialized;
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CAbstractMessageRouter);
};

}

#endif /* CAbstractMessageRouter_h */
