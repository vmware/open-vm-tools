/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CQueueChannelInstance_h_
#define CQueueChannelInstance_h_

#include "Integration/IIntegrationComponentInstance.h"
#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/Core/CAbstractPollableChannel.h"

namespace Caf {

/// Sends responses/errors back to the client.
class CQueueChannelInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public CAbstractPollableChannel {
public:
	CQueueChannelInstance();
	virtual ~CQueueChannelInstance();

	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(IPollableChannel)
		CAF_QI_ENTRY(IMessageChannel)
		CAF_QI_ENTRY(IChannelInterceptorSupport)
	CAF_END_QI()

public: // IIntegrationObject
	void initialize(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties,
		const SmartPtrIDocument& configSection);

	std::string getId() const;

public: // IIntegrationComponentInstance
	void wire(
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver);

protected: // CAbstractPollableChannel
	bool doSend(
			const SmartPtrIIntMessage& message,
			int32 timeout);

	SmartPtrIIntMessage doReceive(const int32 timeout);

private:
	bool _isInitialized;
	SmartPtrIDocument _configSection;
	std::string _id;
	std::deque<SmartPtrIIntMessage> _messageQueue;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(CQueueChannelInstance);
};

CAF_DECLARE_SMART_QI_POINTER(CQueueChannelInstance);

}

#endif // #ifndef CQueueChannelInstance_h_
