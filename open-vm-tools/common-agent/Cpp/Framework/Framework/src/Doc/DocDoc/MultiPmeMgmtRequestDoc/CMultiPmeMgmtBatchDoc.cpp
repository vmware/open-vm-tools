/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/MgmtTypesDoc/CMgmtBatchDoc.h"
#include "Doc/MultiPmeMgmtRequestDoc/CPmeIdCollectionDoc.h"
#include "Doc/MultiPmeMgmtRequestDoc/CMultiPmeMgmtBatchDoc.h"

using namespace Caf;

/// A simple container for objects of type MultiPmeMgmtBatch
CMultiPmeMgmtBatchDoc::CMultiPmeMgmtBatchDoc() :
	_isInitialized(false) {}
CMultiPmeMgmtBatchDoc::~CMultiPmeMgmtBatchDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CMultiPmeMgmtBatchDoc::initialize(
	const SmartPtrCPmeIdCollectionDoc pmeIdCollection,
	const SmartPtrCMgmtBatchDoc batch) {
	if (! _isInitialized) {
		_pmeIdCollection = pmeIdCollection;
		_batch = batch;

		_isInitialized = true;
	}
}

/// Accessor for the PmeIdCollection
SmartPtrCPmeIdCollectionDoc CMultiPmeMgmtBatchDoc::getPmeIdCollection() const {
	return _pmeIdCollection;
}

/// Accessor for the Batch
SmartPtrCMgmtBatchDoc CMultiPmeMgmtBatchDoc::getBatch() const {
	return _batch;
}






