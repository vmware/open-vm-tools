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
#include "Doc/CafInstallRequestDoc/CInstallBatchDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallRequestDoc.h"

using namespace Caf;

/// A simple container for objects of type InstallRequest
CInstallRequestDoc::CInstallRequestDoc() :
	_clientId(CAFCOMMON_GUID_NULL),
	_requestId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CInstallRequestDoc::~CInstallRequestDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CInstallRequestDoc::initialize(
	const UUID clientId,
	const UUID requestId,
	const std::string pmeId,
	const SmartPtrCRequestHeaderDoc requestHeader,
	const SmartPtrCInstallBatchDoc batch,
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
UUID CInstallRequestDoc::getClientId() const {
	return _clientId;
}

/// Accessor for the RequestId
UUID CInstallRequestDoc::getRequestId() const {
	return _requestId;
}

/// Accessor for the PmeId
std::string CInstallRequestDoc::getPmeId() const {
	return _pmeId;
}

/// Accessor for the RequestHeader
SmartPtrCRequestHeaderDoc CInstallRequestDoc::getRequestHeader() const {
	return _requestHeader;
}

/// Accessor for the Batch
SmartPtrCInstallBatchDoc CInstallRequestDoc::getBatch() const {
	return _batch;
}

/// Accessor for the AttachmentCollection
SmartPtrCAttachmentCollectionDoc CInstallRequestDoc::getAttachmentCollection() const {
	return _attachmentCollection;
}





