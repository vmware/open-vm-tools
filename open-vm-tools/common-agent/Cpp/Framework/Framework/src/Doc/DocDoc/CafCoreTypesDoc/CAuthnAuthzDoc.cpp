/*
 *  Author: bwilliams
 *  Created: May 24, 2015
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"
#include "Doc/CafCoreTypesDoc/CAuthnAuthzDoc.h"

using namespace Caf;

/// A simple container for objects of type AuthnAuthz
CAuthnAuthzDoc::CAuthnAuthzDoc() :
	_isInitialized(false),
	_sequenceNumber(0) {}
CAuthnAuthzDoc::~CAuthnAuthzDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CAuthnAuthzDoc::initialize(
	const std::string type,
	const std::string value,
	const std::string name,
	const int32 sequenceNumber) {
	if (! _isInitialized) {
		_type = type;
		_value = value;
		_name = name;
		_sequenceNumber = sequenceNumber;

		_isInitialized = true;
	}
}

/// Accessor for the Type
std::string CAuthnAuthzDoc::getType() const {
	return _type;
}

/// Accessor for the Value
std::string CAuthnAuthzDoc::getValue() const {
	return _value;
}

/// Accessor for the Name
std::string CAuthnAuthzDoc::getName() const {
	return _name;
}

/// Accessor for the SequenceNumber
int32 CAuthnAuthzDoc::getSequenceNumber() const {
	return _sequenceNumber;
}






