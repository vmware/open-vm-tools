/*
 *  Created on: Jan 26, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CABSTRACTMESSAGECHANNEL_H_
#define CABSTRACTMESSAGECHANNEL_H_

#include "Integration/IChannelInterceptorSupport.h"
#include "Integration/IChannelInterceptor.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "Integration/Core/IntegrationCoreLink.h"

namespace Caf {
/**
 * Base class for {@link IMessageChannel} implementations providing common
 * properties and method implementations including {@link IChannelInterceptor interceptors}.
 */
class INTEGRATIONCORE_LINKAGE CAbstractMessageChannel :
	public IMessageChannel,
	public IChannelInterceptorSupport {
public:
	CAbstractMessageChannel();
	virtual ~CAbstractMessageChannel();

public: // IMessageChannel
	bool send(
		const SmartPtrIIntMessage& message);

	bool send(
		const SmartPtrIIntMessage& message,
		const int32 timeout);

public: // IChannelInterceptorSupport
	void setInterceptors(
			const IChannelInterceptorSupport::InterceptorCollection& interceptors);

protected:
	/**
	 * Subclasses must implement this method.  A non-negative timeout indicates
	 * how int32 to wait if the channel is at capacity.  If the value is 0 it must
	 * return immediately with or without success.  A negative timeout value
	 * indicates that the method should block until either the message is accepted
	 * or the blocking thread is interrupted.
	 */
	virtual bool doSend(
			const SmartPtrIIntMessage& message,
			int32 timeout) = 0;

protected:
	std::list<SmartPtrIChannelInterceptor> getInterceptors() const;

private:
	std::list<SmartPtrIChannelInterceptor> _interceptors;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CAbstractMessageChannel);
};
}

#endif /* CABSTRACTMESSAGECHANNEL_H_ */
