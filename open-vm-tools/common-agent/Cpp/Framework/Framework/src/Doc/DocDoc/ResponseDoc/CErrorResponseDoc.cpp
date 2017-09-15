/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/ResponseDoc/CResponseHeaderDoc.h"
#include "Doc/ResponseDoc/CErrorResponseDoc.h"

using namespace Caf;

/// A simple container for objects of type ErrorResponse
CErrorResponseDoc::CErrorResponseDoc() :
	_clientId(CAFCOMMON_GUID_NULL),
	_requestId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CErrorResponseDoc::~CErrorResponseDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CErrorResponseDoc::initialize(
	const UUID clientId,
	const UUID requestId,
	const std::string pmeId,
	const SmartPtrCResponseHeaderDoc responseHeader,
	const std::string errorMessage) {
	if (! _isInitialized) {
		_clientId = clientId;
		_requestId = requestId;
		_pmeId = pmeId;
		_responseHeader = responseHeader;
		_errorMessage = errorMessage;

		_isInitialized = true;
	}
}

/// Accessor for the ClientId
UUID CErrorResponseDoc::getClientId() const {
	return _clientId;
}

/// Accessor for the RequestId
UUID CErrorResponseDoc::getRequestId() const {
	return _requestId;
}

/// Accessor for the PmeId
std::string CErrorResponseDoc::getPmeId() const {
	return _pmeId;
}

/// Accessor for the Response Header
SmartPtrCResponseHeaderDoc CErrorResponseDoc::getResponseHeader() const {
	return _responseHeader;
}

/// Accessor for the ErrorMessage
std::string CErrorResponseDoc::getErrorMessage() const {
	return _errorMessage;
}





