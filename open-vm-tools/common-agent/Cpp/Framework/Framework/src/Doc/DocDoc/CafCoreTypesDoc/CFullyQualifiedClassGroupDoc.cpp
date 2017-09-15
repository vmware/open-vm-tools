/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/CafCoreTypesDoc/CFullyQualifiedClassGroupDoc.h"

using namespace Caf;

/// A simple container for objects of type FullyQualifiedClassGroup
CFullyQualifiedClassGroupDoc::CFullyQualifiedClassGroupDoc() :
	_isInitialized(false) {}
CFullyQualifiedClassGroupDoc::~CFullyQualifiedClassGroupDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CFullyQualifiedClassGroupDoc::initialize(
	const std::string classNamespace,
	const std::string className,
	const std::string classVersion) {
	if (! _isInitialized) {
		_classNamespace = classNamespace;
		_className = className;
		_classVersion = classVersion;

		_isInitialized = true;
	}
}

/// Accessor for the ClassNamespace
std::string CFullyQualifiedClassGroupDoc::getClassNamespace() const {
	return _classNamespace;
}

/// Accessor for the ClassName
std::string CFullyQualifiedClassGroupDoc::getClassName() const {
	return _className;
}

/// Accessor for the ClassVersion
std::string CFullyQualifiedClassGroupDoc::getClassVersion() const {
	return _classVersion;
}






