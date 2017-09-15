/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CPersistenceOutboundChannelAdapterInstance_h_
#define CPersistenceOutboundChannelAdapterInstance_h_


#include "Integration/IIntegrationComponentInstance.h"

#include "Common/IAppContext.h"
#include "IPersistence.h"
#include "Integration/Core/CMessagingTemplate.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ILifecycle.h"

namespace Caf {

class CPersistenceOutboundChannelAdapterInstance :
		public IIntegrationObject,
		public IIntegrationComponentInstance,
		public ILifecycle {
public:
	CPersistenceOutboundChannelAdapterInstance();
	virtual ~CPersistenceOutboundChannelAdapterInstance();

	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(ILifecycle)
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

private:
	SmartPtrIPersistence createPersistence(
		const SmartPtrIAppContext& appContext) const;

private:
	bool _isInitialized;
	bool _isRunning;
	std::string _id;
	SmartPtrIDocument _configSection;
	SmartPtrCMessagingTemplate _messagingTemplate;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CPersistenceOutboundChannelAdapterInstance);
};

CAF_DECLARE_SMART_QI_POINTER(CPersistenceOutboundChannelAdapterInstance);
}

#endif // #ifndef CPersistenceOutboundChannelAdapterInstance_h_
