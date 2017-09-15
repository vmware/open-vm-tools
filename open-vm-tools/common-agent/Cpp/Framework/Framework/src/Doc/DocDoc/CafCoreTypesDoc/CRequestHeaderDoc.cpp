/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CAuthnAuthzCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CPropertyCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CProtocolCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestConfigDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestHeaderDoc.h"

using namespace Caf;

/// A simple container for objects of type RequestHeader
CRequestHeaderDoc::CRequestHeaderDoc() :
	_isInitialized(false),
	_sessionId(CAFCOMMON_GUID_NULL) {}
CRequestHeaderDoc::~CRequestHeaderDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CRequestHeaderDoc::initialize(
	const SmartPtrCRequestConfigDoc requestConfig,
	const SmartPtrCAuthnAuthzCollectionDoc authnAuthzCollection,
	const SmartPtrCProtocolCollectionDoc protocolCollection,
	const SmartPtrCPropertyCollectionDoc echoPropertyBag,
	const std::string version,
	const std::string createdDateTime,
	const UUID sessionId) {
	if (! _isInitialized) {
		_requestConfig = requestConfig;
		_authnAuthzCollection = authnAuthzCollection;
		_protocolCollection = protocolCollection;
		_echoPropertyBag = echoPropertyBag;
		_version = version;
		_createdDateTime = createdDateTime;
		_sessionId = sessionId;

		_isInitialized = true;
	}
}

/// Accessor for the RequestConfig
SmartPtrCRequestConfigDoc CRequestHeaderDoc::getRequestConfig() const {
	return _requestConfig;
}

/// Accessor for the Authentication / Authorization Collection
SmartPtrCAuthnAuthzCollectionDoc CRequestHeaderDoc::getAuthnAuthzCollection() const {
	return _authnAuthzCollection;
}

/// Accessor for the Protocol Collection
SmartPtrCProtocolCollectionDoc CRequestHeaderDoc::getProtocolCollection() const {
	return _protocolCollection;
}

/// Accessor for the EchoPropertyBag
SmartPtrCPropertyCollectionDoc CRequestHeaderDoc::getEchoPropertyBag() const {
	return _echoPropertyBag;
}

/// Accessor for the version
std::string CRequestHeaderDoc::getVersion() const {
	return _version;
}

/// Accessor for the date/time when the request was created
std::string CRequestHeaderDoc::getCreatedDateTime() const {
	return _createdDateTime;
}

/// Accessor for the session ID
UUID CRequestHeaderDoc::getSessionId() const {
	return _sessionId;
}






