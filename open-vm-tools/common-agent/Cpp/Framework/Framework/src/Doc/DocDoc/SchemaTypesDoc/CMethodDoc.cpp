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
#include "Doc/SchemaTypesDoc/CMethodDoc.h"

using namespace Caf;

/// Definition of a method on a class
CMethodDoc::CMethodDoc() :
	_isInitialized(false) {}
CMethodDoc::~CMethodDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CMethodDoc::initialize(
	const std::string name,
	const std::deque<SmartPtrCMethodParameterDoc> parameterCollection,
	const std::deque<SmartPtrCInstanceParameterDoc> instanceParameterCollection,
	const std::deque<SmartPtrCClassIdentifierDoc> returnValCollection,
	const std::deque<SmartPtrCClassIdentifierDoc> eventValCollection,
	const std::deque<SmartPtrCClassIdentifierDoc> errorCollection,
	const std::string displayName,
	const std::string description) {
	if (! _isInitialized) {
		_name = name;
		_parameterCollection = parameterCollection;
		_instanceParameterCollection = instanceParameterCollection;
		_returnValCollection = returnValCollection;
		_eventValCollection = eventValCollection;
		_errorCollection = errorCollection;
		_displayName = displayName;
		_description = description;

		_isInitialized = true;
	}
}

/// name of the method
std::string CMethodDoc::getName() const {
	return _name;
}

/// Definition of a parameter that passes simple types to the method
std::deque<SmartPtrCMethodParameterDoc> CMethodDoc::getParameterCollection() const {
	return _parameterCollection;
}

/// Definition of a parameter that passes data class instances to the method
std::deque<SmartPtrCInstanceParameterDoc> CMethodDoc::getInstanceParameterCollection() const {
	return _instanceParameterCollection;
}

/// Accessor for the ReturnVal
std::deque<SmartPtrCClassIdentifierDoc> CMethodDoc::getReturnValCollection() const {
	return _returnValCollection;
}

/// Accessor for the EventVal
std::deque<SmartPtrCClassIdentifierDoc> CMethodDoc::getEventValCollection() const {
	return _eventValCollection;
}

/// A class that may be returned to indicate an error occurred during the processing of the collection method
std::deque<SmartPtrCClassIdentifierDoc> CMethodDoc::getErrorCollection() const {
	return _errorCollection;
}

/// Human-readable version of the method name
std::string CMethodDoc::getDisplayName() const {
	return _displayName;
}

/// A short phrase describing the purpose of the method
std::string CMethodDoc::getDescription() const {
	return _description;
}






