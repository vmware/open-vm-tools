/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CProviderCollectSchemaRequestDoc_h_
#define CProviderCollectSchemaRequestDoc_h_


#include "Doc/ProviderRequestDoc/CProviderRequestHeaderDoc.h"

namespace Caf {

/// A simple container for objects of type ProviderCollectSchemaRequest
class PROVIDERREQUESTDOC_LINKAGE CProviderCollectSchemaRequestDoc {
public:
	CProviderCollectSchemaRequestDoc();
	virtual ~CProviderCollectSchemaRequestDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID clientId,
		const UUID requestId,
		const std::string pmeId,
		const UUID jobId,
		const std::string outputDir,
		const SmartPtrCProviderRequestHeaderDoc requestHeader);

public:
	/// Accessor for the ClientId
	UUID getClientId() const;

	/// Accessor for the RequestId
	UUID getRequestId() const;

	/// Accessor for the PmeId
	std::string getPmeId() const;

	/// Accessor for the JobId
	UUID getJobId() const;

	/// Accessor for the OutputDir
	std::string getOutputDir() const;

	/// Accessor for the RequestHeader
	SmartPtrCProviderRequestHeaderDoc getRequestHeader() const;

private:
	UUID _clientId;
	UUID _requestId;
	std::string _pmeId;
	UUID _jobId;
	std::string _outputDir;
	SmartPtrCProviderRequestHeaderDoc _requestHeader;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CProviderCollectSchemaRequestDoc);
};

CAF_DECLARE_SMART_POINTER(CProviderCollectSchemaRequestDoc);

}

#endif
