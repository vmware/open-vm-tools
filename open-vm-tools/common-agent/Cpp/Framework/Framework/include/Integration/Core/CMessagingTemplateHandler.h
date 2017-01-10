/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CMessagingTemplateHandler_h_
#define CMessagingTemplateHandler_h_


#include "Integration/IMessageHandler.h"

#include "Integration/IIntMessage.h"

namespace Caf {

class INTEGRATIONCORE_LINKAGE CMessagingTemplateHandler :
	public IMessageHandler {
public:
	CMessagingTemplateHandler();
	virtual ~CMessagingTemplateHandler();

public:
	void initialize(
		const SmartPtrIMessageHandler& messageHandler);

public: // IMessageHandler
	void handleMessage(
		const SmartPtrIIntMessage& message);
	SmartPtrIIntMessage getSavedMessage() const;
	void clearSavedMessage();

private:
	bool _isInitialized;

	SmartPtrIMessageHandler _messageHandler;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CMessagingTemplateHandler);
};

CAF_DECLARE_SMART_POINTER(CMessagingTemplateHandler);

}

#endif // #ifndef CMessagingTemplateHandler_h_
