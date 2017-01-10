/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CErrorChannelInstance_h_
#define CErrorChannelInstance_h_

#include "Integration/IIntegrationComponentInstance.h"
#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageDispatcher.h"
#include "Integration/IMessageHandler.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ISubscribableChannel.h"
#include "Integration/Core/CAbstractMessageChannel.h"

namespace Caf {

/// Sends responses/errors back to the client.
class CErrorChannelInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ISubscribableChannel,
	public CAbstractMessageChannel
{
public:
	CErrorChannelInstance();
	virtual ~CErrorChannelInstance();

	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(ISubscribableChannel)
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

public: // ISubscribableChannel
	void subscribe(
		const SmartPtrIMessageHandler& messageHandler);

	void unsubscribe(
		const SmartPtrIMessageHandler& messageHandler);

protected: // CAbstractMessageChannel
	bool doSend(
			const SmartPtrIIntMessage& message,
			int32 timeout);

private:
	bool _isInitialized;
	std::string _id;
	SmartPtrIMessageDispatcher _messageDispatcher;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CErrorChannelInstance);
};
CAF_DECLARE_SMART_QI_POINTER(CErrorChannelInstance);
}

#endif // #ifndef CErrorChannelInstance_h_
