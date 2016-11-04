/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CMessageHandler_h_
#define CMessageHandler_h_


#include "Integration/IMessageHandler.h"

#include "ICafObject.h"
#include "Integration/IErrorProcessor.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "Integration/IMessageProcessor.h"
#include "Integration/IMessageRouter.h"
#include "Integration/IMessageSplitter.h"
#include "Integration/ITransformer.h"

namespace Caf {

class INTEGRATIONCORE_LINKAGE CMessageHandler : public IMessageHandler {
public:
	CMessageHandler();
	virtual ~CMessageHandler();

public:
	void initialize(
		const std::string& inputId,
		const SmartPtrIMessageChannel& outputMessageChannel,
		const SmartPtrICafObject& messageHandlerObj);

public:
	std::string getInputId() const;

public: // IMessageHandler
	void handleMessage(
		const SmartPtrIIntMessage& message);
	SmartPtrIIntMessage getSavedMessage() const;
	void clearSavedMessage();

private:
	bool _isInitialized;
	std::string _inputId;
	SmartPtrIIntMessage _savedMessage;

	SmartPtrIMessageChannel _outputMessageChannel;
	SmartPtrITransformer _transformer;
	SmartPtrIMessageProcessor _messageProcessor;
	SmartPtrIErrorProcessor _errorProcessor;
	SmartPtrIMessageSplitter _messageSplitter;
	SmartPtrIMessageRouter _messageRouter;
	SmartPtrIMessageHandler _messageHandler;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CMessageHandler);
};

CAF_DECLARE_SMART_POINTER(CMessageHandler);

}

#endif // #ifndef CMessageHandler_h_
