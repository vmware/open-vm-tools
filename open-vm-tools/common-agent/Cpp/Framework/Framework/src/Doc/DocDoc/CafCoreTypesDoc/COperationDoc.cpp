/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CParameterCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/COperationDoc.h"

using namespace Caf;

/// A simple container for objects of type Operation
COperationDoc::COperationDoc() :
	_isInitialized(false) {}
COperationDoc::~COperationDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void COperationDoc::initialize(
	const std::string name,
	const SmartPtrCParameterCollectionDoc parameterCollection) {
	if (! _isInitialized) {
		_name = name;
		_parameterCollection = parameterCollection;

		_isInitialized = true;
	}
}

/// Accessor for the Name
std::string COperationDoc::getName() const {
	return _name;
}

/// Accessor for the ParameterCollection
SmartPtrCParameterCollectionDoc COperationDoc::getParameterCollection() const {
	return _parameterCollection;
}






