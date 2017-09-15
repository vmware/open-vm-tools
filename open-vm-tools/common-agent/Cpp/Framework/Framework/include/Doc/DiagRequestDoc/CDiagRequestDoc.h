/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CDiagRequestDoc_h_
#define CDiagRequestDoc_h_


#include "Doc/CafCoreTypesDoc/CRequestHeaderDoc.h"
#include "Doc/DiagTypesDoc/CDiagBatchDoc.h"

namespace Caf {

/// A simple container for objects of type DiagRequest
class DIAGREQUESTDOC_LINKAGE CDiagRequestDoc {
public:
	CDiagRequestDoc();
	virtual ~CDiagRequestDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID clientId,
		const UUID requestId,
		const std::string pmeId,
		const SmartPtrCRequestHeaderDoc requestHeader,
		const SmartPtrCDiagBatchDoc batch);

public:
	/// Accessor for the ClientId
	UUID getClientId() const;

	/// Accessor for the RequestId
	UUID getRequestId() const;

	/// Accessor for the PmeId
	std::string getPmeId() const;

	/// Accessor for the RequestHeader
	SmartPtrCRequestHeaderDoc getRequestHeader() const;

	/// Accessor for the Batch
	SmartPtrCDiagBatchDoc getBatch() const;

private:
	UUID _clientId;
	UUID _requestId;
	std::string _pmeId;
	SmartPtrCRequestHeaderDoc _requestHeader;
	SmartPtrCDiagBatchDoc _batch;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CDiagRequestDoc);
};

CAF_DECLARE_SMART_POINTER(CDiagRequestDoc);

}

#endif
