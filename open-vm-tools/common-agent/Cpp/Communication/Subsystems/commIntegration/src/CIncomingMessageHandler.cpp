/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "CIncomingMessageHandler.h"

using namespace Caf;

CIncomingMessageHandler::CIncomingMessageHandler() :
	_isInitialized(false),
	CAF_CM_INIT("CIncomingMessageHandler") {
}

CIncomingMessageHandler::~CIncomingMessageHandler() {
}

void CIncomingMessageHandler::initializeBean(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties) {
	CAF_CM_FUNCNAME_VALIDATE("initializeBean");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STL_EMPTY(ctorArgs);
	CAF_CM_VALIDATE_STL_EMPTY(properties);

	_ctorArgs = ctorArgs;
	_properties = properties;
	initialize();
}

void CIncomingMessageHandler::terminateBean() {
}

void CIncomingMessageHandler::initialize() {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_isInitialized = true;
}

bool CIncomingMessageHandler::isResponsible(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("isResponsible");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	return false;
}

SmartPtrIIntegrationObject CIncomingMessageHandler::createObject(
	const SmartPtrIDocument& configSection) const {
	CAF_CM_FUNCNAME_VALIDATE("createObject");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	SmartPtrIIntegrationObject rc;
	rc.CreateInstance(_sObjIdCommIntegrationIncomingMessageHandlerInstance);
	rc->initialize(_ctorArgs, _properties, configSection);

	return rc;
}
