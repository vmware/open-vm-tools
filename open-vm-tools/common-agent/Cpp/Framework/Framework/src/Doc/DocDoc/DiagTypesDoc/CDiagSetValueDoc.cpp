/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CPropertyDoc.h"
#include "Doc/DiagTypesDoc/CDiagSetValueDoc.h"

using namespace Caf;

/// A simple container for objects of type DiagSetValue
CDiagSetValueDoc::CDiagSetValueDoc() :
	_jobId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CDiagSetValueDoc::~CDiagSetValueDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CDiagSetValueDoc::initialize(
	const UUID jobId,
	const std::string fileAlias,
	const SmartPtrCPropertyDoc value) {
	if (! _isInitialized) {
		_jobId = jobId;
		_fileAlias = fileAlias;
		_value = value;

		_isInitialized = true;
	}
}

/// Accessor for the JobId
UUID CDiagSetValueDoc::getJobId() const {
	return _jobId;
}

/// Accessor for the FileAlias
std::string CDiagSetValueDoc::getFileAlias() const {
	return _fileAlias;
}

/// Accessor for the Value
SmartPtrCPropertyDoc CDiagSetValueDoc::getValue() const {
	return _value;
}





