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

#include "Doc/SchemaTypesDoc/CClassCardinalityDoc.h"
#include "Doc/SchemaTypesDoc/CRelationshipDoc.h"

using namespace Caf;

/// Definition of a relationship between data classes
CRelationshipDoc::CRelationshipDoc() :
	_arity(ARITY_NONE),
	_isInitialized(false) {}
CRelationshipDoc::~CRelationshipDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CRelationshipDoc::initialize(
	const std::string namespaceVal,
	const std::string name,
	const std::string version,
	const ARITY_TYPE arity,
	const SmartPtrCClassCardinalityDoc dataClassLeft,
	const SmartPtrCClassCardinalityDoc dataClassRight,
	const std::string description) {
	if (! _isInitialized) {
		_namespaceVal = namespaceVal;
		_name = name;
		_version = version;
		_arity = arity;
		_dataClassLeft = dataClassLeft;
		_dataClassRight = dataClassRight;
		_description = description;

		_isInitialized = true;
	}
}

/// Accessor for the NamespaceVal
std::string CRelationshipDoc::getNamespaceVal() const {
	return _namespaceVal;
}

/// Accessor for the Name
std::string CRelationshipDoc::getName() const {
	return _name;
}

/// Accessor for the Version
std::string CRelationshipDoc::getVersion() const {
	return _version;
}

/// Number of parts (sides) to relationship. Restricted to a two-sided relationship for now
ARITY_TYPE CRelationshipDoc::getArity() const {
	return _arity;
}

/// Identifies the two classes that make up the relationship
SmartPtrCClassCardinalityDoc CRelationshipDoc::getDataClassLeft() const {
	return _dataClassLeft;
}

/// Identifies the two classes that make up the relationship
SmartPtrCClassCardinalityDoc CRelationshipDoc::getDataClassRight() const {
	return _dataClassRight;
}

/// A short human-readable description of the relationship
std::string CRelationshipDoc::getDescription() const {
	return _description;
}






