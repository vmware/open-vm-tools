/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/SchemaTypesDoc/CInstanceOperationCollectionDoc.h"
#include "Doc/SchemaTypesDoc/CActionClassInstanceDoc.h"

using namespace Caf;

/// A simple container for objects of type ActionClassInstance
CActionClassInstanceDoc::CActionClassInstanceDoc() :
	_isInitialized(false) {}
CActionClassInstanceDoc::~CActionClassInstanceDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CActionClassInstanceDoc::initialize(
	const std::string namespaceVal,
	const std::string name,
	const std::string version,
	const SmartPtrCInstanceOperationCollectionDoc instanceOperationCollection) {
	if (! _isInitialized) {
		_namespaceVal = namespaceVal;
		_name = name;
		_version = version;
		_instanceOperationCollection = instanceOperationCollection;

		_isInitialized = true;
	}
}

/// Accessor for the NamespaceVal
std::string CActionClassInstanceDoc::getNamespaceVal() const {
	return _namespaceVal;
}

/// Accessor for the Name
std::string CActionClassInstanceDoc::getName() const {
	return _name;
}

/// Accessor for the Version
std::string CActionClassInstanceDoc::getVersion() const {
	return _version;
}

/// Accessor for the InstanceOperationCollection
SmartPtrCInstanceOperationCollectionDoc CActionClassInstanceDoc::getInstanceOperationCollection() const {
	return _instanceOperationCollection;
}






