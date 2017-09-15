/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CStatisticsDoc.h"
#include "Doc/ResponseDoc/CEventKeyCollectionDoc.h"
#include "Doc/ResponseDoc/CEventManifestDoc.h"
#include "Doc/ResponseDoc/CResponseHeaderDoc.h"
#include "Doc/ResponseDoc/CProviderEventResponseDoc.h"

using namespace Caf;

/// A simple container for objects of type ProviderEventResponse
CProviderEventResponseDoc::CProviderEventResponseDoc() :
	_isInitialized(false) {}
CProviderEventResponseDoc::~CProviderEventResponseDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CProviderEventResponseDoc::initialize(
	const std::string pmeId,
	const SmartPtrCResponseHeaderDoc responseHeader,
	const SmartPtrCEventManifestDoc manifest,
	const SmartPtrCEventKeyCollectionDoc eventKeyCollection,
	const SmartPtrCStatisticsDoc statistics) {
	if (! _isInitialized) {
		_pmeId = pmeId;
		_responseHeader = responseHeader;
		_manifest = manifest;
		_eventKeyCollection = eventKeyCollection;
		_statistics = statistics;

		_isInitialized = true;
	}
}

/// Accessor for the PmeId
std::string CProviderEventResponseDoc::getPmeId() const {
	return _pmeId;
}

/// Accessor for the ResponseHeader
SmartPtrCResponseHeaderDoc CProviderEventResponseDoc::getResponseHeader() const {
	return _responseHeader;
}

/// Accessor for the Manifest
SmartPtrCEventManifestDoc CProviderEventResponseDoc::getManifest() const {
	return _manifest;
}

/// Accessor for the EventKeyCollection
SmartPtrCEventKeyCollectionDoc CProviderEventResponseDoc::getEventKeyCollection() const {
	return _eventKeyCollection;
}

/// Accessor for the Statistics
SmartPtrCStatisticsDoc CProviderEventResponseDoc::getStatistics() const {
	return _statistics;
}






