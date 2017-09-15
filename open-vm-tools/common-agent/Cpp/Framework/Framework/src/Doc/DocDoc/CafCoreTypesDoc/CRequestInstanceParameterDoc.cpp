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

using namespace Caf;

/// A simple container for objects of type RequestInstanceParameter
CRequestInstanceParameterDoc::CRequestInstanceParameterDoc() :
	_isInitialized(false) {}
CRequestInstanceParameterDoc::~CRequestInstanceParameterDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CRequestInstanceParameterDoc::initialize(
	const std::string name,
	const std::string classNamespace,
	const std::string className,
	const std::string classVersion,
	const std::deque<std::string> value) {
	if (! _isInitialized) {
		_name = name;
		_classNamespace = classNamespace;
		_className = className;
		_classVersion = classVersion;
		_value = value;

		_isInitialized = true;
	}
}

/// Accessor for the Name
std::string CRequestInstanceParameterDoc::getName() const {
	return _name;
}

/// Accessor for the ClassNamespace
std::string CRequestInstanceParameterDoc::getClassNamespace() const {
	return _classNamespace;
}

/// Accessor for the ClassName
std::string CRequestInstanceParameterDoc::getClassName() const {
	return _className;
}

/// Accessor for the ClassVersion
std::string CRequestInstanceParameterDoc::getClassVersion() const {
	return _classVersion;
}

/// Accessor for the Value
std::deque<std::string> CRequestInstanceParameterDoc::getValue() const {
	return _value;
}






