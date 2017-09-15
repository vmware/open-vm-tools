/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/ProviderRequestDoc/CProviderCollectInstancesDoc.h"
#include "Doc/ProviderRequestDoc/CProviderCollectInstancesCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type ProviderCollectInstancesCollection
CProviderCollectInstancesCollectionDoc::CProviderCollectInstancesCollectionDoc() :
	_isInitialized(false) {}
CProviderCollectInstancesCollectionDoc::~CProviderCollectInstancesCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CProviderCollectInstancesCollectionDoc::initialize(
	const std::deque<SmartPtrCProviderCollectInstancesDoc> collectInstances) {
	if (! _isInitialized) {
		_collectInstances = collectInstances;

		_isInitialized = true;
	}
}

/// Accessor for the CollectInstances
std::deque<SmartPtrCProviderCollectInstancesDoc> CProviderCollectInstancesCollectionDoc::getCollectInstances() const {
	return _collectInstances;
}






