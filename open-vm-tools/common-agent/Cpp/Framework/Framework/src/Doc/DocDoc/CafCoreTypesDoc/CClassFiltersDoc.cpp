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

using namespace Caf;

/// A simple container for objects of type ClassFilters
CClassFiltersDoc::CClassFiltersDoc() :
	_isInitialized(false) {}
CClassFiltersDoc::~CClassFiltersDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CClassFiltersDoc::initialize(
	const std::string dialect,
	const std::deque<std::string> classFilter) {
	if (! _isInitialized) {
		_dialect = dialect;
		_classFilter = classFilter;

		_isInitialized = true;
	}
}

/// Accessor for the Dialect
std::string CClassFiltersDoc::getDialect() const {
	return _dialect;
}

/// Accessor for the ClassFilter
std::deque<std::string> CClassFiltersDoc::getClassFilter() const {
	return _classFilter;
}






