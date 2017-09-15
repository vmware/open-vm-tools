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
#include "Doc/CafCoreTypesDoc/CStatisticsDoc.h"
#include "Doc/ResponseDoc/CManifestDoc.h"
#include "Doc/ResponseDoc/CResponseHeaderDoc.h"
#include "Doc/ResponseDoc/CProviderResponseDoc.h"

using namespace Caf;

/// A simple container for objects of type ProviderResponse
CProviderResponseDoc::CProviderResponseDoc() :
	_clientId(CAFCOMMON_GUID_NULL),
	_requestId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CProviderResponseDoc::~CProviderResponseDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CProviderResponseDoc::initialize(
	const UUID clientId,
	const UUID requestId,
	const std::string pmeId,
	const SmartPtrCResponseHeaderDoc responseHeader,
	const SmartPtrCManifestDoc manifest,
	const SmartPtrCAttachmentCollectionDoc attachmentCollection,
	const SmartPtrCStatisticsDoc statistics) {
	if (! _isInitialized) {
		_clientId = clientId;
		_requestId = requestId;
		_pmeId = pmeId;
		_responseHeader = responseHeader;
		_manifest = manifest;
		_attachmentCollection = attachmentCollection;
		_statistics = statistics;

		_isInitialized = true;
	}
}

/// Accessor for the ClientId
UUID CProviderResponseDoc::getClientId() const {
	return _clientId;
}

/// Accessor for the RequestId
UUID CProviderResponseDoc::getRequestId() const {
	return _requestId;
}

/// Accessor for the PmeId
std::string CProviderResponseDoc::getPmeId() const {
	return _pmeId;
}

/// Accessor for the ResponseHeader
SmartPtrCResponseHeaderDoc CProviderResponseDoc::getResponseHeader() const {
	return _responseHeader;
}

/// Accessor for the Manifest
SmartPtrCManifestDoc CProviderResponseDoc::getManifest() const {
	return _manifest;
}

/// Accessor for the AttachmentCollection
SmartPtrCAttachmentCollectionDoc CProviderResponseDoc::getAttachmentCollection() const {
	return _attachmentCollection;
}

/// Accessor for the Statistics
SmartPtrCStatisticsDoc CProviderResponseDoc::getStatistics() const {
	return _statistics;
}





