/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/SchemaTypesDoc/CActionClassInstanceDoc.h"
#include "Doc/SchemaTypesDoc/CActionClassInstanceCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type ActionClassInstanceCollection
CActionClassInstanceCollectionDoc::CActionClassInstanceCollectionDoc() :
	_isInitialized(false) {}
CActionClassInstanceCollectionDoc::~CActionClassInstanceCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CActionClassInstanceCollectionDoc::initialize(
	const std::deque<SmartPtrCActionClassInstanceDoc> actionClassInstanceCollection) {
	if (! _isInitialized) {
		_actionClassInstanceCollection = actionClassInstanceCollection;

		_isInitialized = true;
	}
}

/// Accessor for the ActionClassInstance
std::deque<SmartPtrCActionClassInstanceDoc> CActionClassInstanceCollectionDoc::getActionClassInstanceCollection() const {
	return _actionClassInstanceCollection;
}






