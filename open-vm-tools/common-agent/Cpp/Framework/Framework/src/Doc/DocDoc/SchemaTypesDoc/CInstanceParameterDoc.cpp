/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/SchemaTypesDoc/CInstanceParameterDoc.h"

using namespace Caf;

/// A parameter containing a data class instance used by a method to control the outcome
CInstanceParameterDoc::CInstanceParameterDoc() :
	_isOptional(false),
	_isList(false),
	_isInitialized(false) {}
CInstanceParameterDoc::~CInstanceParameterDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CInstanceParameterDoc::initialize(
	const std::string name,
	const std::string instanceNamespace,
	const std::string instanceName,
	const std::string instanceVersion,
	const bool isOptional,
	const bool isList,
	const std::string displayName,
	const std::string description) {
	if (! _isInitialized) {
		_name = name;
		_instanceNamespace = instanceNamespace;
		_instanceName = instanceName;
		_instanceVersion = instanceVersion;
		_isOptional = isOptional;
		_isList = isList;
		_displayName = displayName;
		_description = description;

		_isInitialized = true;
	}
}

/// Name of parameter
std::string CInstanceParameterDoc::getName() const {
	return _name;
}

/// Namespace of instance object type
std::string CInstanceParameterDoc::getInstanceNamespace() const {
	return _instanceNamespace;
}

/// Name of instance object type
std::string CInstanceParameterDoc::getInstanceName() const {
	return _instanceName;
}

/// Version of instance object type
std::string CInstanceParameterDoc::getInstanceVersion() const {
	return _instanceVersion;
}

/// Indicates this parameter need not be passed
bool CInstanceParameterDoc::getIsOptional() const {
	return _isOptional;
}

/// Indicates whether to expect a list of values as opposed to a single value (the default if this attribute is not present)
bool CInstanceParameterDoc::getIsList() const {
	return _isList;
}

/// Human-readable version of the parameter name
std::string CInstanceParameterDoc::getDisplayName() const {
	return _displayName;
}

/// Short description of what the parameter is for
std::string CInstanceParameterDoc::getDescription() const {
	return _description;
}





