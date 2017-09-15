/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/DiagTypesDoc/CDiagDeleteValueDoc.h"
#include "Doc/DiagTypesDoc/CDiagDeleteValueCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type DiagDeleteValueCollection
CDiagDeleteValueCollectionDoc::CDiagDeleteValueCollectionDoc() :
	_isInitialized(false) {}
CDiagDeleteValueCollectionDoc::~CDiagDeleteValueCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CDiagDeleteValueCollectionDoc::initialize(
	const std::deque<SmartPtrCDiagDeleteValueDoc> deleteValueCollection) {
	if (! _isInitialized) {
		_deleteValueCollection = deleteValueCollection;

		_isInitialized = true;
	}
}

/// Accessor for the DeleteValue
std::deque<SmartPtrCDiagDeleteValueDoc> CDiagDeleteValueCollectionDoc::getDeleteValueCollection() const {
	return _deleteValueCollection;
}






