/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectSchemaDoc.h"

using namespace Caf;

/// A simple container for objects of type MgmtCollectSchema
CMgmtCollectSchemaDoc::CMgmtCollectSchemaDoc() :
	_jobId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CMgmtCollectSchemaDoc::~CMgmtCollectSchemaDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CMgmtCollectSchemaDoc::initialize(
	const UUID jobId) {
	if (! _isInitialized) {
		_jobId = jobId;

		_isInitialized = true;
	}
}

/// Accessor for the JobId
UUID CMgmtCollectSchemaDoc::getJobId() const {
	return _jobId;
}





