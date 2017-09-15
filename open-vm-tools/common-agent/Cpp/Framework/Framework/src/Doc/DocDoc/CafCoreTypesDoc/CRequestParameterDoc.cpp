/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"
#include "Doc/CafCoreTypesDoc/CRequestParameterDoc.h"

using namespace Caf;

/// A simple container for objects of type RequestParameter
CRequestParameterDoc::CRequestParameterDoc() :
	_type(PARAMETER_NONE),
	_isInitialized(false) {}
CRequestParameterDoc::~CRequestParameterDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CRequestParameterDoc::initialize(
	const std::string name,
	const PARAMETER_TYPE type,
	const std::deque<std::string> value) {
	if (! _isInitialized) {
		_name = name;
		_type = type;
		_value = value;

		_isInitialized = true;
	}
}

/// Accessor for the Name
std::string CRequestParameterDoc::getName() const {
	return _name;
}

/// Accessor for the Type
PARAMETER_TYPE CRequestParameterDoc::getType() const {
	return _type;
}

/// Accessor for the Value
std::deque<std::string> CRequestParameterDoc::getValue() const {
	return _value;
}






