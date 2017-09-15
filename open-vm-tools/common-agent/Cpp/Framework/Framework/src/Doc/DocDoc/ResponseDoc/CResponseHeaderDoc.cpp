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

using namespace Caf;

/// A simple container for objects of type ResponseHeader
CResponseHeaderDoc::CResponseHeaderDoc() :
	_sequenceNumber(0),
	_isFinalResponse(true),
	_sessionId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CResponseHeaderDoc::~CResponseHeaderDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CResponseHeaderDoc::initialize(
	const std::string version,
	const std::string createdDateTime,
	const uint32 sequenceNumber,
	const bool isFinalResponse,
	const UUID sessionId) {
	if (! _isInitialized) {
		_version = version;
		_createdDateTime = createdDateTime;
		_sequenceNumber = sequenceNumber;
		_isFinalResponse = isFinalResponse;
		_sessionId = sessionId;

		_isInitialized = true;
	}
}

/// Accessor for the version
std::string CResponseHeaderDoc::getVersion() const {
	return _version;
}

/// Accessor for the date/time when the request was created
std::string CResponseHeaderDoc::getCreatedDateTime() const {
	return _createdDateTime;
}

/// Accessor for the sequenceNumber
uint32 CResponseHeaderDoc::getSequenceNumber() const {
	return _sequenceNumber;
}

/// Accessor for the version
bool CResponseHeaderDoc::getIsFinalResponse() const {
	return _isFinalResponse;
}

/// Accessor for the session ID
UUID CResponseHeaderDoc::getSessionId() const {
	return _sessionId;
}





