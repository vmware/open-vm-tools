/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/MgmtTypesDoc/CMgmtCollectInstancesCollectionDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectSchemaDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtInvokeOperationCollectionDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtBatchDoc.h"

using namespace Caf;

/// A simple container for objects of type MgmtBatch
CMgmtBatchDoc::CMgmtBatchDoc() :
	_isInitialized(false) {}
CMgmtBatchDoc::~CMgmtBatchDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CMgmtBatchDoc::initialize(
	const SmartPtrCMgmtCollectSchemaDoc collectSchema,
	const SmartPtrCMgmtCollectInstancesCollectionDoc collectInstancesCollection,
	const SmartPtrCMgmtInvokeOperationCollectionDoc invokeOperationCollection) {
	if (! _isInitialized) {
		_collectSchema = collectSchema;
		_collectInstancesCollection = collectInstancesCollection;
		_invokeOperationCollection = invokeOperationCollection;

		_isInitialized = true;
	}
}

/// Accessor for the CollectSchema
SmartPtrCMgmtCollectSchemaDoc CMgmtBatchDoc::getCollectSchema() const {
	return _collectSchema;
}

/// Accessor for the CollectInstancesCollection
SmartPtrCMgmtCollectInstancesCollectionDoc CMgmtBatchDoc::getCollectInstancesCollection() const {
	return _collectInstancesCollection;
}

/// Accessor for the InvokeOperationCollection
SmartPtrCMgmtInvokeOperationCollectionDoc CMgmtBatchDoc::getInvokeOperationCollection() const {
	return _invokeOperationCollection;
}






