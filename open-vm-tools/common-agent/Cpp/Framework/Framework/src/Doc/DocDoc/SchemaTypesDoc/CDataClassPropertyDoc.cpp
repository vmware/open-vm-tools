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
#include "Doc/SchemaTypesDoc/CDataClassPropertyDoc.h"

using namespace Caf;

/// A simple container for objects of type DataClassProperty
CDataClassPropertyDoc::CDataClassPropertyDoc() :
	_isInitialized(false) {}
CDataClassPropertyDoc::~CDataClassPropertyDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CDataClassPropertyDoc::initialize(
	const std::string name,
	const std::deque<SmartPtrCCmdlMetadataDoc> cmdlMetadata,
	const std::string value) {
	if (! _isInitialized) {
		_name = name;
		_cmdlMetadata = cmdlMetadata;
		_value = value;

		_isInitialized = true;
	}
}

/// Accessor for the Name
std::string CDataClassPropertyDoc::getName() const {
	return _name;
}

/// Accessor for the CmdlMetadata
std::deque<SmartPtrCCmdlMetadataDoc> CDataClassPropertyDoc::getCmdlMetadata() const {
	return _cmdlMetadata;
}

/// Accessor for the Value
std::string CDataClassPropertyDoc::getValue() const {
	return _value;
}






