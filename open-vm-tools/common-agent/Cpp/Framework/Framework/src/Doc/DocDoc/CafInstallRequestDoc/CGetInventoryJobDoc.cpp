/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/CafInstallRequestDoc/CGetInventoryJobDoc.h"

using namespace Caf;

/// A simple container for objects of type GetInventoryJob
CGetInventoryJobDoc::CGetInventoryJobDoc() :
	_jobId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CGetInventoryJobDoc::~CGetInventoryJobDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CGetInventoryJobDoc::initialize(
	const UUID jobId) {
	if (! _isInitialized) {
		_jobId = jobId;

		_isInitialized = true;
	}
}

/// Accessor for the JobId
UUID CGetInventoryJobDoc::getJobId() const {
	return _jobId;
}





