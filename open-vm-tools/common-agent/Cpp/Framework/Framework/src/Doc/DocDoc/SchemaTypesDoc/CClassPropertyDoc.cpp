/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/SchemaTypesDoc/SchemaTypesDocTypes.h"
#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"
#include "Doc/SchemaTypesDoc/CClassPropertyDoc.h"

using namespace Caf;

/// Definition of an attribute (field) of a class
CClassPropertyDoc::CClassPropertyDoc() :
	_type(PROPERTY_NONE),
	_required(false),
	_key(false),
	_list(false),
	_caseSensitive(false),
	_transientVal(false),
	_validator(VALIDATOR_NONE),
	_isInitialized(false) {}
CClassPropertyDoc::~CClassPropertyDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CClassPropertyDoc::initialize(
	const std::string name,
	const PROPERTY_TYPE type,
	const std::deque<std::string> value,
	const bool required,
	const bool key,
	const bool list,
	const bool caseSensitive,
	const bool transientVal,
	const std::string defaultVal,
	const VALIDATOR_TYPE validator,
	const std::string upperRange,
	const std::string lowerRange,
	const std::string displayName,
	const std::string description) {
	if (! _isInitialized) {
		_name = name;
		_type = type;
		_value = value;
		_required = required;
		_key = key;
		_list = list;
		_caseSensitive = caseSensitive;
		_transientVal = transientVal;
		_defaultVal = defaultVal;
		_validator = validator;
		_upperRange = upperRange;
		_lowerRange = lowerRange;
		_displayName = displayName;
		_description = description;

		_isInitialized = true;
	}
}

/// Property name
std::string CClassPropertyDoc::getName() const {
	return _name;
}

/// Describes the data type of the property
PROPERTY_TYPE CClassPropertyDoc::getType() const {
	return _type;
}

/// The contents of a validator used on this property
std::deque<std::string> CClassPropertyDoc::getValue() const {
	return _value;
}

/// Whether this is a required property, i.e. this property must always be non-empty
bool CClassPropertyDoc::getRequired() const {
	return _required;
}

/// Indicates this property may be used as a key identifying field
bool CClassPropertyDoc::getKey() const {
	return _key;
}

/// Indicates whether to expect a list of properties in the provider response
bool CClassPropertyDoc::getList() const {
	return _list;
}

/// Indicates whether a string field should be treated in a case-sensitive manner
bool CClassPropertyDoc::getCaseSensitive() const {
	return _caseSensitive;
}

/// Accessor for the TransientVal
bool CClassPropertyDoc::getTransientVal() const {
	return _transientVal;
}

/// Accessor for the DefaultVal
std::string CClassPropertyDoc::getDefaultVal() const {
	return _defaultVal;
}

/// The type of validator described in the 'value' sub-elements
VALIDATOR_TYPE CClassPropertyDoc::getValidator() const {
	return _validator;
}

/// If a 'range' validator is in use, this describes the upper limit of allowable values for the property. QUESTIONABLE: how do we determine inclusive or exclusive range
std::string CClassPropertyDoc::getUpperRange() const {
	return _upperRange;
}

/// If a 'range' validator is in use, this describes the lower limit of allowable values for the property. QUESTIONABLE: how do we determine inclusive or exclusive range
std::string CClassPropertyDoc::getLowerRange() const {
	return _lowerRange;
}

/// A hint as to what this property should be called when displaying it to a human
std::string CClassPropertyDoc::getDisplayName() const {
	return _displayName;
}

/// A phrase to describe the property for mouse-over text, etc
std::string CClassPropertyDoc::getDescription() const {
	return _description;
}

VALIDATOR_TYPE _validator;




