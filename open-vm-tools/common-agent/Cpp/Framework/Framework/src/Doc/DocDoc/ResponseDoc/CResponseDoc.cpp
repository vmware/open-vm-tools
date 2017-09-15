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
#include "Doc/ResponseDoc/CManifestCollectionDoc.h"
#include "Doc/ResponseDoc/CResponseHeaderDoc.h"
#include "Doc/ResponseDoc/CResponseDoc.h"

using namespace Caf;

/// A simple container for objects of type Response
CResponseDoc::CResponseDoc() :
	_clientId(CAFCOMMON_GUID_NULL),
	_requestId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CResponseDoc::~CResponseDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CResponseDoc::initialize(
	const UUID clientId,
	const UUID requestId,
	const std::string pmeId,
	const SmartPtrCResponseHeaderDoc responseHeader,
	const SmartPtrCManifestCollectionDoc manifestCollection,
	const SmartPtrCAttachmentCollectionDoc attachmentCollection,
	const SmartPtrCStatisticsDoc statistics) {
	if (! _isInitialized) {
		_clientId = clientId;
		_requestId = requestId;
		_pmeId = pmeId;
		_responseHeader = responseHeader;
		_manifestCollection = manifestCollection;
		_attachmentCollection = attachmentCollection;
		_statistics = statistics;

		_isInitialized = true;
	}
}

/// Accessor for the ClientId
UUID CResponseDoc::getClientId() const {
	return _clientId;
}

/// Accessor for the RequestId
UUID CResponseDoc::getRequestId() const {
	return _requestId;
}

/// Accessor for the PmeId
std::string CResponseDoc::getPmeId() const {
	return _pmeId;
}

/// Accessor for the ManifestCollection
SmartPtrCResponseHeaderDoc CResponseDoc::getResponseHeader() const {
	return _responseHeader;
}

/// Accessor for the ManifestCollection
SmartPtrCManifestCollectionDoc CResponseDoc::getManifestCollection() const {
	return _manifestCollection;
}

/// Accessor for the AttachmentCollection
SmartPtrCAttachmentCollectionDoc CResponseDoc::getAttachmentCollection() const {
	return _attachmentCollection;
}

/// Accessor for the Statistics
SmartPtrCStatisticsDoc CResponseDoc::getStatistics() const {
	return _statistics;
}





