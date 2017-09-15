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
#include "Doc/SchemaTypesDoc/CPhysicalRelationshipDoc.h"

using namespace Caf;

/// Describes a relationship between dataclass where the key information from data class instances comprising the relationship are listed in an instance of this class
CPhysicalRelationshipDoc::CPhysicalRelationshipDoc() :
	_arity(ARITY_NONE),
	_isInitialized(false) {}
CPhysicalRelationshipDoc::~CPhysicalRelationshipDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CPhysicalRelationshipDoc::initialize(
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
std::string CPhysicalRelationshipDoc::getNamespaceVal() const {
	return _namespaceVal;
}

/// Accessor for the Name
std::string CPhysicalRelationshipDoc::getName() const {
	return _name;
}

/// Accessor for the Version
std::string CPhysicalRelationshipDoc::getVersion() const {
	return _version;
}

/// Accessor for the Arity
ARITY_TYPE CPhysicalRelationshipDoc::getArity() const {
	return _arity;
}

/// Accessor for the DataClassLeft
SmartPtrCClassCardinalityDoc CPhysicalRelationshipDoc::getDataClassLeft() const {
	return _dataClassLeft;
}

/// Accessor for the DataClassRight
SmartPtrCClassCardinalityDoc CPhysicalRelationshipDoc::getDataClassRight() const {
	return _dataClassRight;
}

/// Accessor for the Description
std::string CPhysicalRelationshipDoc::getDescription() const {
	return _description;
}





