/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/DiagTypesDoc/CDiagCollectInstancesDoc.h"

using namespace Caf;

/// A simple container for objects of type DiagCollectInstances
CDiagCollectInstancesDoc::CDiagCollectInstancesDoc() :
	_jobId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CDiagCollectInstancesDoc::~CDiagCollectInstancesDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CDiagCollectInstancesDoc::initialize(
	const UUID jobId) {
	if (! _isInitialized) {
		_jobId = jobId;

		_isInitialized = true;
	}
}

/// Accessor for the JobId
UUID CDiagCollectInstancesDoc::getJobId() const {
	return _jobId;
}





