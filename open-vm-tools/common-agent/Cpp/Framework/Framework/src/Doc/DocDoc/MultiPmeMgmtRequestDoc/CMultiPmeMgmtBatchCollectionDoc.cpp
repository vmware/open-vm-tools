/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/MultiPmeMgmtRequestDoc/CMultiPmeMgmtBatchDoc.h"
#include "Doc/MultiPmeMgmtRequestDoc/CMultiPmeMgmtBatchCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type MultiPmeMgmtBatchCollection
CMultiPmeMgmtBatchCollectionDoc::CMultiPmeMgmtBatchCollectionDoc() :
	_isInitialized(false) {}
CMultiPmeMgmtBatchCollectionDoc::~CMultiPmeMgmtBatchCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CMultiPmeMgmtBatchCollectionDoc::initialize(
	const std::deque<SmartPtrCMultiPmeMgmtBatchDoc> multiPmeBatch) {
	if (! _isInitialized) {
		_multiPmeBatch = multiPmeBatch;

		_isInitialized = true;
	}
}

/// Accessor for the MultiPmeBatch
std::deque<SmartPtrCMultiPmeMgmtBatchDoc> CMultiPmeMgmtBatchCollectionDoc::getMultiPmeBatch() const {
	return _multiPmeBatch;
}






