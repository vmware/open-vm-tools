/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/SchemaTypesDoc/CClassIdentifierDoc.h"
#include "Doc/SchemaTypesDoc/CInstanceParameterDoc.h"
#include "Doc/SchemaTypesDoc/CMethodParameterDoc.h"
#include "Doc/SchemaTypesDoc/CCollectMethodDoc.h"

using namespace Caf;

/// Definition of a collection method on a class
CCollectMethodDoc::CCollectMethodDoc() :
	_isInitialized(false) {}
CCollectMethodDoc::~CCollectMethodDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CCollectMethodDoc::initialize(
	const std::string name,
	const std::deque<SmartPtrCMethodParameterDoc> parameterCollection,
	const std::deque<SmartPtrCInstanceParameterDoc> instanceParameterCollection,
	const std::deque<SmartPtrCClassIdentifierDoc> returnValCollection,
	const std::deque<SmartPtrCClassIdentifierDoc> eventValCollection,
	const std::deque<SmartPtrCClassIdentifierDoc> errorCollection) {
	if (! _isInitialized) {
		_name = name;
		_parameterCollection = parameterCollection;
		_instanceParameterCollection = instanceParameterCollection;
		_returnValCollection = returnValCollection;
		_eventValCollection = eventValCollection;
		_errorCollection = errorCollection;

		_isInitialized = true;
	}
}

/// name of the collection method
std::string CCollectMethodDoc::getName() const {
	return _name;
}

/// Definition of a parameter that passes simple types to the collection method
std::deque<SmartPtrCMethodParameterDoc> CCollectMethodDoc::getParameterCollection() const {
	return _parameterCollection;
}

/// Definition of a parameter passing data class instances to the collection method
std::deque<SmartPtrCInstanceParameterDoc> CCollectMethodDoc::getInstanceParameterCollection() const {
	return _instanceParameterCollection;
}

/// Accessor for the ReturnVal
std::deque<SmartPtrCClassIdentifierDoc> CCollectMethodDoc::getReturnValCollection() const {
	return _returnValCollection;
}

/// Accessor for the EventVal
std::deque<SmartPtrCClassIdentifierDoc> CCollectMethodDoc::getEventValCollection() const {
	return _eventValCollection;
}

/// A class that may be returned to indicate an error occurred during the processing of the collection method
std::deque<SmartPtrCClassIdentifierDoc> CCollectMethodDoc::getErrorCollection() const {
	return _errorCollection;
}






