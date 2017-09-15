/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CRequestHeaderDoc.h"
#include "Doc/DiagTypesDoc/CDiagBatchDoc.h"
#include "Doc/DiagRequestDoc/CDiagRequestDoc.h"

using namespace Caf;

/// A simple container for objects of type DiagRequest
CDiagRequestDoc::CDiagRequestDoc() :
	_clientId(CAFCOMMON_GUID_NULL),
	_requestId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CDiagRequestDoc::~CDiagRequestDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CDiagRequestDoc::initialize(
	const UUID clientId,
	const UUID requestId,
	const std::string pmeId,
	const SmartPtrCRequestHeaderDoc requestHeader,
	const SmartPtrCDiagBatchDoc batch) {
	if (! _isInitialized) {
		_clientId = clientId;
		_requestId = requestId;
		_pmeId = pmeId;
		_requestHeader = requestHeader;
		_batch = batch;

		_isInitialized = true;
	}
}

/// Accessor for the ClientId
UUID CDiagRequestDoc::getClientId() const {
	return _clientId;
}

/// Accessor for the RequestId
UUID CDiagRequestDoc::getRequestId() const {
	return _requestId;
}

/// Accessor for the PmeId
std::string CDiagRequestDoc::getPmeId() const {
	return _pmeId;
}

/// Accessor for the RequestHeader
SmartPtrCRequestHeaderDoc CDiagRequestDoc::getRequestHeader() const {
	return _requestHeader;
}

/// Accessor for the Batch
SmartPtrCDiagBatchDoc CDiagRequestDoc::getBatch() const {
	return _batch;
}





