/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/SchemaTypesDoc/CClassCardinalityDoc.h"

using namespace Caf;

/// Class description of one end of a relationship
CClassCardinalityDoc::CClassCardinalityDoc() :
	_isInitialized(false) {}
CClassCardinalityDoc::~CClassCardinalityDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CClassCardinalityDoc::initialize(
	const std::string namespaceVal,
	const std::string name,
	const std::string version,
	const std::string cardinality) {
	if (! _isInitialized) {
		_namespaceVal = namespaceVal;
		_name = name;
		_version = version;
		_cardinality = cardinality;

		_isInitialized = true;
	}
}

/// Accessor for the NamespaceVal
std::string CClassCardinalityDoc::getNamespaceVal() const {
	return _namespaceVal;
}

/// Accessor for the Name
std::string CClassCardinalityDoc::getName() const {
	return _name;
}

/// Accessor for the Version
std::string CClassCardinalityDoc::getVersion() const {
	return _version;
}

/// Cardinality of one end relationship, i.e. has one, has many, etc
std::string CClassCardinalityDoc::getCardinality() const {
	return _cardinality;
}






