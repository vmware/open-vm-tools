/*
 *  Created on: Jan 25, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CCHANNELINTERCEPTORADAPTER_H_
#define CCHANNELINTERCEPTORADAPTER_H_


#include "Integration/IChannelInterceptor.h"

#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"

namespace Caf {

/**
 * A {@link IChannelInterceptor} with no-op method implementations so that
 * subclasses do not have to implement all of th einterface's methods.
 */
class INTEGRATIONCORE_LINKAGE CChannelInterceptorAdapter : public IChannelInterceptor {
public:
	CChannelInterceptorAdapter();
	virtual ~CChannelInterceptorAdapter();

public: // IChannelInterceptor
	virtual SmartPtrIIntMessage& preSend(
			SmartPtrIIntMessage& message,
			SmartPtrIMessageChannel& channel);

	virtual void postSend(
			SmartPtrIIntMessage& message,
			SmartPtrIMessageChannel& channel,
			bool sent);

	virtual bool preReceive(
			SmartPtrIMessageChannel& channel);

	virtual SmartPtrIIntMessage& postReceive(
			SmartPtrIIntMessage& message,
			SmartPtrIMessageChannel& channel);

private:
	CAF_CM_DECLARE_NOCOPY(CChannelInterceptorAdapter);
};

}

#endif /* CCHANNELINTERCEPTORADAPTER_H_ */
