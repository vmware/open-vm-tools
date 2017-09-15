/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/SchemaTypesDoc/CClassFieldDoc.h"

using namespace Caf;

/// Description of a class and the field used to identify one end of a relationship
CClassFieldDoc::CClassFieldDoc() :
	_isInitialized(false) {}
CClassFieldDoc::~CClassFieldDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CClassFieldDoc::initialize(
	const std::string namespaceVal,
	const std::string name,
	const std::string version,
	const std::string field) {
	if (! _isInitialized) {
		_namespaceVal = namespaceVal;
		_name = name;
		_version = version;
		_field = field;

		_isInitialized = true;
	}
}

/// Accessor for the NamespaceVal
std::string CClassFieldDoc::getNamespaceVal() const {
	return _namespaceVal;
}

/// Accessor for the Name
std::string CClassFieldDoc::getName() const {
	return _name;
}

/// Accessor for the Version
std::string CClassFieldDoc::getVersion() const {
	return _version;
}

/// Description of a class field used to identify one end of a relationship
std::string CClassFieldDoc::getField() const {
	return _field;
}






