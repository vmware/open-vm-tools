/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CProviderEventResponseDoc_h_
#define CProviderEventResponseDoc_h_


#include "Doc/CafCoreTypesDoc/CStatisticsDoc.h"
#include "Doc/ResponseDoc/CEventKeyCollectionDoc.h"
#include "Doc/ResponseDoc/CEventManifestDoc.h"
#include "Doc/ResponseDoc/CResponseHeaderDoc.h"

namespace Caf {

/// A simple container for objects of type ProviderEventResponse
class RESPONSEDOC_LINKAGE CProviderEventResponseDoc {
public:
	CProviderEventResponseDoc();
	virtual ~CProviderEventResponseDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string pmeId,
		const SmartPtrCResponseHeaderDoc responseHeader,
		const SmartPtrCEventManifestDoc manifest,
		const SmartPtrCEventKeyCollectionDoc eventKeyCollection,
		const SmartPtrCStatisticsDoc statistics);

public:
	/// Accessor for the PmeId
	std::string getPmeId() const;

	/// Accessor for the ResponseHeader
	SmartPtrCResponseHeaderDoc getResponseHeader() const;

	/// Accessor for the Manifest
	SmartPtrCEventManifestDoc getManifest() const;

	/// Accessor for the EventKeyCollection
	SmartPtrCEventKeyCollectionDoc getEventKeyCollection() const;

	/// Accessor for the Statistics
	SmartPtrCStatisticsDoc getStatistics() const;

private:
	bool _isInitialized;

	std::string _pmeId;
	SmartPtrCResponseHeaderDoc _responseHeader;
	SmartPtrCEventManifestDoc _manifest;
	SmartPtrCEventKeyCollectionDoc _eventKeyCollection;
	SmartPtrCStatisticsDoc _statistics;

private:
	CAF_CM_DECLARE_NOCOPY(CProviderEventResponseDoc);
};

CAF_DECLARE_SMART_POINTER(CProviderEventResponseDoc);

}

#endif
