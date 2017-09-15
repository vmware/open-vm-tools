/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CAddInsDoc.h"
#include "Doc/CafCoreTypesDoc/CLoggingLevelCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestConfigDoc.h"

using namespace Caf;

/// A simple container for objects of type RequestConfig
CRequestConfigDoc::CRequestConfigDoc() :
	_isInitialized(false) {}
CRequestConfigDoc::~CRequestConfigDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CRequestConfigDoc::initialize(
	const std::string responseFormatType,
	const SmartPtrCAddInsDoc requestProcessorAddIns,
	const SmartPtrCAddInsDoc responseProcessorAddIns,
	const SmartPtrCLoggingLevelCollectionDoc loggingLevelCollection) {
	if (! _isInitialized) {
		_responseFormatType = responseFormatType;
		_requestProcessorAddIns = requestProcessorAddIns;
		_responseProcessorAddIns = responseProcessorAddIns;
		_loggingLevelCollection = loggingLevelCollection;

		_isInitialized = true;
	}
}

/// Accessor for the ResponseFormatType
std::string CRequestConfigDoc::getResponseFormatType() const {
	return _responseFormatType;
}

/// Accessor for the RequestProcessorAddIns
SmartPtrCAddInsDoc CRequestConfigDoc::getRequestProcessorAddIns() const {
	return _requestProcessorAddIns;
}

/// Accessor for the ResponseProcessorAddIns
SmartPtrCAddInsDoc CRequestConfigDoc::getResponseProcessorAddIns() const {
	return _responseProcessorAddIns;
}

/// Accessor for the LoggingLevelCollection
SmartPtrCLoggingLevelCollectionDoc CRequestConfigDoc::getLoggingLevelCollection() const {
	return _loggingLevelCollection;
}






