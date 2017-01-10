/*
 *  Created on: Aug 10, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "IBean.h"
#include "ICafObject.h"
#include "Integration/Core/CErrorHandler.h"
#include "Integration/Core/CSimpleAsyncTaskExecutor.h"
#include "Integration/Core/CSourcePollingChannelAdapter.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IIntegrationComponent.h"
#include "Integration/IIntegrationComponentInstance.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/IMessageChannel.h"
#include "Integration/IPollableChannel.h"
#include "CMessageHandlerChainInstance.h"
#include "Exception/CCafException.h"
#include "CObjectFactoryTables.h"
#include "Integration/Core/CMessageHeaderUtils.h"

using namespace Caf;

CMessageHandlerChainInstance::CMessageHandlerChainInstance() :
	_isInitialized(false),
	_isRunning(false),
	CAF_CM_INIT_LOG("CMessageHandlerChainInstance") {
}

CMessageHandlerChainInstance::~CMessageHandlerChainInstance() {
	if (_weakRefSelf) {
		_weakRefSelf->setReference(NULL);
		_weakRefSelf = NULL;
	}
}

void CMessageHandlerChainInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);
	_ctorArgs = ctorArgs;
	_properties = properties;
	_configSection = configSection;
	_id = _configSection->findRequiredAttribute("id");
	_isInitialized = true;
}

std::string CMessageHandlerChainInstance::getId() const {
	return _id;
}

void CMessageHandlerChainInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);

	// no point in doing a bunch of work if there are no chained components
	IDocument::SmartPtrCOrderedChildCollection handlerConfigs =
		_configSection->getAllChildrenInOrder();
	if (!handlerConfigs->size()) {
		CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, 0,
			"No message handlers are present in the chain '%s'", _id.c_str());
	}

	// an input channel is required
	const std::string inputChannelId = _configSection->findRequiredAttribute(
		"input-channel");
	SmartPtrIMessageChannel inputChannel = channelResolver->resolveChannelName(
		inputChannelId);
	SmartPtrIPollableChannel inputPollableChannel;
	inputPollableChannel.QueryInterface(inputChannel, false);
	_subscribableInputChannel.QueryInterface(inputChannel, false);
	if (!inputPollableChannel && !_subscribableInputChannel) {
		CAF_CM_EXCEPTIONEX_VA1(NoSuchInterfaceException, 0,
			"Input channel '%s' does not support any required interfaces",
			inputChannelId.c_str());
	}

	// an output channel may be required - we'll figure it out in a bit
	const std::string outputChannelId = _configSection->findOptionalAttribute(
		"output-channel");
	SmartPtrIMessageChannel outputChannel;
	if (outputChannelId.length()) {
		outputChannel = channelResolver->resolveChannelName(outputChannelId);
	}

	// build up a collection of messaging objects comprising the chain
	ChainLinks chainLinks;
	for (TSmartConstIterator<IDocument::COrderedChildCollection> handlerConfig(
		*handlerConfigs); handlerConfig; handlerConfig++) {
		CAF_CM_LOG_DEBUG_VA2("Found handler config '%s' chain '%s'",
			handlerConfig->getName().c_str(), _id.c_str());
		const std::string& handlerType = handlerConfig->getName();
		MessageHandlerObjectCreatorMap::const_iterator handlerMapEntry =
			CObjectFactoryTables::messageHandlerObjectCreatorMap.find(handlerType);
		if (handlerMapEntry
			== CObjectFactoryTables::messageHandlerObjectCreatorMap.end()) {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, 0,
				"Message handler type '%s' is not allowed to be part of a chain",
				handlerType.c_str());
		}

		// Get the messaging object
		std::string messageHandlerId;
		SmartPtrICafObject messageHandlerObj;

		if (handlerMapEntry->second.first) {
			SmartPtrIIntegrationObject intObj;
			intObj = (handlerMapEntry->second.first)();
			intObj->initialize(_ctorArgs, _properties, *handlerConfig);
			messageHandlerId = intObj->getId();
			messageHandlerObj = intObj;
		} else {
			messageHandlerId = handlerConfig->findRequiredAttribute("id");
			const std::string refStr = handlerConfig->findRequiredAttribute("ref");
			SmartPtrIBean bean = appContext->getBean(refStr);
			SmartPtrIIntegrationComponent integrationComponent;
			integrationComponent.QueryInterface(bean, false);
			if (integrationComponent) {
				messageHandlerObj = integrationComponent->createObject(*handlerConfig);
			} else {
				messageHandlerObj = bean;
			}
		}

		// Create a partially initialized chain link.
		SmartPtrChainedMessageHandler handler;
		handler.CreateInstance();
		handler->setId(messageHandlerId);
		handler->setMessageHandler(messageHandlerObj);

		SmartPtrChainLink chainLink;
		chainLink.CreateInstance();
		chainLink->handler = handler;
		chainLink->id = messageHandlerId;
		chainLink->isMessageProducer = handlerMapEntry->second.second;
		chainLinks.push_back(chainLink);
		CAF_CM_LOG_DEBUG_VA2("Adding message handler '%s' to chain '%s'",
			messageHandlerId.c_str(), _id.c_str());
	}

	// Configure the chain links.
	// All handlers in the chain except for the last one must be message producers.
	// If the last object is a message producer than the output-channel must be set.
	for (size_t i = 0; i < chainLinks.size(); ++i) {
		SmartPtrChainLink chainLink = chainLinks[i];
		if (i < chainLinks.size() - 1) {
			if (!chainLink->isMessageProducer) {
				CAF_CM_EXCEPTIONEX_VA2(InvalidArgumentException, 0,
					"Handler '%s' in chain '%s' is not a message producer. "
						"All handlers except for the last one in the chain must "
						"be message producers.", chainLink->id.c_str(), _id.c_str());
			}

			SmartPtrInterconnectChannel nextChannel;
			nextChannel.CreateInstance();
			nextChannel->init(chainLinks[i + 1]->handler);
			chainLink->handler->setOutputChannel(nextChannel);
		} else {
			if (outputChannel && !chainLink->isMessageProducer) {
				CAF_CM_EXCEPTIONEX_VA2(InvalidArgumentException, 0,
					"Handler '%s' in chain '%s' - "
						"An output channel was provided but the last handler "
						"in the chain is not a message producer.", chainLink->id.c_str(),
					_id.c_str());
			} else if (!outputChannel && chainLink->isMessageProducer) {
				CAF_CM_EXCEPTIONEX_VA2(InvalidArgumentException, 0,
					"Handler '%s' in chain '%s' - "
						"An output channel was not provided but the last handler "
						"in the chain is a message producer.", chainLink->id.c_str(),
					_id.c_str());
			}
			if (chainLink->isMessageProducer) {
				chainLink->handler->setOutputChannel(outputChannel);
			}
		}
	}

	// Initialize the chain link handlers and store them
	// in this object's state
	for (TSmartConstIterator<ChainLinks> chainLink(chainLinks); chainLink; chainLink++) {
		chainLink->handler->init(appContext, channelResolver);
		_messageHandlers.push_back(chainLink->handler);
	}

	_weakRefSelf.CreateInstance();
	_weakRefSelf->setReference(this);

	if (!_subscribableInputChannel) {
		SmartPtrCErrorHandler errorHandler;
		errorHandler.CreateInstance();
		errorHandler->initialize(channelResolver,
			channelResolver->resolveChannelName("errorChannel"));

		SmartPtrCSourcePollingChannelAdapter channelAdapter;
		channelAdapter.CreateInstance();
		channelAdapter->initialize(_weakRefSelf, inputPollableChannel, errorHandler);

		SmartPtrCSimpleAsyncTaskExecutor taskExecutor;
		taskExecutor.CreateInstance();
		taskExecutor->initialize(channelAdapter, errorHandler);

		_taskExecutor = taskExecutor;
	}
}

void CMessageHandlerChainInstance::start(const uint32 timeoutMs) {
	CAF_CM_FUNCNAME("start");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	if (_subscribableInputChannel) {
		CAF_CM_LOG_DEBUG_VA1("Subscribing handler - %s", _id.c_str());
		_subscribableInputChannel->subscribe(_weakRefSelf);
	} else if (_taskExecutor) {
		CAF_CM_LOG_DEBUG_VA1("Executing task - %s", _id.c_str());
		_taskExecutor->execute(timeoutMs);
	} else {
		CAF_CM_EXCEPTIONEX_VA1(IllegalStateException, 0,
			"handler '%s' : you should not see this. report this bug.", _id.c_str());
	}
	_isRunning = true;
}

void CMessageHandlerChainInstance::stop(const uint32 timeoutMs) {
	CAF_CM_FUNCNAME("stop");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	try {
		if (_subscribableInputChannel) {
			CAF_CM_LOG_DEBUG_VA1("Unsubscribing handler - %s", _id.c_str());
			_subscribableInputChannel->subscribe(_weakRefSelf);
		} else if (_taskExecutor) {
			CAF_CM_LOG_DEBUG_VA1("Stopping task - %s", _id.c_str());
			_taskExecutor->cancel(timeoutMs);
		} else {
			CAF_CM_EXCEPTIONEX_VA1(IllegalStateException, 0,
				"handler '%s' : you should not see this. report this bug.", _id.c_str());
		}
	}
	CAF_CM_CATCH_ALL;
	_weakRefSelf->setReference(NULL);
	_isRunning = false;
	CAF_CM_THROWEXCEPTION;
}

bool CMessageHandlerChainInstance::isRunning() const {
	CAF_CM_FUNCNAME_VALIDATE("isRunning");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _isRunning;
}

void CMessageHandlerChainInstance::handleMessage(const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("handleMessage");

	try {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STL(_messageHandlers);
		CAF_CM_VALIDATE_INTERFACE(message);
		//logMessage(message);

		for (TSmartConstIterator<MessageHandlers> messageHandlerIter(_messageHandlers);
			messageHandlerIter; messageHandlerIter++) {
			const SmartPtrChainedMessageHandler messageHandler = *messageHandlerIter;
			CAF_CM_VALIDATE_SMARTPTR(messageHandler);
			messageHandler->clearSavedMessage();
		}

		_savedMessage = message;
		CAF_CM_VALIDATE_SMARTPTR(_messageHandlers.front());
		_messageHandlers.front()->handleMessage(message);
	}
	CAF_CM_CATCH_ALL;

	try {
		for (TSmartConstIterator<MessageHandlers> messageHandlerIter(_messageHandlers);
			messageHandlerIter; messageHandlerIter++) {
			const SmartPtrChainedMessageHandler messageHandler = *messageHandlerIter;
			CAF_CM_VALIDATE_SMARTPTR(messageHandler);
			const SmartPtrIIntMessage savedMessage = messageHandler->getSavedMessage();
			if (!savedMessage.IsNull()) {
				_savedMessage = savedMessage;
			}
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_THROWEXCEPTION;
}

SmartPtrIIntMessage CMessageHandlerChainInstance::getSavedMessage() const {
	CAF_CM_FUNCNAME_VALIDATE("getSavedMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _savedMessage;
}

void CMessageHandlerChainInstance::clearSavedMessage() {
	CAF_CM_FUNCNAME_VALIDATE("clearSavedMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_savedMessage = NULL;
}

CMessageHandlerChainInstance::ChainedMessageHandler::ChainedMessageHandler() :
	_isInitialized(false),
	CAF_CM_INIT("CMessageHandlerChainInstance::ChainedMessageHandler") {
}

void CMessageHandlerChainInstance::ChainedMessageHandler::init(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);
	CAF_CM_VALIDATE_STRING(_id);
	CAF_CM_VALIDATE_INTERFACE(_messageHandlerObj);
	// output channel is optional

	// The underlying component may need to be wired
	SmartPtrIIntegrationComponentInstance intInstance;
	intInstance.QueryInterface(_messageHandlerObj, false);
	if (intInstance) {
		intInstance->wire(appContext, channelResolver);
	}

	_messageHandler.CreateInstance();
	_messageHandler->initialize(_id, _outputChannel, _messageHandlerObj);
	_isInitialized = true;
}
void CMessageHandlerChainInstance::ChainedMessageHandler::setId(const std::string& id) {
	CAF_CM_FUNCNAME_VALIDATE("setId");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(id);
	_id = id;
}

void CMessageHandlerChainInstance::ChainedMessageHandler::setOutputChannel(
	const SmartPtrIMessageChannel& channel) {
	CAF_CM_FUNCNAME_VALIDATE("setOutputChannel");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(channel);
	_outputChannel = channel;
}

void CMessageHandlerChainInstance::ChainedMessageHandler::setMessageHandler(
	const SmartPtrICafObject& handlerObj) {
	CAF_CM_FUNCNAME_VALIDATE("setMessageHandler");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(handlerObj);
	_messageHandlerObj = handlerObj;
}

void CMessageHandlerChainInstance::ChainedMessageHandler::handleMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("handleMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);
	//logMessage(message);
	_messageHandler->handleMessage(message);
}

SmartPtrIIntMessage CMessageHandlerChainInstance::ChainedMessageHandler::getSavedMessage() const {
	CAF_CM_FUNCNAME_VALIDATE("getSavedMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _messageHandler->getSavedMessage();
}

void CMessageHandlerChainInstance::ChainedMessageHandler::clearSavedMessage() {
	CAF_CM_FUNCNAME_VALIDATE("clearSavedMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_messageHandler->clearSavedMessage();
}

CMessageHandlerChainInstance::InterconnectChannel::InterconnectChannel() :
	CAF_CM_INIT("CMessageHandlerChainInstance::InterconnectChannel") {
}

CMessageHandlerChainInstance::InterconnectChannel::~InterconnectChannel() {
}

void CMessageHandlerChainInstance::InterconnectChannel::init(
	const SmartPtrChainedMessageHandler& nextHandler) {
	_nextHandler = nextHandler;
}

bool CMessageHandlerChainInstance::InterconnectChannel::send(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("send");
	CAF_CM_VALIDATE_SMARTPTR(_nextHandler);
	_nextHandler->handleMessage(message);
	return true;
}

bool CMessageHandlerChainInstance::InterconnectChannel::send(
	const SmartPtrIIntMessage& message,
	const int32 timeout) {
	return send(message);
}

CMessageHandlerChainInstance::SelfWeakReference::SelfWeakReference() :
	_reference(NULL) {
	CAF_CM_INIT_THREADSAFE;
}

void CMessageHandlerChainInstance::SelfWeakReference::setReference(
	IMessageHandler *handler) {
	CAF_CM_LOCK_UNLOCK;
	_reference = handler;
}

void CMessageHandlerChainInstance::SelfWeakReference::handleMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_LOCK_UNLOCK;
	if (_reference) {
		_reference->handleMessage(message);
	}
}

SmartPtrIIntMessage CMessageHandlerChainInstance::SelfWeakReference::getSavedMessage() const {
	CAF_CM_LOCK_UNLOCK;
	SmartPtrIIntMessage savedMessage;
	if (_reference) {
		savedMessage = _reference->getSavedMessage();
	}

	return savedMessage;
}

void CMessageHandlerChainInstance::SelfWeakReference::clearSavedMessage() {
	CAF_CM_LOCK_UNLOCK;
	if (_reference) {
		_reference->clearSavedMessage();
	}
}

void CMessageHandlerChainInstance::logMessage(
	const SmartPtrIIntMessage& message) const {
	CAF_CM_FUNCNAME_VALIDATE("logMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const std::string prefix = _id.empty() ? "NULL" : _id;
	if (message.IsNull()) {
		CAF_CM_LOG_DEBUG_VA1("%s - NULL Message", prefix.c_str());
	} else {
		CAF_CM_LOG_DEBUG_VA2("%s - payload: %s", prefix.c_str(),
			message->getPayloadStr().c_str());
		CMessageHeaderUtils::log(message->getHeaders());
	}
}

void CMessageHandlerChainInstance::ChainedMessageHandler::logMessage(
	const SmartPtrIIntMessage& message) const {
	CAF_CM_STATIC_FUNC_LOG_ONLY("ChainedMessageHandler", "logMessage");

	const std::string prefix = _id.empty() ? "NULL" : _id;
	if (message.IsNull()) {
		CAF_CM_LOG_DEBUG_VA1("%s - NULL Message", prefix.c_str());
	} else {
		CAF_CM_LOG_DEBUG_VA2("%s - payload: %s", prefix.c_str(),
			message->getPayloadStr().c_str());
		CMessageHeaderUtils::log(message->getHeaders());
	}
}
