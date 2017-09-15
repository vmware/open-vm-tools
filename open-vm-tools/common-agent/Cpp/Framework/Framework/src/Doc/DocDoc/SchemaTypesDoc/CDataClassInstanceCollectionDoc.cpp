/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/SchemaTypesDoc/CDataClassInstanceDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassInstanceCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type DataClassInstanceCollection
CDataClassInstanceCollectionDoc::CDataClassInstanceCollectionDoc() :
	_isInitialized(false) {}
CDataClassInstanceCollectionDoc::~CDataClassInstanceCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CDataClassInstanceCollectionDoc::initialize(
	const std::deque<SmartPtrCDataClassInstanceDoc> dataClassInstanceCollection) {
	if (! _isInitialized) {
		_dataClassInstanceCollection = dataClassInstanceCollection;

		_isInitialized = true;
	}
}

/// Accessor for the DataClassInstance
std::deque<SmartPtrCDataClassInstanceDoc> CDataClassInstanceCollectionDoc::getDataClassInstanceCollection() const {
	return _dataClassInstanceCollection;
}






