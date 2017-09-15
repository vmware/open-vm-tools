/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/MgmtTypesDoc/CMgmtInvokeOperationCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type MgmtInvokeOperationCollection
CMgmtInvokeOperationCollectionDoc::CMgmtInvokeOperationCollectionDoc() :
	_isInitialized(false) {}
CMgmtInvokeOperationCollectionDoc::~CMgmtInvokeOperationCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CMgmtInvokeOperationCollectionDoc::initialize(
	const std::deque<SmartPtrCMgmtInvokeOperationDoc> invokeOperationCollection) {
	if (! _isInitialized) {
		_invokeOperationCollection = invokeOperationCollection;

		_isInitialized = true;
	}
}

/// Accessor for the InvokeOperation
std::deque<SmartPtrCMgmtInvokeOperationDoc> CMgmtInvokeOperationCollectionDoc::getInvokeOperationCollection() const {
	return _invokeOperationCollection;
}






