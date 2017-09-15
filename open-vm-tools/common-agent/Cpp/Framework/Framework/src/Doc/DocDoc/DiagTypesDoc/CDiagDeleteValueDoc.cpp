/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/DiagTypesDoc/CDiagDeleteValueDoc.h"

using namespace Caf;

/// A simple container for objects of type DiagDeleteValue
CDiagDeleteValueDoc::CDiagDeleteValueDoc() :
	_jobId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CDiagDeleteValueDoc::~CDiagDeleteValueDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CDiagDeleteValueDoc::initialize(
	const UUID jobId,
	const std::string fileAlias,
	const std::string valueName) {
	if (! _isInitialized) {
		_jobId = jobId;
		_fileAlias = fileAlias;
		_valueName = valueName;

		_isInitialized = true;
	}
}

/// Accessor for the JobId
UUID CDiagDeleteValueDoc::getJobId() const {
	return _jobId;
}

/// Accessor for the FileAlias
std::string CDiagDeleteValueDoc::getFileAlias() const {
	return _fileAlias;
}

/// Accessor for the ValueName
std::string CDiagDeleteValueDoc::getValueName() const {
	return _valueName;
}





