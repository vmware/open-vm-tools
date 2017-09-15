/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CAddInCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAddInsDoc.h"

using namespace Caf;

/// A simple container for objects of type AddIns
CAddInsDoc::CAddInsDoc() :
	_isInitialized(false) {}
CAddInsDoc::~CAddInsDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CAddInsDoc::initialize(
	const std::deque<SmartPtrCAddInCollectionDoc> addInCollection) {
	if (! _isInitialized) {
		_addInCollection = addInCollection;

		_isInitialized = true;
	}
}

/// Accessor for the AddInCollection
std::deque<SmartPtrCAddInCollectionDoc> CAddInsDoc::getAddInCollection() const {
	return _addInCollection;
}






