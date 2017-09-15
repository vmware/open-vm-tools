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

#include "Doc/SchemaTypesDoc/CClassFieldDoc.h"
#include "Doc/SchemaTypesDoc/CJoinTypeDoc.h"

using namespace Caf;

/// A simple container for objects of type JoinType
CJoinTypeDoc::CJoinTypeDoc() :
	_operand(OPERATOR_NONE),
	_isInitialized(false) {}
CJoinTypeDoc::~CJoinTypeDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CJoinTypeDoc::initialize(
	const OPERATOR_TYPE operand,
	const SmartPtrCClassFieldDoc dataClassLeft,
	const SmartPtrCClassFieldDoc dataClassRight,
	const std::string description) {
	if (! _isInitialized) {
		_operand = operand;
		_dataClassLeft = dataClassLeft;
		_dataClassRight = dataClassRight;
		_description = description;

		_isInitialized = true;
	}
}

/// Defines the operand used to join the class fields. Restricted to '=' for now
OPERATOR_TYPE CJoinTypeDoc::getOperand() const {
	return _operand;
}

/// Identifies the fields on classes that uniquely identify relationship
SmartPtrCClassFieldDoc CJoinTypeDoc::getDataClassLeft() const {
	return _dataClassLeft;
}

/// Identifies the fields on classes that uniquely identify relationship
SmartPtrCClassFieldDoc CJoinTypeDoc::getDataClassRight() const {
	return _dataClassRight;
}

/// A short human-readable description of the join
std::string CJoinTypeDoc::getDescription() const {
	return _description;
}

OPERATOR_TYPE _operand;




