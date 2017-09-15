/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CProviderRequestDoc_h_
#define CProviderRequestDoc_h_


#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/ProviderRequestDoc/CProviderBatchDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestHeaderDoc.h"

namespace Caf {

/// A simple container for objects of type ProviderRequest
class PROVIDERREQUESTDOC_LINKAGE CProviderRequestDoc {
public:
	CProviderRequestDoc();
	virtual ~CProviderRequestDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID clientId,
		const UUID requestId,
		const std::string pmeId,
		const SmartPtrCProviderRequestHeaderDoc requestHeader,
		const SmartPtrCProviderBatchDoc batch,
		const SmartPtrCAttachmentCollectionDoc attachmentCollection);

public:
	/// Accessor for the ClientId
	UUID getClientId() const;

	/// Accessor for the RequestId
	UUID getRequestId() const;

	/// Accessor for the PmeId
	std::string getPmeId() const;

	/// Accessor for the RequestHeader
	SmartPtrCProviderRequestHeaderDoc getRequestHeader() const;

	/// Accessor for the Batch
	SmartPtrCProviderBatchDoc getBatch() const;

	/// Accessor for the AttachmentCollection
	SmartPtrCAttachmentCollectionDoc getAttachmentCollection() const;

private:
	UUID _clientId;
	UUID _requestId;
	std::string _pmeId;
	SmartPtrCProviderRequestHeaderDoc _requestHeader;
	SmartPtrCProviderBatchDoc _batch;
	SmartPtrCAttachmentCollectionDoc _attachmentCollection;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CProviderRequestDoc);
};

CAF_DECLARE_SMART_POINTER(CProviderRequestDoc);

}

#endif
