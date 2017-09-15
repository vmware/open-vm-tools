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

using namespace Caf;

/// Tuple of values to uniquely identify a class
CClassIdentifierDoc::CClassIdentifierDoc() :
	_isInitialized(false) {}
CClassIdentifierDoc::~CClassIdentifierDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CClassIdentifierDoc::initialize(
	const std::string namespaceVal,
	const std::string name,
	const std::string version) {
	if (! _isInitialized) {
		_namespaceVal = namespaceVal;
		_name = name;
		_version = version;

		_isInitialized = true;
	}
}

/// Accessor for the NamespaceVal
std::string CClassIdentifierDoc::getNamespaceVal() const {
	return _namespaceVal;
}

/// Accessor for the Name
std::string CClassIdentifierDoc::getName() const {
	return _name;
}

/// Accessor for the Version
std::string CClassIdentifierDoc::getVersion() const {
	return _version;
}






