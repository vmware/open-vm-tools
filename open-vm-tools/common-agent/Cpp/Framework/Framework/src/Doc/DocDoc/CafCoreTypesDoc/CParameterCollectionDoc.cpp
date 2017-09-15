/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CRequestInstanceParameterDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestParameterDoc.h"
#include "Doc/CafCoreTypesDoc/CParameterCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type ParameterCollection
CParameterCollectionDoc::CParameterCollectionDoc() :
	_isInitialized(false) {}
CParameterCollectionDoc::~CParameterCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CParameterCollectionDoc::initialize(
	const std::deque<SmartPtrCRequestParameterDoc> parameter,
	const std::deque<SmartPtrCRequestInstanceParameterDoc> instanceParameter) {
	if (! _isInitialized) {
		_parameter = parameter;
		_instanceParameter = instanceParameter;

		_isInitialized = true;
	}
}

/// Accessor for the Parameter
std::deque<SmartPtrCRequestParameterDoc> CParameterCollectionDoc::getParameter() const {
	return _parameter;
}

/// Accessor for the InstanceParameter
std::deque<SmartPtrCRequestInstanceParameterDoc> CParameterCollectionDoc::getInstanceParameter() const {
	return _instanceParameter;
}






