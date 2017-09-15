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

using namespace Caf;

/// A simple container for objects of type AddInCollection
CAddInCollectionDoc::CAddInCollectionDoc() :
	_isInitialized(false) {}
CAddInCollectionDoc::~CAddInCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CAddInCollectionDoc::initialize(
	const std::deque<std::string> addInCollection) {
	if (! _isInitialized) {
		_addInCollection = addInCollection;

		_isInitialized = true;
	}
}

/// Accessor for the AddIn
std::deque<std::string> CAddInCollectionDoc::getAddInCollection() const {
	return _addInCollection;
}






