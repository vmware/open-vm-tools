/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/CafCoreTypesDoc/CProtocolDoc.h"

using namespace Caf;

/// A simple container for objects of Protocol
CProtocolDoc::CProtocolDoc() :
	_sequenceNumber(0),
	_isInitialized(false) {}
CProtocolDoc::~CProtocolDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CProtocolDoc::initialize(
	const std::string uri,
	const std::string name,
	const int32 sequenceNumber) {
	if (! _isInitialized) {
		_name = name;
		_uri = uri;
		_sequenceNumber = sequenceNumber;

		_isInitialized = true;
	}
}

/// Accessor for the Uri
std::string CProtocolDoc::getUri() const {
	return _uri;
}

/// Accessor for the Name
std::string CProtocolDoc::getName() const {
	return _name;
}

/// Accessor for the Value
int32 CProtocolDoc::getSequenceNumber() const {
	return _sequenceNumber;
}






