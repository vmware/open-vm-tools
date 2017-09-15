/*
 *  Created on: Aug 10, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CMessageHandlerChainInstance_h
#define CMessageHandlerChainInstance_h


#include "Integration/IIntegrationComponentInstance.h"

#include "Common/IAppContext.h"
#include "ICafObject.h"
#include "Integration/Core/CMessageHandler.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/ISubscribableChannel.h"
#include "Integration/ITaskExecutor.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ILifecycle.h"
#include "Integration/IMessageChannel.h"
#include "Integration/IMessageHandler.h"

namespace Caf {

class CMessageHandlerChainInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ILifecycle,
	public IMessageHandler
{
public:
		CMessageHandlerChainInstance();
		virtual ~CMessageHandlerChainInstance();

	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(ILifecycle)
		CAF_QI_ENTRY(IMessageHandler)
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

public: // IMessageHandler
	void handleMessage(
		const SmartPtrIIntMessage& message);
	SmartPtrIIntMessage getSavedMessage() const;
	void clearSavedMessage();

private:
	void logMessage(const SmartPtrIIntMessage& message) const;

private:
	class ChainedMessageHandler {
	public:
		ChainedMessageHandler();

		void init(
			const SmartPtrIAppContext& appContext,
			const SmartPtrIChannelResolver& channelResolver);
		void setId(const std::string& id);
		void setOutputChannel(const SmartPtrIMessageChannel& channel);
		void setMessageHandler(const SmartPtrICafObject& handlerObj);
		void handleMessage(const SmartPtrIIntMessage& message);
		SmartPtrIIntMessage getSavedMessage() const;
		void clearSavedMessage();

	private:
		void logMessage(const SmartPtrIIntMessage& message) const;

	private:
		bool _isInitialized;
		std::string _id;
		SmartPtrIMessageChannel _outputChannel;
		SmartPtrICafObject _messageHandlerObj;
		SmartPtrCMessageHandler _messageHandler;
		CAF_CM_CREATE;
		CAF_CM_DECLARE_NOCOPY(ChainedMessageHandler);
	};
	CAF_DECLARE_SMART_POINTER(ChainedMessageHandler);
	typedef std::deque<SmartPtrChainedMessageHandler> MessageHandlers;

	class InterconnectChannel : public IMessageChannel {
	public:
		InterconnectChannel();
		virtual ~InterconnectChannel();
		void init(const SmartPtrChainedMessageHandler& nextHandler);
		bool send(
			const SmartPtrIIntMessage& message);
		bool send(
			const SmartPtrIIntMessage& message,
			const int32 timeout);

	private:
		SmartPtrChainedMessageHandler _nextHandler;
		CAF_CM_CREATE;
		CAF_CM_DECLARE_NOCOPY(InterconnectChannel);
	};
	CAF_DECLARE_SMART_POINTER(InterconnectChannel);

	struct ChainLink {
		CMessageHandlerChainInstance::SmartPtrChainedMessageHandler handler;
		std::string id;
		bool isMessageProducer;
	};
	CAF_DECLARE_SMART_POINTER(ChainLink);
	typedef std::vector<SmartPtrChainLink> ChainLinks;

	class SelfWeakReference : public IMessageHandler {
	public:
		SelfWeakReference();
		void setReference(IMessageHandler *handler);
	public: // IMessageHandler
		void handleMessage(
			const SmartPtrIIntMessage& message);
		SmartPtrIIntMessage getSavedMessage() const;
		void clearSavedMessage();
	private:
		IMessageHandler *_reference;
		CAF_CM_CREATE_THREADSAFE;
	};
	CAF_DECLARE_SMART_POINTER(SelfWeakReference);

private:
	bool _isInitialized;
	bool _isRunning;
	IBean::Cargs _ctorArgs;
	IBean::Cprops _properties;
	SmartPtrIDocument _configSection;
	std::string _id;
	SmartPtrISubscribableChannel _subscribableInputChannel;
	SmartPtrITaskExecutor _taskExecutor;
	SmartPtrSelfWeakReference _weakRefSelf;
	MessageHandlers _messageHandlers;
	SmartPtrIIntMessage _savedMessage;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CMessageHandlerChainInstance);
};

CAF_DECLARE_SMART_QI_POINTER(CMessageHandlerChainInstance);

}

#endif /* CMessageHandlerChainInstance_h */
