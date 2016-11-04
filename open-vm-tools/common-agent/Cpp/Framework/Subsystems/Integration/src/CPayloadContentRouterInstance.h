/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CPayloadContentRouterInstance_h_
#define CPayloadContentRouterInstance_h_

#include "Integration/IIntegrationComponentInstance.h"
#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/Core/CAbstractMessageRouter.h"

namespace Caf {

class CPayloadContentRouterInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public CAbstractMessageRouter {
public:
	CPayloadContentRouterInstance();
	virtual ~CPayloadContentRouterInstance();

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
	std::string _defaultOutputChannelId;
	bool _resolutionRequired;
	Cmapstrstr _valueToChannelMapping;
	SmartPtrIChannelResolver _channelResolver;

private:
	std::string calcOutputChannel(
		const SmartPtrCDynamicByteArray& payload) const;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CPayloadContentRouterInstance);
};
CAF_DECLARE_SMART_QI_POINTER(CPayloadContentRouterInstance);
}

#endif // #ifndef CPayloadContentRouterInstance_h_
