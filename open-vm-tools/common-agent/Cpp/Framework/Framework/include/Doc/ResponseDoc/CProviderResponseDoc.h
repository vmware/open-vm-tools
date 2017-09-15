/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CProviderResponseDoc_h_
#define CProviderResponseDoc_h_


#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CStatisticsDoc.h"
#include "Doc/ResponseDoc/CManifestDoc.h"
#include "Doc/ResponseDoc/CResponseHeaderDoc.h"

namespace Caf {

/// A simple container for objects of type ProviderResponse
class RESPONSEDOC_LINKAGE CProviderResponseDoc {
public:
	CProviderResponseDoc();
	virtual ~CProviderResponseDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID clientId,
		const UUID requestId,
		const std::string pmeId,
		const SmartPtrCResponseHeaderDoc responseHeader,
		const SmartPtrCManifestDoc manifest,
		const SmartPtrCAttachmentCollectionDoc attachmentCollection,
		const SmartPtrCStatisticsDoc statistics);

public:
	/// Accessor for the ClientId
	UUID getClientId() const;

	/// Accessor for the RequestId
	UUID getRequestId() const;

	/// Accessor for the PmeId
	std::string getPmeId() const;

	/// Accessor for the ResponseHeader
	SmartPtrCResponseHeaderDoc getResponseHeader() const;

	/// Accessor for the Manifest
	SmartPtrCManifestDoc getManifest() const;

	/// Accessor for the AttachmentCollection
	SmartPtrCAttachmentCollectionDoc getAttachmentCollection() const;

	/// Accessor for the Statistics
	SmartPtrCStatisticsDoc getStatistics() const;

private:
	UUID _clientId;
	UUID _requestId;
	std::string _pmeId;
	SmartPtrCResponseHeaderDoc _responseHeader;
	SmartPtrCManifestDoc _manifest;
	SmartPtrCAttachmentCollectionDoc _attachmentCollection;
	SmartPtrCStatisticsDoc _statistics;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CProviderResponseDoc);
};

CAF_DECLARE_SMART_POINTER(CProviderResponseDoc);

}

#endif
