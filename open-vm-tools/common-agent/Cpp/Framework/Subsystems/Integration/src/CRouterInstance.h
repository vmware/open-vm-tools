/*
 *  Created on: Aug 15, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CRouterInstance_h
#define CRouterInstnace_h

#include "Integration/IIntegrationComponentInstance.h"
#include "Common/IAppContext.h"
#include "Integration/Core/CExpressionHandler.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/Core/CAbstractMessageRouter.h"

namespace Caf {

class CRouterInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public CAbstractMessageRouter {
public:
	CRouterInstance();
	virtual ~CRouterInstance();

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

private:
	ChannelCollection getTargetChannels(
			const SmartPtrIIntMessage& message);

private:
	bool _isInitialized;
	std::string _id;
	int32 _timeout;
	std::string _defaultOutputChannelId;
	bool _resolutionRequired;
	std::string _expressionStr;
	SmartPtrCExpressionHandler _expressionHandler;
	Cmapstrstr _valueToChannelMapping;
	SmartPtrIChannelResolver _channelResolver;

	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CRouterInstance);
};
CAF_DECLARE_SMART_QI_POINTER(CRouterInstance);
}


#endif /* CRouterInstance_h */
