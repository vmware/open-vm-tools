/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CMgmtRequestDoc_h_
#define CMgmtRequestDoc_h_


#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestHeaderDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtBatchDoc.h"

namespace Caf {

/// A simple container for objects of type MgmtRequest
class MGMTREQUESTDOC_LINKAGE CMgmtRequestDoc {
public:
	CMgmtRequestDoc();
	virtual ~CMgmtRequestDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID clientId,
		const UUID requestId,
		const std::string pmeId,
		const SmartPtrCRequestHeaderDoc requestHeader,
		const SmartPtrCMgmtBatchDoc batch,
		const SmartPtrCAttachmentCollectionDoc attachmentCollection);

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
	SmartPtrCMgmtBatchDoc getBatch() const;

	/// Accessor for the AttachmentCollection
	SmartPtrCAttachmentCollectionDoc getAttachmentCollection() const;

private:
	UUID _clientId;
	UUID _requestId;
	std::string _pmeId;
	SmartPtrCRequestHeaderDoc _requestHeader;
	SmartPtrCMgmtBatchDoc _batch;
	SmartPtrCAttachmentCollectionDoc _attachmentCollection;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CMgmtRequestDoc);
};

CAF_DECLARE_SMART_POINTER(CMgmtRequestDoc);

}

#endif
