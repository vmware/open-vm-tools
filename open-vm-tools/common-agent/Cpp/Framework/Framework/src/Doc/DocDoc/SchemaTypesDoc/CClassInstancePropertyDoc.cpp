/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/SchemaTypesDoc/CClassIdentifierDoc.h"
#include "Doc/SchemaTypesDoc/CClassInstancePropertyDoc.h"

using namespace Caf;

/// Definition of an attribute (field) of a class
CClassInstancePropertyDoc::CClassInstancePropertyDoc() :
	_required(false),
	_transientVal(false),
	_list(false),
	_isInitialized(false) {}
CClassInstancePropertyDoc::~CClassInstancePropertyDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CClassInstancePropertyDoc::initialize(
	const std::string name,
	const std::deque<SmartPtrCClassIdentifierDoc> type,
	const bool required,
	const bool transientVal,
	const bool list,
	const std::string displayName,
	const std::string description) {
	if (! _isInitialized) {
		_name = name;
		_type = type;
		_required = required;
		_transientVal = transientVal;
		_list = list;
		_displayName = displayName;
		_description = description;

		_isInitialized = true;
	}
}

/// Property name
std::string CClassInstancePropertyDoc::getName() const {
	return _name;
}

/// Accessor for the Type
std::deque<SmartPtrCClassIdentifierDoc> CClassInstancePropertyDoc::getType() const {
	return _type;
}

/// Whether this is a required property, i.e. this property must always be non-empty
bool CClassInstancePropertyDoc::getRequired() const {
	return _required;
}

/// Accessor for the TransientVal
bool CClassInstancePropertyDoc::getTransientVal() const {
	return _transientVal;
}

/// Indicates whether to expect a list of properties in the provider response
bool CClassInstancePropertyDoc::getList() const {
	return _list;
}

/// A hint as to what this property should be called when displaying it to a human
std::string CClassInstancePropertyDoc::getDisplayName() const {
	return _displayName;
}

/// A phrase to describe the property for mouse-over text, etc
std::string CClassInstancePropertyDoc::getDescription() const {
	return _description;
}





