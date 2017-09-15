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
#include "Doc/MultiPmeMgmtRequestDoc/CMultiPmeMgmtBatchCollectionDoc.h"
#include "Doc/MultiPmeMgmtRequestDoc/CMultiPmeMgmtRequestDoc.h"

using namespace Caf;

/// A simple container for objects of type MultiPmeMgmtRequest
CMultiPmeMgmtRequestDoc::CMultiPmeMgmtRequestDoc() :
	_clientId(CAFCOMMON_GUID_NULL),
	_requestId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CMultiPmeMgmtRequestDoc::~CMultiPmeMgmtRequestDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CMultiPmeMgmtRequestDoc::initialize(
	const UUID clientId,
	const UUID requestId,
	const SmartPtrCRequestHeaderDoc requestHeader,
	const SmartPtrCMultiPmeMgmtBatchCollectionDoc multiPmeBatchCollection) {
	if (! _isInitialized) {
		_clientId = clientId;
		_requestId = requestId;
		_requestHeader = requestHeader;
		_multiPmeBatchCollection = multiPmeBatchCollection;

		_isInitialized = true;
	}
}

/// Accessor for the ClientId
UUID CMultiPmeMgmtRequestDoc::getClientId() const {
	return _clientId;
}

/// Accessor for the RequestId
UUID CMultiPmeMgmtRequestDoc::getRequestId() const {
	return _requestId;
}

/// Accessor for the RequestHeader
SmartPtrCRequestHeaderDoc CMultiPmeMgmtRequestDoc::getRequestHeader() const {
	return _requestHeader;
}

/// Accessor for the MultiPmeBatchCollection
SmartPtrCMultiPmeMgmtBatchCollectionDoc CMultiPmeMgmtRequestDoc::getMultiPmeBatchCollection() const {
	return _multiPmeBatchCollection;
}





