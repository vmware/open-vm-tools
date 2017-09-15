/*
 *  Created on: Nov 25, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef COutgoingMessageHandler_h
#define COutgoingMessageHandler_h


#include "IBean.h"

#include "CMessageDeliveryRecord.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageProcessor.h"

using namespace Caf;

class COutgoingMessageHandler :
	public TCafSubSystemObjectRoot<COutgoingMessageHandler>,
	public IBean,
	public IMessageProcessor {
public:
	COutgoingMessageHandler();
	virtual ~COutgoingMessageHandler();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdCommIntegrationOutgoingMessageHandler)

	CAF_BEGIN_INTERFACE_MAP(COutgoingMessageHandler)
		CAF_INTERFACE_ENTRY(IBean)
		CAF_INTERFACE_ENTRY(IMessageProcessor)
	CAF_END_INTERFACE_MAP()

public: // IBean
	virtual void initializeBean(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties);

	virtual void terminateBean();

public: // IMessageProcessor
	SmartPtrIIntMessage processMessage(
		const SmartPtrIIntMessage& message);

private:
	static SmartPtrIIntMessage rehydrateMultiPartMessage(
		const SmartPtrCMessageDeliveryRecord& deliveryRecord,
		const IIntMessage::SmartPtrCHeaders& addlHeaders);

	static SmartPtrIIntMessage augmentHeaders(
		const bool isMultiPart,
		const SmartPtrIIntMessage& message);

private:
	bool _isInitialized;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(COutgoingMessageHandler);
};

#endif /* COutgoingMessageHandler_h */

