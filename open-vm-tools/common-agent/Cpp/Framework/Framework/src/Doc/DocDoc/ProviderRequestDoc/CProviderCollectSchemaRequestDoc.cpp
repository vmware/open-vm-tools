/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/ProviderRequestDoc/CProviderRequestHeaderDoc.h"
#include "Doc/ProviderRequestDoc/CProviderCollectSchemaRequestDoc.h"

using namespace Caf;

/// A simple container for objects of type ProviderCollectSchemaRequest
CProviderCollectSchemaRequestDoc::CProviderCollectSchemaRequestDoc() :
	_clientId(CAFCOMMON_GUID_NULL),
	_requestId(CAFCOMMON_GUID_NULL),
	_jobId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CProviderCollectSchemaRequestDoc::~CProviderCollectSchemaRequestDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CProviderCollectSchemaRequestDoc::initialize(
	const UUID clientId,
	const UUID requestId,
	const std::string pmeId,
	const UUID jobId,
	const std::string outputDir,
	const SmartPtrCProviderRequestHeaderDoc requestHeader) {
	if (! _isInitialized) {
		_clientId = clientId;
		_requestId = requestId;
		_pmeId = pmeId;
		_jobId = jobId;
		_outputDir = outputDir;
		_requestHeader = requestHeader;

		_isInitialized = true;
	}
}

/// Accessor for the ClientId
UUID CProviderCollectSchemaRequestDoc::getClientId() const {
	return _clientId;
}

/// Accessor for the RequestId
UUID CProviderCollectSchemaRequestDoc::getRequestId() const {
	return _requestId;
}

/// Accessor for the PmeId
std::string CProviderCollectSchemaRequestDoc::getPmeId() const {
	return _pmeId;
}

/// Accessor for the JobId
UUID CProviderCollectSchemaRequestDoc::getJobId() const {
	return _jobId;
}

/// Accessor for the OutputDir
std::string CProviderCollectSchemaRequestDoc::getOutputDir() const {
	return _outputDir;
}

/// Accessor for the RequestHeader
SmartPtrCProviderRequestHeaderDoc CProviderCollectSchemaRequestDoc::getRequestHeader() const {
	return _requestHeader;
}





