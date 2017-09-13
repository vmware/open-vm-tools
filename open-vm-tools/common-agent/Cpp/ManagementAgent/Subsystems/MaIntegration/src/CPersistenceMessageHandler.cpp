/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CPersistenceMessageHandler.h"

using namespace Caf;

CPersistenceMessageHandler::CPersistenceMessageHandler() :
		_isInitialized(false),
		_deleteSourceEntries(false),
	CAF_CM_INIT_LOG("CPersistenceMessageHandler") {
}

CPersistenceMessageHandler::~CPersistenceMessageHandler() {
}

void CPersistenceMessageHandler::initialize(
		const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	_id = configSection->findRequiredAttribute("id");

	const std::string implClass = configSection->findRequiredAttribute("impl-class");
	_persistence.CreateInstance(implClass.c_str());
	_persistence->initialize();

	const std::string deleteSourceEntriesStr = configSection->findOptionalAttribute(
		"delete-source-entries");
	_deleteSourceEntries = (deleteSourceEntriesStr.empty() || deleteSourceEntriesStr.compare("false") == 0) ? false : true;

	_isInitialized = true;
}

void CPersistenceMessageHandler::handleMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("handleMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);

	_savedMessage = message;

	const std::string payloadStr = message->getPayloadStr();
	const SmartPtrCPersistenceDoc persistence =
			XmlRoots::parsePersistenceFromString(payloadStr);
	_persistence->update(persistence);

	if (_deleteSourceEntries) {
		;//TODO: Delete entries like the localSecurity private key.
	}
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
