/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CFullyQualifiedClassGroupDoc.h"
#include "Doc/ProviderInfraDoc/CClassCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type ClassCollection
CClassCollectionDoc::CClassCollectionDoc() :
	_isInitialized(false) {}
CClassCollectionDoc::~CClassCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CClassCollectionDoc::initialize(
	const std::deque<SmartPtrCFullyQualifiedClassGroupDoc> fullyQualifiedClass) {
	if (! _isInitialized) {
		_fullyQualifiedClass = fullyQualifiedClass;

		_isInitialized = true;
	}
}

/// Accessor for the FullyQualifiedClass
std::deque<SmartPtrCFullyQualifiedClassGroupDoc> CClassCollectionDoc::getFullyQualifiedClass() const {
	return _fullyQualifiedClass;
}






