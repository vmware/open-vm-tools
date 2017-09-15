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
#include "Doc/ProviderRequestDoc/CProviderBatchDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestHeaderDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestDoc.h"

using namespace Caf;

/// A simple container for objects of type ProviderRequest
CProviderRequestDoc::CProviderRequestDoc() :
	_clientId(CAFCOMMON_GUID_NULL),
	_requestId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CProviderRequestDoc::~CProviderRequestDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CProviderRequestDoc::initialize(
	const UUID clientId,
	const UUID requestId,
	const std::string pmeId,
	const SmartPtrCProviderRequestHeaderDoc requestHeader,
	const SmartPtrCProviderBatchDoc batch,
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
UUID CProviderRequestDoc::getClientId() const {
	return _clientId;
}

/// Accessor for the RequestId
UUID CProviderRequestDoc::getRequestId() const {
	return _requestId;
}

/// Accessor for the PmeId
std::string CProviderRequestDoc::getPmeId() const {
	return _pmeId;
}

/// Accessor for the RequestHeader
SmartPtrCProviderRequestHeaderDoc CProviderRequestDoc::getRequestHeader() const {
	return _requestHeader;
}

/// Accessor for the Batch
SmartPtrCProviderBatchDoc CProviderRequestDoc::getBatch() const {
	return _batch;
}

/// Accessor for the AttachmentCollection
SmartPtrCAttachmentCollectionDoc CProviderRequestDoc::getAttachmentCollection() const {
	return _attachmentCollection;
}





