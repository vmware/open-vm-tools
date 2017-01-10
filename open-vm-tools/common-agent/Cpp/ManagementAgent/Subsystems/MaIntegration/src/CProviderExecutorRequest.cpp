/*
 *	 Author: brets
 *  Created: Nov 20, 2015
 *
 *	Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CPropertyDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestDoc.h"
#include "Integration/IIntMessage.h"
#include "CProviderExecutorRequest.h"
#include "Exception/CCafException.h"
#include "CProviderExecutor.h"
#include "Integration/Caf/CCafMessagePayloadParser.h"

using namespace Caf;

CProviderExecutorRequest::CProviderExecutorRequest() :
		_isInitialized(false),
		CAF_CM_INIT_LOG("CProviderExecutorRequest") {
}

CProviderExecutorRequest::~CProviderExecutorRequest() {
}

void CProviderExecutorRequest::initialize(const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("initialize");

	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);

	CAF_CM_LOG_DEBUG_VA0("Called");

	_internalRequest = message;
	_request = CCafMessagePayloadParser::getProviderRequest(message->getPayload());

	std::deque<SmartPtrCPropertyDoc> properties =
			_request->getRequestHeader()->getEchoPropertyBag()->getProperty();

	std::string relDirectory;
	for(std::deque<SmartPtrCPropertyDoc>::const_iterator itr = properties.begin(); itr != properties.end(); itr++) {
		if ((*itr)->getName().compare("relDirectory") == 0) {
			relDirectory = (*itr)->getValue().front();
		} else if ((*itr)->getName().compare("providerUri") == 0) {
			_providerUri = (*itr)->getValue().front();
		}
	}

	if (relDirectory.empty() || _providerUri.empty()) {
		CAF_CM_EXCEPTIONEX_VA2(Caf::NoSuchElementException, ERROR_NOT_FOUND,
				"Missing provider request information - relDirectory: [%s]  providerUri: [%s]",
				relDirectory.c_str(), _providerUri.c_str());
	}

	const std::string configOutputDir = AppConfigUtils::getRequiredString(_sConfigOutputDir);
	_outputDir = FileSystemUtils::buildPath(configOutputDir, _sProviderHostArea, relDirectory);

	_isInitialized = true;
}

const SmartPtrCProviderRequestDoc CProviderExecutorRequest::getRequest() const {
	CAF_CM_FUNCNAME_VALIDATE("getRequest");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _request;
}

const SmartPtrIIntMessage CProviderExecutorRequest::getInternalRequest() const {
	CAF_CM_FUNCNAME_VALIDATE("getRequest");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _internalRequest;
}

const std::string& CProviderExecutorRequest::getOutputDirectory() const {
	CAF_CM_FUNCNAME_VALIDATE("getOutputDirectory");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _outputDir;
}

const std::string& CProviderExecutorRequest::getProviderUri() const {
	CAF_CM_FUNCNAME_VALIDATE("getProviderUri");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _providerUri;
}

