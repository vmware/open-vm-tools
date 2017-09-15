/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/ProviderRequestDoc/CProviderCollectInstancesCollectionDoc.h"
#include "Doc/ProviderRequestDoc/CProviderInvokeOperationCollectionDoc.h"
#include "Doc/ProviderRequestDoc/CProviderBatchDoc.h"

using namespace Caf;

/// A simple container for objects of type ProviderBatch
CProviderBatchDoc::CProviderBatchDoc() :
	_isInitialized(false) {}
CProviderBatchDoc::~CProviderBatchDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CProviderBatchDoc::initialize(
	const std::string outputDir,
	const SmartPtrCProviderCollectInstancesCollectionDoc collectInstancesCollection,
	const SmartPtrCProviderInvokeOperationCollectionDoc invokeOperationCollection) {
	if (! _isInitialized) {
		_outputDir = outputDir;
		_collectInstancesCollection = collectInstancesCollection;
		_invokeOperationCollection = invokeOperationCollection;

		_isInitialized = true;
	}
}

/// Accessor for the OutputDir
std::string CProviderBatchDoc::getOutputDir() const {
	return _outputDir;
}

/// Accessor for the CollectInstancesCollection
SmartPtrCProviderCollectInstancesCollectionDoc CProviderBatchDoc::getCollectInstancesCollection() const {
	return _collectInstancesCollection;
}

/// Accessor for the InvokeOperationCollection
SmartPtrCProviderInvokeOperationCollectionDoc CProviderBatchDoc::getInvokeOperationCollection() const {
	return _invokeOperationCollection;
}






