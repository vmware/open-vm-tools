/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CPropertyDoc.h"
#include "Doc/CafCoreTypesDoc/CPropertyCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type PropertyCollection
CPropertyCollectionDoc::CPropertyCollectionDoc() :
	_isInitialized(false) {}
CPropertyCollectionDoc::~CPropertyCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CPropertyCollectionDoc::initialize(
	const std::deque<SmartPtrCPropertyDoc> property) {
	if (! _isInitialized) {
		_property = property;

		_isInitialized = true;
	}
}

/// Accessor for the Property
std::deque<SmartPtrCPropertyDoc> CPropertyCollectionDoc::getProperty() const {
	return _property;
}






