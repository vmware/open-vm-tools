/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CPropertyCollectionDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestConfigDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestHeaderDoc.h"

using namespace Caf;

/// A simple container for objects of type ProviderRequestHeader
CProviderRequestHeaderDoc::CProviderRequestHeaderDoc() :
	_isInitialized(false) {}
CProviderRequestHeaderDoc::~CProviderRequestHeaderDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CProviderRequestHeaderDoc::initialize(
	const SmartPtrCProviderRequestConfigDoc requestConfig,
	const SmartPtrCPropertyCollectionDoc echoPropertyBag) {
	if (! _isInitialized) {
		_requestConfig = requestConfig;
		_echoPropertyBag = echoPropertyBag;

		_isInitialized = true;
	}
}

/// Accessor for the RequestConfig
SmartPtrCProviderRequestConfigDoc CProviderRequestHeaderDoc::getRequestConfig() const {
	return _requestConfig;
}

/// Accessor for the EchoPropertyBag
SmartPtrCPropertyCollectionDoc CProviderRequestHeaderDoc::getEchoPropertyBag() const {
	return _echoPropertyBag;
}






