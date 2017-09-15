/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CInstallRequestDoc_h_
#define CInstallRequestDoc_h_


#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestHeaderDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallBatchDoc.h"

namespace Caf {

/// A simple container for objects of type InstallRequest
class CAFINSTALLREQUESTDOC_LINKAGE CInstallRequestDoc {
public:
	CInstallRequestDoc();
	virtual ~CInstallRequestDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID clientId,
		const UUID requestId,
		const std::string pmeId,
		const SmartPtrCRequestHeaderDoc requestHeader,
		const SmartPtrCInstallBatchDoc batch,
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
	SmartPtrCInstallBatchDoc getBatch() const;

	/// Accessor for the AttachmentCollection
	SmartPtrCAttachmentCollectionDoc getAttachmentCollection() const;

private:
	UUID _clientId;
	UUID _requestId;
	std::string _pmeId;
	SmartPtrCRequestHeaderDoc _requestHeader;
	SmartPtrCInstallBatchDoc _batch;
	SmartPtrCAttachmentCollectionDoc _attachmentCollection;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CInstallRequestDoc);
};

CAF_DECLARE_SMART_POINTER(CInstallRequestDoc);

}

#endif
