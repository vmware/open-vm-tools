/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/ProviderResultsDoc/CDefinitionObjectCollectionDoc.h"

using namespace Caf;

/// Set of elements containing data returned as a result of a provider collection or action
CDefinitionObjectCollectionDoc::CDefinitionObjectCollectionDoc() :
	_isInitialized(false) {}
CDefinitionObjectCollectionDoc::~CDefinitionObjectCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CDefinitionObjectCollectionDoc::initialize(
	const std::deque<std::string> value) {
	if (! _isInitialized) {
		_value = value;

		_isInitialized = true;
	}
}

/// Accessor for the Value
std::deque<std::string> CDefinitionObjectCollectionDoc::getValue() const {
	return _value;
}






