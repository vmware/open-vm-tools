/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/DocXml/PersistenceXml/PersistenceXmlRoots.h"

#include "Doc/PersistenceDoc/CPersistenceDoc.h"
#include "IPersistence.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "CPersistenceMessageHandler.h"

using namespace Caf;

CPersistenceMessageHandler::CPersistenceMessageHandler() :
		_isInitialized(false),
	CAF_CM_INIT_LOG("CPersistenceMessageHandler") {
}

CPersistenceMessageHandler::~CPersistenceMessageHandler() {
}

void CPersistenceMessageHandler::initialize(
		const SmartPtrIDocument& configSection,
		const SmartPtrIPersistence& persistence) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);
	CAF_CM_VALIDATE_SMARTPTR(persistence);

	_id = configSection->findRequiredAttribute("id");
	_persistence = persistence;

	_isInitialized = true;
}

void CPersistenceMessageHandler::handleMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("handleMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);

	_savedMessage = message;

	SmartPtrCPersistenceDoc persistence;
	const std::string payloadStr = message->getPayloadStr();
	if (! payloadStr.empty()) {
		persistence = XmlRoots::parsePersistenceFromString(payloadStr);
	}

	_persistence->update(persistence);
}

SmartPtrIIntMessage CPersistenceMessageHandler::getSavedMessage() const {
	CAF_CM_FUNCNAME_VALIDATE("getSavedMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _savedMessage;
}

void CPersistenceMessageHandler::clearSavedMessage() {
	CAF_CM_FUNCNAME_VALIDATE("clearSavedMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_savedMessage = NULL;
}

SmartPtrIIntMessage CPersistenceMessageHandler::processErrorMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("processErrorMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);

	_savedMessage = message;

	//TODO: Verify that I'm receiving the error message and decide what
	//to do with it.

	return SmartPtrIIntMessage();
}
