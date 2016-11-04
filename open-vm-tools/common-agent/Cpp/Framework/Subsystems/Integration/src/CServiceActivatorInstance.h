/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CServiceActivatorInstance_h_
#define CServiceActivatorInstance_h_


#include "Integration/IIntegrationComponentInstance.h"

#include "Common/IAppContext.h"
#include "Integration/Core/CMessagingTemplate.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"

namespace Caf {

class CServiceActivatorInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ILifecycle
{
public:
	CServiceActivatorInstance();
	virtual ~CServiceActivatorInstance();

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
	bool _isInitialized;
	IBean::Cargs _ctorArgs;
	IBean::Cprops _properties;
	SmartPtrIDocument _configSection;
	std::string _id;
	SmartPtrCMessagingTemplate _messagingTemplate;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CServiceActivatorInstance);
};

CAF_DECLARE_SMART_QI_POINTER(CServiceActivatorInstance);

}

#endif // #ifndef CServiceActivatorInstance_h_
