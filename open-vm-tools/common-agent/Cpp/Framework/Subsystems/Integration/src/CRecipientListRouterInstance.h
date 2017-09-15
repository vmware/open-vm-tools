/*
 *  Created on: Aug 9, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CRecipientListRouterInstance_h
#define CRecipientListRouterInstance_h

#include "Integration/IIntegrationComponentInstance.h"
#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/Core/CAbstractMessageRouter.h"

namespace Caf {

class CRecipientListRouterInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public CAbstractMessageRouter {
public:
	CRecipientListRouterInstance();
	virtual ~CRecipientListRouterInstance();

	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(IMessageRouter)
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

private: // CAbstractMessageRouter
	ChannelCollection getTargetChannels(
			const SmartPtrIIntMessage& message);

private:
	bool _isInitialized;
	std::string _id;
	bool _ignoreSendFailures;
	int32 _timeout;
	Cdeqstr _staticChannelIds;
	Cmapstrstr _selectorDefinitions;
	std::deque<SmartPtrIMessageChannel> _staticChannels;
	typedef std::deque<std::pair<SmartPtrCExpressionHandler, SmartPtrIMessageChannel> > SelectorChannelCollection;
	SelectorChannelCollection _selectorChannels;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CRecipientListRouterInstance);
};
CAF_DECLARE_SMART_QI_POINTER(CRecipientListRouterInstance);
}

#endif /* CRecipientListRouterInstance_h */
