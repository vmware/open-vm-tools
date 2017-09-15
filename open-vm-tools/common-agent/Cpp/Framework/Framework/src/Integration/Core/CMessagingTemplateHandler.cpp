/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IIntMessage.h"
#include "Integration/IMessageHandler.h"
#include "Integration/Core/CMessagingTemplateHandler.h"

using namespace Caf;

CMessagingTemplateHandler::CMessagingTemplateHandler() :
	_isInitialized(false),
	CAF_CM_INIT("CMessagingTemplateHandler") {
}

CMessagingTemplateHandler::~CMessagingTemplateHandler() {
}

void CMessagingTemplateHandler::initialize(
	const SmartPtrIMessageHandler& messageHandler) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(messageHandler);

	_messageHandler = messageHandler;
	_isInitialized = true;
}

void CMessagingTemplateHandler::handleMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("handleMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);

	_messageHandler->handleMessage(message);
}

SmartPtrIIntMessage CMessagingTemplateHandler::getSavedMessage() const {
	CAF_CM_FUNCNAME_VALIDATE("getSavedMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _messageHandler->getSavedMessage();
}

void CMessagingTemplateHandler::clearSavedMessage() {
	CAF_CM_FUNCNAME_VALIDATE("clearSavedMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_messageHandler->clearSavedMessage();;
}
