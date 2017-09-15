/*
 *  Created on: Aug 16, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CIncomingMessageHandlerInstance_h
#define CIncomingMessageHandlerInstance_h



#include "Integration/IIntegrationComponentInstance.h"

#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ITransformer.h"

namespace Caf {

class CIncomingMessageHandlerInstance :
	public TCafSubSystemObjectRoot<CIncomingMessageHandlerInstance>,
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ITransformer {
public:
	CIncomingMessageHandlerInstance();
	virtual ~CIncomingMessageHandlerInstance();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdCommIntegrationIncomingMessageHandlerInstance)

	CAF_BEGIN_INTERFACE_MAP(CIncomingMessageHandlerInstance)
		CAF_INTERFACE_ENTRY(IIntegrationObject)
		CAF_INTERFACE_ENTRY(IIntegrationComponentInstance)
		CAF_INTERFACE_ENTRY(ITransformer)
	CAF_END_INTERFACE_MAP()

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

public: // ITransformer
	SmartPtrIIntMessage transformMessage(
		const SmartPtrIIntMessage& message);

private:
	static SmartPtrIIntMessage handleMessage(
		const SmartPtrIIntMessage& message);

	static SmartPtrIIntMessage getAssembledMessage(
		const SmartPtrIIntMessage& message,
		const std::string& workingDir);

	static std::string processMessage(
		const SmartPtrIIntMessage& message,
		const std::string& workingDir);

private:
	bool _isInitialized;
	std::string _id;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CIncomingMessageHandlerInstance);
};
CAF_DECLARE_SMART_QI_POINTER(CIncomingMessageHandlerInstance);
}

#endif /* CIncomingMessageHandlerInstance_h */
