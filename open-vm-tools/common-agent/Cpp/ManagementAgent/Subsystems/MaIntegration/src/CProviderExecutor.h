/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CProviderExecutor_h_
#define CProviderExecutor_h_


#include "IBean.h"

#include "CProviderExecutorRequestHandler.h"
#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IErrorHandler.h"
#include "Integration/IIntMessage.h"
#include "Integration/ITransformer.h"
#include "Integration/IIntegrationComponentInstance.h"
#include "Integration/IMessageHandler.h"

using namespace Caf;

/// TODO - describe class
class CProviderExecutor :
	public TCafSubSystemObjectRoot<CProviderExecutor>,
	public IBean,
	public IIntegrationComponentInstance,
	public IMessageHandler {

public:
	CProviderExecutor();
	virtual ~CProviderExecutor();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdProviderExecutor)

	CAF_BEGIN_INTERFACE_MAP(CProviderExecutor)
		CAF_INTERFACE_ENTRY(IBean)
		CAF_INTERFACE_ENTRY(IIntegrationComponentInstance)
		CAF_INTERFACE_ENTRY(IMessageHandler)
	CAF_END_INTERFACE_MAP()

public: // IBean
	virtual void initializeBean(
			const IBean::Cargs& ctorArgs,
			const IBean::Cprops& properties);

	virtual void terminateBean();

public: // IIntegrationComponentInstance
	virtual void wire(const SmartPtrIAppContext& appContext,
			const SmartPtrIChannelResolver& channelResolver);

public: // IMessageHandler
	virtual void handleMessage(const SmartPtrIIntMessage& message);
	SmartPtrIIntMessage getSavedMessage() const;
	void clearSavedMessage();

private:
	SmartPtrITransformer loadTransformer(
			const std::string& id,
			const SmartPtrIAppContext& appContext,
			const SmartPtrIChannelResolver& channelResolver);

private:
	bool _isInitialized;
	std::map<const std::string, SmartPtrCProviderExecutorRequestHandler> _handlers;
	std::string _beginImpersonationBeanId;
	std::string _endImpersonationBeanId;
	SmartPtrITransformer _beginImpersonationTransformer;
	SmartPtrITransformer _endImpersonationTransformer;
	SmartPtrIErrorHandler _errorHandler;


private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CProviderExecutor);
};

#endif // #ifndef CProviderExecutor_h_
