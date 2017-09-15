/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/Core/CIntException.h"
#include "Integration/IErrorHandler.h"
#include "Integration/IIntMessage.h"
#include "Integration/IMessageHandler.h"
#include "Integration/Core/CUnicastingDispatcher.h"

using namespace Caf;

CUnicastingDispatcher::CUnicastingDispatcher() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CUnicastingDispatcher") {
}

CUnicastingDispatcher::~CUnicastingDispatcher() {
}

void CUnicastingDispatcher::initialize(
	const SmartPtrIErrorHandler& errorHandler) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(errorHandler);

	_errorHandler = errorHandler;
	_messageHandlerCollection.CreateInstance();

	_isInitialized = true;
}

void CUnicastingDispatcher::addHandler(
	const SmartPtrIMessageHandler& messageHandler) {
	CAF_CM_FUNCNAME_VALIDATE("addHandler");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(messageHandler);

	const void* handlerPtr = messageHandler.GetNonAddRefedInterface();
	_messageHandlerCollection->insert(std::make_pair(handlerPtr, messageHandler));

#ifdef __x86_64__
	CAF_CM_LOG_DEBUG_VA1("Added handler - %llX", handlerPtr);
#else
	CAF_CM_LOG_DEBUG_VA1("Added handler - %X", handlerPtr);
#endif
}

void CUnicastingDispatcher::removeHandler(
	const SmartPtrIMessageHandler& messageHandler) {
	CAF_CM_FUNCNAME_VALIDATE("removeHandler");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(messageHandler);

	const void* handlerPtr = messageHandler.GetNonAddRefedInterface();
	_messageHandlerCollection->erase(handlerPtr);

#ifdef __x86_64__
	CAF_CM_LOG_DEBUG_VA1("Removed handler - %llX", handlerPtr);
#else
	CAF_CM_LOG_DEBUG_VA1("Removed handler - %X", handlerPtr);
#endif
}

bool CUnicastingDispatcher::dispatch(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("dispatch");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);

	bool isMessageHandled = false;

	for (TSmartConstMapIterator<CIntMessageHandlerCollection> messageHandlerIter(*_messageHandlerCollection);
		! isMessageHandled && messageHandlerIter; messageHandlerIter++) {
		const SmartPtrIMessageHandler messageHandler = *messageHandlerIter;

#ifdef __x86_64__
		CAF_CM_LOG_DEBUG_VA1("Dispatching to handler - %llX", messageHandlerIter.getKey());
#else
		CAF_CM_LOG_DEBUG_VA1("Dispatching to handler - %X", messageHandlerIter.getKey());
#endif
		try {
			messageHandler->handleMessage(message);
			isMessageHandled = true;
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;

		try {
			if (CAF_CM_ISEXCEPTION) {
				CAF_CM_VALIDATE_INTERFACE(messageHandler);
				SmartPtrIIntMessage savedMessage = messageHandler->getSavedMessage();
				if (savedMessage.IsNull()) {
					savedMessage = message;
				}

				SmartPtrCIntException intException;
				intException.CreateInstance();
				intException->initialize(CAF_CM_GETEXCEPTION);
				_errorHandler->handleError(intException, savedMessage);

				CAF_CM_CLEAREXCEPTION;
			}
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_CLEAREXCEPTION;
	}

	return isMessageHandled;
}
