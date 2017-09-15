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

using namespace Caf;

/// A simple container for objects of type DataClassSubInstance
CDataClassSubInstanceDoc::CDataClassSubInstanceDoc() :
	_isInitialized(false) {}
CDataClassSubInstanceDoc::~CDataClassSubInstanceDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CDataClassSubInstanceDoc::initialize(
	const std::string name,
	const std::deque<SmartPtrCCmdlMetadataDoc> cmdlMetadataCollection,
	const std::deque<SmartPtrCDataClassPropertyDoc> propertyCollection,
	const std::deque<SmartPtrCDataClassSubInstanceDoc> instancePropertyCollection,
	const SmartPtrCCmdlUnionDoc cmdlUnion) {
	if (! _isInitialized) {
		_name = name;
		_cmdlMetadataCollection = cmdlMetadataCollection;
		_propertyCollection = propertyCollection;
		_instancePropertyCollection = instancePropertyCollection;
		_cmdlUnion = cmdlUnion;

		_isInitialized = true;
	}
}

/// Accessor for the Name
std::string CDataClassSubInstanceDoc::getName() const {
	return _name;
}

/// Accessor for the CmdlMetadata
std::deque<SmartPtrCCmdlMetadataDoc> CDataClassSubInstanceDoc::getCmdlMetadataCollection() const {
	return _cmdlMetadataCollection;
}

/// Accessor for the Property
std::deque<SmartPtrCDataClassPropertyDoc> CDataClassSubInstanceDoc::getPropertyCollection() const {
	return _propertyCollection;
}

/// Accessor for the InstanceProperty
std::deque<SmartPtrCDataClassSubInstanceDoc> CDataClassSubInstanceDoc::getInstancePropertyCollection() const {
	return _instancePropertyCollection;
}

/// Accessor for the CmdlUnion
SmartPtrCCmdlUnionDoc CDataClassSubInstanceDoc::getCmdlUnion() const {
	return _cmdlUnion;
}






