/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CMonitorInboundChannelAdapterInstance_h_
#define CMonitorInboundChannelAdapterInstance_h_


#include "Integration/IIntegrationComponentInstance.h"

#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/ITaskExecutor.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ILifecycle.h"
#include "Integration/IMessageProducer.h"

namespace Caf {

class CMonitorInboundChannelAdapterInstance :
		public IIntegrationObject,
		public IIntegrationComponentInstance,
		public ILifecycle,
		public IMessageProducer {
public:
	CMonitorInboundChannelAdapterInstance();
	virtual ~CMonitorInboundChannelAdapterInstance();

public:
	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(ILifecycle)
		CAF_QI_ENTRY(IMessageProducer)
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

public: // IMessageProducer
	bool isMessageProducer() const;

private:
	bool _isInitialized;
	std::string _id;
	SmartPtrIDocument _configSection;
	SmartPtrITaskExecutor _taskExecutor;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CMonitorInboundChannelAdapterInstance);
};

CAF_DECLARE_SMART_QI_POINTER(CMonitorInboundChannelAdapterInstance);

}

#endif // #ifndef CMonitorInboundChannelAdapterInstance_h_
