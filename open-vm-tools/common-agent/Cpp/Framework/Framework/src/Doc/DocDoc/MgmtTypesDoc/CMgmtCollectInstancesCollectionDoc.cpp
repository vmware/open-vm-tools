/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/MgmtTypesDoc/CMgmtCollectInstancesDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectInstancesCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type MgmtCollectInstancesCollection
CMgmtCollectInstancesCollectionDoc::CMgmtCollectInstancesCollectionDoc() :
	_isInitialized(false) {}
CMgmtCollectInstancesCollectionDoc::~CMgmtCollectInstancesCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CMgmtCollectInstancesCollectionDoc::initialize(
	const std::deque<SmartPtrCMgmtCollectInstancesDoc> collectInstancesCollection) {
	if (! _isInitialized) {
		_collectInstancesCollection = collectInstancesCollection;

		_isInitialized = true;
	}
}

/// Accessor for the CollectInstances
std::deque<SmartPtrCMgmtCollectInstancesDoc> CMgmtCollectInstancesCollectionDoc::getCollectInstancesCollection() const {
	return _collectInstancesCollection;
}






