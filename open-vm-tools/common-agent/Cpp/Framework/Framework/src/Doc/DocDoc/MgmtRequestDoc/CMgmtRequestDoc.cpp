/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestHeaderDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtBatchDoc.h"
#include "Doc/MgmtRequestDoc/CMgmtRequestDoc.h"

using namespace Caf;

/// A simple container for objects of type MgmtRequest
CMgmtRequestDoc::CMgmtRequestDoc() :
	_clientId(CAFCOMMON_GUID_NULL),
	_requestId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CMgmtRequestDoc::~CMgmtRequestDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CMgmtRequestDoc::initialize(
	const UUID clientId,
	const UUID requestId,
	const std::string pmeId,
	const SmartPtrCRequestHeaderDoc requestHeader,
	const SmartPtrCMgmtBatchDoc batch,
	const SmartPtrCAttachmentCollectionDoc attachmentCollection) {
	if (! _isInitialized) {
		_clientId = clientId;
		_requestId = requestId;
		_pmeId = pmeId;
		_requestHeader = requestHeader;
		_batch = batch;
		_attachmentCollection = attachmentCollection;

		_isInitialized = true;
	}
}

/// Accessor for the ClientId
UUID CMgmtRequestDoc::getClientId() const {
	return _clientId;
}

/// Accessor for the RequestId
UUID CMgmtRequestDoc::getRequestId() const {
	return _requestId;
}

/// Accessor for the PmeId
std::string CMgmtRequestDoc::getPmeId() const {
	return _pmeId;
}

/// Accessor for the RequestHeader
SmartPtrCRequestHeaderDoc CMgmtRequestDoc::getRequestHeader() const {
	return _requestHeader;
}

/// Accessor for the Batch
SmartPtrCMgmtBatchDoc CMgmtRequestDoc::getBatch() const {
	return _batch;
}

/// Accessor for the AttachmentCollection
SmartPtrCAttachmentCollectionDoc CMgmtRequestDoc::getAttachmentCollection() const {
	return _attachmentCollection;
}





