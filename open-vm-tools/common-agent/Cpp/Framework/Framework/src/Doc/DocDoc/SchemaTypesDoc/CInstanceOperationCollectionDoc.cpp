/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/SchemaTypesDoc/CInstanceOperationDoc.h"
#include "Doc/SchemaTypesDoc/CInstanceOperationCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type InstanceOperationCollection
CInstanceOperationCollectionDoc::CInstanceOperationCollectionDoc() :
	_isInitialized(false) {}
CInstanceOperationCollectionDoc::~CInstanceOperationCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CInstanceOperationCollectionDoc::initialize(
	const std::deque<SmartPtrCInstanceOperationDoc> instanceOperationCollection) {
	if (! _isInitialized) {
		_instanceOperationCollection = instanceOperationCollection;

		_isInitialized = true;
	}
}

/// Accessor for the InstanceOperation
std::deque<SmartPtrCInstanceOperationDoc> CInstanceOperationCollectionDoc::getInstanceOperationCollection() const {
	return _instanceOperationCollection;
}






