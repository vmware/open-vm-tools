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
#include "Doc/SchemaTypesDoc/CMethodParameterDoc.h"

using namespace Caf;

/// A parameter containing a simple type used by a method to control the outcome
CMethodParameterDoc::CMethodParameterDoc() :
	_type(PARAMETER_NONE),
	_isOptional(false),
	_isList(false),
	_isInitialized(false) {}
CMethodParameterDoc::~CMethodParameterDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CMethodParameterDoc::initialize(
	const std::string name,
	const PARAMETER_TYPE type,
	const bool isOptional,
	const bool isList,
	const std::string defaultVal,
	const std::string displayName,
	const std::string description) {
	if (! _isInitialized) {
		_name = name;
		_type = type;
		_isOptional = isOptional;
		_isList = isList;
		_defaultVal = defaultVal;
		_displayName = displayName;
		_description = description;

		_isInitialized = true;
	}
}

/// Name of parameter
std::string CMethodParameterDoc::getName() const {
	return _name;
}

/// Describes the data type of the property
PARAMETER_TYPE CMethodParameterDoc::getType() const {
	return _type;
}

/// Indicates this parameter need not be passed
bool CMethodParameterDoc::getIsOptional() const {
	return _isOptional;
}

/// Indicates whether to expect a list of values as opposed to a single value (the default if this attribute is not present)
bool CMethodParameterDoc::getIsList() const {
	return _isList;
}

/// Accessor for the DefaultVal
std::string CMethodParameterDoc::getDefaultVal() const {
	return _defaultVal;
}

/// Human-readable version of the parameter name
std::string CMethodParameterDoc::getDisplayName() const {
	return _displayName;
}

/// Short description of what the parameter is for
std::string CMethodParameterDoc::getDescription() const {
	return _description;
}





