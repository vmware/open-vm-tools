/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/SchemaTypesDoc/CInstanceOperationDoc.h"

using namespace Caf;

/// A simple container for objects of type InstanceOperation
CInstanceOperationDoc::CInstanceOperationDoc() :
	_isInitialized(false) {}
CInstanceOperationDoc::~CInstanceOperationDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CInstanceOperationDoc::initialize(
	const std::string operationName,
	const std::string moniker) {
	if (! _isInitialized) {
		_operationName = operationName;
		_moniker = moniker;

		_isInitialized = true;
	}
}

/// Accessor for the OperationName
std::string CInstanceOperationDoc::getOperationName() const {
	return _operationName;
}

/// Accessor for the Moniker
std::string CInstanceOperationDoc::getMoniker() const {
	return _moniker;
}






