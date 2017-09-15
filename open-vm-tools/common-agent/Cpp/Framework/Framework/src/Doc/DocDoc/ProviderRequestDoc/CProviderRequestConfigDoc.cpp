/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CLoggingLevelCollectionDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestConfigDoc.h"

using namespace Caf;

/// A simple container for objects of type ProviderRequestConfig
CProviderRequestConfigDoc::CProviderRequestConfigDoc() :
	_isInitialized(false) {}
CProviderRequestConfigDoc::~CProviderRequestConfigDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CProviderRequestConfigDoc::initialize(
	const std::string responseFormatType,
	const SmartPtrCLoggingLevelCollectionDoc loggingLevelCollection) {
	if (! _isInitialized) {
		_responseFormatType = responseFormatType;
		_loggingLevelCollection = loggingLevelCollection;

		_isInitialized = true;
	}
}

/// Accessor for the ResponseFormatType
std::string CProviderRequestConfigDoc::getResponseFormatType() const {
	return _responseFormatType;
}

/// Accessor for the LoggingLevelCollection
SmartPtrCLoggingLevelCollectionDoc CProviderRequestConfigDoc::getLoggingLevelCollection() const {
	return _loggingLevelCollection;
}






