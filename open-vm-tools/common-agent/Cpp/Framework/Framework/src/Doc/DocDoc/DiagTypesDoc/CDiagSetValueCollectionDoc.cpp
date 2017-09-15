/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/DiagTypesDoc/CDiagSetValueDoc.h"
#include "Doc/DiagTypesDoc/CDiagSetValueCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type DiagSetValueCollection
CDiagSetValueCollectionDoc::CDiagSetValueCollectionDoc() :
	_isInitialized(false) {}
CDiagSetValueCollectionDoc::~CDiagSetValueCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CDiagSetValueCollectionDoc::initialize(
	const std::deque<SmartPtrCDiagSetValueDoc> setValueCollection) {
	if (! _isInitialized) {
		_setValueCollection = setValueCollection;

		_isInitialized = true;
	}
}

/// Accessor for the SetValue
std::deque<SmartPtrCDiagSetValueDoc> CDiagSetValueCollectionDoc::getSetValueCollection() const {
	return _setValueCollection;
}






