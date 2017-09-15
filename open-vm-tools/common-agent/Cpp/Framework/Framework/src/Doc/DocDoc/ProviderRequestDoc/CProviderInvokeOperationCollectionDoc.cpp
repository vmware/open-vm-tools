/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/ProviderRequestDoc/CProviderInvokeOperationDoc.h"
#include "Doc/ProviderRequestDoc/CProviderInvokeOperationCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type ProviderInvokeOperationCollection
CProviderInvokeOperationCollectionDoc::CProviderInvokeOperationCollectionDoc() :
	_isInitialized(false) {}
CProviderInvokeOperationCollectionDoc::~CProviderInvokeOperationCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CProviderInvokeOperationCollectionDoc::initialize(
	const std::deque<SmartPtrCProviderInvokeOperationDoc> invokeOperation) {
	if (! _isInitialized) {
		_invokeOperation = invokeOperation;

		_isInitialized = true;
	}
}

/// Accessor for the InvokeOperation
std::deque<SmartPtrCProviderInvokeOperationDoc> CProviderInvokeOperationCollectionDoc::getInvokeOperation() const {
	return _invokeOperation;
}






