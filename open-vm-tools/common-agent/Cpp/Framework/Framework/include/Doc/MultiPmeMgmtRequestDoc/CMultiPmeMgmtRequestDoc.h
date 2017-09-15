/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CMultiPmeMgmtRequestDoc_h_
#define CMultiPmeMgmtRequestDoc_h_


#include "Doc/CafCoreTypesDoc/CRequestHeaderDoc.h"
#include "Doc/MultiPmeMgmtRequestDoc/CMultiPmeMgmtBatchCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type MultiPmeMgmtRequest
class MULTIPMEMGMTREQUESTDOC_LINKAGE CMultiPmeMgmtRequestDoc {
public:
	CMultiPmeMgmtRequestDoc();
	virtual ~CMultiPmeMgmtRequestDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID clientId,
		const UUID requestId,
		const SmartPtrCRequestHeaderDoc requestHeader,
		const SmartPtrCMultiPmeMgmtBatchCollectionDoc multiPmeBatchCollection);

public:
	/// Accessor for the ClientId
	UUID getClientId() const;

	/// Accessor for the RequestId
	UUID getRequestId() const;

	/// Accessor for the RequestHeader
	SmartPtrCRequestHeaderDoc getRequestHeader() const;

	/// Accessor for the MultiPmeBatchCollection
	SmartPtrCMultiPmeMgmtBatchCollectionDoc getMultiPmeBatchCollection() const;

private:
	UUID _clientId;
	UUID _requestId;
	SmartPtrCRequestHeaderDoc _requestHeader;
	SmartPtrCMultiPmeMgmtBatchCollectionDoc _multiPmeBatchCollection;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CMultiPmeMgmtRequestDoc);
};

CAF_DECLARE_SMART_POINTER(CMultiPmeMgmtRequestDoc);

}

#endif
