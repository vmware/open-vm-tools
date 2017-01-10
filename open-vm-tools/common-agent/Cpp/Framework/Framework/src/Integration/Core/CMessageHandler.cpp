/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "ICafObject.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageChannel.h"
#include "Integration/Core/CMessageHandler.h"
#include "Exception/CCafException.h"

using namespace Caf;

CMessageHandler::CMessageHandler() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CMessageHandler") {
}

CMessageHandler::~CMessageHandler() {
}

void CMessageHandler::initialize(
	const std::string& inputId,
	const SmartPtrIMessageChannel& outputMessageChannel,
	const SmartPtrICafObject& messageHandlerObj) {
	CAF_CM_FUNCNAME("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(inputId);
	// outputMessageChannel is optional
	//
	// messageHandlerObj optional but if provided must be one of
	//		ITransformer
	//		IMessageProcessor
	//		IMessageSplitter
	//		IMessageRouter
	//		IMessageHandler
	//
	// messageHandlerObj may also support IErrorProcessor

	_inputId = inputId;
	_outputMessageChannel = outputMessageChannel;

	if (messageHandlerObj) {
		_transformer.QueryInterface(messageHandlerObj, false);
		_errorProcessor.QueryInterface(messageHandlerObj, false);
		_messageProcessor.QueryInterface(messageHandlerObj, false);
		_messageSplitter.QueryInterface(messageHandlerObj, false);
		_messageRouter.QueryInterface(messageHandlerObj, false);
		_messageHandler.QueryInterface(messageHandlerObj, false);
		if (!_transformer &&
			!_errorProcessor &&
			!_messageProcessor &&
			!_messageSplitter &&
			!_messageRouter &&
			!_messageHandler) {
			CAF_CM_EXCEPTIONEX_VA1(
					InvalidArgumentException,
					0,
					"The messageHandler object '%s' does not have "
					"a supported endpoint interface",
					_inputId.c_str());
		}
	}

	_isInitialized = true;
}

std::string CMessageHandler::getInputId() const {
	CAF_CM_FUNCNAME_VALIDATE("getInputId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _inputId;
}

void CMessageHandler::handleMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("handleMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);

	_savedMessage = message;

	const std::string isThrowableStr =
		message->findOptionalHeaderAsString(MessageHeaders::_sIS_THROWABLE);
	const bool isThrowable =
		(isThrowableStr.empty() || (isThrowableStr.compare("false") == 0)) ? false : true;

	if (isThrowable) {
		if (! _errorProcessor.IsNull()) {
			_savedMessage = _errorProcessor->processErrorMessage(message);
			if (_savedMessage.IsNull()) {
				CAF_CM_LOG_WARN_VA1(
					"Error processing did not return a message - %s", _inputId.c_str());
			} else {
				CAF_CM_VALIDATE_INTERFACE(_outputMessageChannel);
				_outputMessageChannel->send(_savedMessage);
			}
		} else {
			CAF_CM_EXCEPTIONEX_VA1(IllegalStateException, ERROR_INVALID_STATE,
				"Nothing handled the throwable message - %s", _inputId.c_str());
		}
	} else {
		if (_inputId.compare("errorChannel") == 0) {
			CAF_CM_LOG_WARN_VA1(
				"Received non-error on error channel - %s", _inputId.c_str());
		} else if (! _messageSplitter.IsNull()) {
			const IMessageSplitter::SmartPtrCMessageCollection outputMessageCollection =
				_messageSplitter->splitMessage(message);
			if (outputMessageCollection.IsNull() || outputMessageCollection->empty()) {
				CAF_CM_LOG_WARN_VA1(
					"Splitter did not split the message - %s", _inputId.c_str());
			} else {
				CAF_CM_VALIDATE_INTERFACE(_outputMessageChannel);
				for (TSmartConstIterator<IMessageSplitter::CMessageCollection> outputMessageIter(*outputMessageCollection);
					outputMessageIter; outputMessageIter++) {
					_savedMessage = *outputMessageIter;
					_outputMessageChannel->send(_savedMessage);
				}
			}
		} else if (! _messageRouter.IsNull()) {
			CAF_CM_VALIDATE_BOOL(_outputMessageChannel.IsNull());
			_messageRouter->routeMessage(message);
		} else if (! _messageProcessor.IsNull()) {
			_savedMessage = _messageProcessor->processMessage(message);
			if (_savedMessage.IsNull()) {
				CAF_CM_LOG_WARN_VA1(
					"Message processing did not return a message - %s", _inputId.c_str());
			} else {
				CAF_CM_VALIDATE_INTERFACE(_outputMessageChannel);
				_outputMessageChannel->send(_savedMessage);
			}
		} else if (! _transformer.IsNull()) {
			_savedMessage = _transformer->transformMessage(message);
			if (_savedMessage.IsNull()) {
				CAF_CM_LOG_WARN_VA1(
					"Transform did not return a message - %s", _inputId.c_str());
			} else {
				CAF_CM_VALIDATE_INTERFACE(_outputMessageChannel);
				_outputMessageChannel->send(_savedMessage);
			}
		} else if (! _messageHandler.IsNull()) {
			_messageHandler->handleMessage(message);
		} else if (! _outputMessageChannel.IsNull()) {
			_outputMessageChannel->send(message);
		} else {
			CAF_CM_EXCEPTIONEX_VA1(IllegalStateException, ERROR_INVALID_STATE,
				"Nothing handled the message - %s", _inputId.c_str());
		}
	}
}

SmartPtrIIntMessage CMessageHandler::getSavedMessage() const {
	CAF_CM_FUNCNAME_VALIDATE("getSavedMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _savedMessage;
}

void CMessageHandler::clearSavedMessage() {
	CAF_CM_FUNCNAME_VALIDATE("clearSavedMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_savedMessage = NULL;
}
