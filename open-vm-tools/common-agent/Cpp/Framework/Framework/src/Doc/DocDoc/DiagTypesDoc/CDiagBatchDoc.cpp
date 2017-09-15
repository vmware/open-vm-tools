/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/DiagTypesDoc/CDiagCollectInstancesDoc.h"
#include "Doc/DiagTypesDoc/CDiagDeleteValueCollectionDoc.h"
#include "Doc/DiagTypesDoc/CDiagSetValueCollectionDoc.h"
#include "Doc/DiagTypesDoc/CDiagBatchDoc.h"

using namespace Caf;

/// A simple container for objects of type DiagBatch
CDiagBatchDoc::CDiagBatchDoc() :
	_isInitialized(false) {}
CDiagBatchDoc::~CDiagBatchDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CDiagBatchDoc::initialize(
	const SmartPtrCDiagCollectInstancesDoc collectInstances,
	const SmartPtrCDiagSetValueCollectionDoc setValueCollection,
	const SmartPtrCDiagDeleteValueCollectionDoc deleteValueCollection) {
	if (! _isInitialized) {
		_collectInstances = collectInstances;
		_setValueCollection = setValueCollection;
		_deleteValueCollection = deleteValueCollection;

		_isInitialized = true;
	}
}

/// Accessor for the CollectInstances
SmartPtrCDiagCollectInstancesDoc CDiagBatchDoc::getCollectInstances() const {
	return _collectInstances;
}

/// Accessor for the SetValueCollection
SmartPtrCDiagSetValueCollectionDoc CDiagBatchDoc::getSetValueCollection() const {
	return _setValueCollection;
}

/// Accessor for the DeleteValueCollection
SmartPtrCDiagDeleteValueCollectionDoc CDiagBatchDoc::getDeleteValueCollection() const {
	return _deleteValueCollection;
}






