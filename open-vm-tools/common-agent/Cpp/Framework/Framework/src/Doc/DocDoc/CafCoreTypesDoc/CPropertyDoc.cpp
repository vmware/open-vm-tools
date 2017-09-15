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
#include "Doc/CafCoreTypesDoc/CPropertyDoc.h"

using namespace Caf;

/// A simple container for objects of type Property
CPropertyDoc::CPropertyDoc() :
	_type(PROPERTY_NONE),
	_isInitialized(false) {}
CPropertyDoc::~CPropertyDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CPropertyDoc::initialize(
	const std::string name,
	const PROPERTY_TYPE type,
	const std::deque<std::string> value) {
	if (! _isInitialized) {
		_name = name;
		_type = type;
		_value = value;

		_isInitialized = true;
	}
}

/// Accessor for the Name
std::string CPropertyDoc::getName() const {
	return _name;
}

/// Accessor for the Type
PROPERTY_TYPE CPropertyDoc::getType() const {
	return _type;
}

/// Accessor for the Value
std::deque<std::string> CPropertyDoc::getValue() const {
	return _value;
}






