/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/ResponseDoc/CEventKeyDoc.h"

using namespace Caf;

/// A simple container for objects of type EventKey
CEventKeyDoc::CEventKeyDoc() :
	_isInitialized(false) {}
CEventKeyDoc::~CEventKeyDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CEventKeyDoc::initialize(
	const std::string name,
	const std::string value) {
	if (! _isInitialized) {
		_name = name;
		_value = value;

		_isInitialized = true;
	}
}

/// Accessor for the Name
std::string CEventKeyDoc::getName() const {
	return _name;
}

/// Accessor for the Value
std::string CEventKeyDoc::getValue() const {
	return _value;
}






