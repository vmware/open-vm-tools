/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/SchemaTypesDoc/CCmdlMetadataDoc.h"
#include "Doc/SchemaTypesDoc/CCmdlUnionDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassPropertyDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassSubInstanceDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassInstanceDoc.h"

using namespace Caf;

/// A simple container for objects of type DataClassInstance
CDataClassInstanceDoc::CDataClassInstanceDoc() :
	_isInitialized(false) {}
CDataClassInstanceDoc::~CDataClassInstanceDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CDataClassInstanceDoc::initialize(
	const std::string namespaceVal,
	const std::string name,
	const std::string version,
	const std::deque<SmartPtrCCmdlMetadataDoc> cmdlMetadataCollection,
	const std::deque<SmartPtrCDataClassPropertyDoc> propertyCollection,
	const std::deque<SmartPtrCDataClassSubInstanceDoc> instancePropertyCollection,
	const SmartPtrCCmdlUnionDoc cmdlUnion) {
	if (! _isInitialized) {
		_namespaceVal = namespaceVal;
		_name = name;
		_version = version;
		_cmdlMetadataCollection = cmdlMetadataCollection;
		_propertyCollection = propertyCollection;
		_instancePropertyCollection = instancePropertyCollection;
		_cmdlUnion = cmdlUnion;

		_isInitialized = true;
	}
}

/// Accessor for the NamespaceVal
std::string CDataClassInstanceDoc::getNamespaceVal() const {
	return _namespaceVal;
}

/// Accessor for the Name
std::string CDataClassInstanceDoc::getName() const {
	return _name;
}

/// Accessor for the Version
std::string CDataClassInstanceDoc::getVersion() const {
	return _version;
}

/// Accessor for the CmdlMetadata
std::deque<SmartPtrCCmdlMetadataDoc> CDataClassInstanceDoc::getCmdlMetadataCollection() const {
	return _cmdlMetadataCollection;
}

/// Accessor for the Property
std::deque<SmartPtrCDataClassPropertyDoc> CDataClassInstanceDoc::getPropertyCollection() const {
	return _propertyCollection;
}

/// Accessor for the InstanceProperty
std::deque<SmartPtrCDataClassSubInstanceDoc> CDataClassInstanceDoc::getInstancePropertyCollection() const {
	return _instancePropertyCollection;
}

/// Accessor for the CmdlUnion
SmartPtrCCmdlUnionDoc CDataClassInstanceDoc::getCmdlUnion() const {
	return _cmdlUnion;
}






