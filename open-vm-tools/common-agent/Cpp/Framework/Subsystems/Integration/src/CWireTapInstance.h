/*
 *  Created on: Jan 25, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CWIRETAPINSTANCE_H_
#define CWIRETAPINSTANCE_H_

#include "Integration/IChannelInterceptorInstance.h"
#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "Integration/IIntegrationComponentInstance.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ILifecycle.h"
#include "Integration/Core/CChannelInterceptorAdapter.h"

namespace Caf {

class CWireTapInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ILifecycle,
	public IChannelInterceptorInstance,
	public CChannelInterceptorAdapter {
public:
	CWireTapInstance();
	virtual ~CWireTapInstance();

	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(ILifecycle)
		CAF_QI_ENTRY(IChannelInterceptor)
		CAF_QI_ENTRY(IChannelInterceptorInstance)
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

public: // ILifecycle
	void start(const uint32 timeoutMs);
	void stop(const uint32 timeoutMs);
	bool isRunning() const;

public: // IChannelIntercepter
	SmartPtrIIntMessage& preSend(
			SmartPtrIIntMessage& message,
			SmartPtrIMessageChannel& channel);

public: // IChannelInterceptorInstance
	uint32 getOrder() const;

	bool isChannelIdMatched(const std::string& channelId) const;

private:
	SmartPtrIDocument _configSection;
	std::string _id;
	uint32 _order;
	int32 _timeout;
	bool _isRunning;
	std::string _channelId;
	SmartPtrIMessageChannel _channel;
	GRegex* _pattern;
	bool _isInitialized;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CWireTapInstance);
};
CAF_DECLARE_SMART_QI_POINTER(CWireTapInstance);

}

#endif /* CWIRETAPINSTANCE_H_ */
