/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CMessagingTemplate_h_
#define CMessagingTemplate_h_

namespace Caf {

class INTEGRATIONCORE_LINKAGE CMessagingTemplate :
	public ILifecycle {
public:
	CMessagingTemplate();
	virtual ~CMessagingTemplate();

public:
	void initialize(
		const SmartPtrIChannelResolver& channelResolver,
		const SmartPtrIIntegrationObject& inputIntegrationObject,
		const SmartPtrIMessageChannel& errorMessageChannel,
		const SmartPtrIMessageChannel& outputMessageChannel,
		const SmartPtrICafObject& messageHandlerObj);

public: // ILifecycle
	void start(const uint32 timeoutMs);
	void stop(const uint32 timeoutMs);
	bool isRunning() const;

private:
	bool _isInitialized;
	bool _isRunning;
	std::string _inputId;

	SmartPtrISubscribableChannel _inputSubscribableChannel;
	SmartPtrCMessagingTemplateHandler _messagingTemplateHandler;
	SmartPtrITaskExecutor _taskExecutor;

private:
	SmartPtrITaskExecutor createTaskExecutor(
		const SmartPtrIChannelResolver& channelResolver,
		const SmartPtrCMessageHandler& messageHandler,
		const SmartPtrIPollableChannel& inputPollableChannel,
		const SmartPtrIMessageChannel& errorMessageChannel) const;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CMessagingTemplate);
};

CAF_DECLARE_SMART_POINTER(CMessagingTemplate);

}

#endif // #ifndef CMessagingTemplate_h_
