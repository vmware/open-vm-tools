/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CClassFiltersDoc.h"
#include "Doc/CafCoreTypesDoc/CFullyQualifiedClassGroupDoc.h"
#include "Doc/CafCoreTypesDoc/CClassSpecifierDoc.h"

using namespace Caf;

/// A simple container for objects of type ClassSpecifier
CClassSpecifierDoc::CClassSpecifierDoc() :
	_isInitialized(false) {}
CClassSpecifierDoc::~CClassSpecifierDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CClassSpecifierDoc::initialize(
	const SmartPtrCFullyQualifiedClassGroupDoc fullyQualifiedClass,
	const SmartPtrCClassFiltersDoc classFilters) {
	if (! _isInitialized) {
		_fullyQualifiedClass = fullyQualifiedClass;
		_classFilters = classFilters;

		_isInitialized = true;
	}
}

/// Accessor for the FullyQualifiedClass
SmartPtrCFullyQualifiedClassGroupDoc CClassSpecifierDoc::getFullyQualifiedClass() const {
	return _fullyQualifiedClass;
}

/// Accessor for the ClassFilters
SmartPtrCClassFiltersDoc CClassSpecifierDoc::getClassFilters() const {
	return _classFilters;
}






