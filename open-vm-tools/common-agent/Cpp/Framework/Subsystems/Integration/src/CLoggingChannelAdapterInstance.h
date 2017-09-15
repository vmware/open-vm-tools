/*
 *  Created on: Jan 31, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CLOGGINGCHANNELADAPTERINSTANCE_H_
#define CLOGGINGCHANNELADAPTERINSTANCE_H_


#include "Integration/IIntegrationComponentInstance.h"

#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/IMessageHandler.h"

namespace Caf {

class CLoggingChannelAdapterInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public IMessageHandler {
public:
	CLoggingChannelAdapterInstance();
	virtual ~CLoggingChannelAdapterInstance();

	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(IMessageHandler)
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

public: // IMessageHandler
	void handleMessage(const SmartPtrIIntMessage& message);
	SmartPtrIIntMessage getSavedMessage() const;
	void clearSavedMessage();

private:
	bool _isInitialized;
	std::string _id;
	log4cpp::Priority::PriorityLevel _level;
	bool _logFullMessage;
	log4cpp::Category* _category;
	SmartPtrIIntMessage _savedMessage;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CLoggingChannelAdapterInstance);
};
CAF_DECLARE_SMART_QI_POINTER(CLoggingChannelAdapterInstance);
}

#endif /* CLOGGINGCHANNELADAPTERINSTANCE_H_ */
