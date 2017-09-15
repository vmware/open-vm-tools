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
#include "Doc/SchemaTypesDoc/CJoinTypeDoc.h"
#include "Doc/SchemaTypesDoc/CLogicalRelationshipDoc.h"

using namespace Caf;

/// Definition of a relationship between classes that can be described by identifying the fields on the classes that uniquely identify the relationship
CLogicalRelationshipDoc::CLogicalRelationshipDoc() :
	_arity(ARITY_NONE),
	_isInitialized(false) {}
CLogicalRelationshipDoc::~CLogicalRelationshipDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CLogicalRelationshipDoc::initialize(
	const std::string namespaceVal,
	const std::string name,
	const std::string version,
	const ARITY_TYPE arity,
	const SmartPtrCClassCardinalityDoc dataClassLeft,
	const SmartPtrCClassCardinalityDoc dataClassRight,
	const std::deque<SmartPtrCJoinTypeDoc> joinCollection,
	const std::string description) {
	if (! _isInitialized) {
		_namespaceVal = namespaceVal;
		_name = name;
		_version = version;
		_arity = arity;
		_dataClassLeft = dataClassLeft;
		_dataClassRight = dataClassRight;
		_joinCollection = joinCollection;
		_description = description;

		_isInitialized = true;
	}
}

/// Accessor for the NamespaceVal
std::string CLogicalRelationshipDoc::getNamespaceVal() const {
	return _namespaceVal;
}

/// Accessor for the Name
std::string CLogicalRelationshipDoc::getName() const {
	return _name;
}

/// Accessor for the Version
std::string CLogicalRelationshipDoc::getVersion() const {
	return _version;
}

/// Accessor for the Arity
ARITY_TYPE CLogicalRelationshipDoc::getArity() const {
	return _arity;
}

/// Accessor for the DataClassLeft
SmartPtrCClassCardinalityDoc CLogicalRelationshipDoc::getDataClassLeft() const {
	return _dataClassLeft;
}

/// Accessor for the DataClassRight
SmartPtrCClassCardinalityDoc CLogicalRelationshipDoc::getDataClassRight() const {
	return _dataClassRight;
}

/// Defines a join condition of the relationship
std::deque<SmartPtrCJoinTypeDoc> CLogicalRelationshipDoc::getJoinCollection() const {
	return _joinCollection;
}

/// Accessor for the Description
std::string CLogicalRelationshipDoc::getDescription() const {
	return _description;
}





