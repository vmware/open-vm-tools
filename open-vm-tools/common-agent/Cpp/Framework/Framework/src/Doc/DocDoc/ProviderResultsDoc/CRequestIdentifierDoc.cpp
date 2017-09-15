/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/SchemaTypesDoc/CActionClassDoc.h"
#include "Doc/ProviderResultsDoc/CRequestIdentifierDoc.h"

using namespace Caf;

/// Fields that allow client to determine which request resulted in this response document
CRequestIdentifierDoc::CRequestIdentifierDoc() :
	_clientId(CAFCOMMON_GUID_NULL),
	_requestId(CAFCOMMON_GUID_NULL),
	_jobId(CAFCOMMON_GUID_NULL),
	_sessionId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CRequestIdentifierDoc::~CRequestIdentifierDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CRequestIdentifierDoc::initialize(
	const UUID clientId,
	const UUID requestId,
	const std::string pmeId,
	const UUID jobId,
	const SmartPtrCActionClassDoc actionClass,
	const UUID sessionId) {
	if (! _isInitialized) {
		_clientId = clientId;
		_requestId = requestId;
		_pmeId = pmeId;
		_jobId = jobId;
		_actionClass = actionClass;
		_sessionId = sessionId;

		_isInitialized = true;
	}
}

/// Accessor for the ClientId
UUID CRequestIdentifierDoc::getClientId() const {
	return _clientId;
}

/// Accessor for the RequestId
UUID CRequestIdentifierDoc::getRequestId() const {
	return _requestId;
}

/// Accessor for the PmeId
std::string CRequestIdentifierDoc::getPmeId() const {
	return _pmeId;
}

/// Identifier of the specific job within the request
UUID CRequestIdentifierDoc::getJobId() const {
	return _jobId;
}

/// Accessor for the ActionClass
SmartPtrCActionClassDoc CRequestIdentifierDoc::getActionClass() const {
	return _actionClass;
}

/// Client-configurable identifier that is opaque (not used) by the Common Agent Framework
UUID CRequestIdentifierDoc::getSessionId() const {
	return _sessionId;
}





