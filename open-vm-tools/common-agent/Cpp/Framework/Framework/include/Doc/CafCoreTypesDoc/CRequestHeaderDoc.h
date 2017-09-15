/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CRequestHeaderDoc_h_
#define CRequestHeaderDoc_h_


#include "Doc/CafCoreTypesDoc/CAuthnAuthzCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CPropertyCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CProtocolCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestConfigDoc.h"

namespace Caf {

/// A simple container for objects of type RequestHeader
class CAFCORETYPESDOC_LINKAGE CRequestHeaderDoc {
public:
	CRequestHeaderDoc();
	virtual ~CRequestHeaderDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const SmartPtrCRequestConfigDoc requestConfig,
		const SmartPtrCAuthnAuthzCollectionDoc authnAuthzCollection = SmartPtrCAuthnAuthzCollectionDoc(),
		const SmartPtrCProtocolCollectionDoc protocolCollection = SmartPtrCProtocolCollectionDoc(),
		const SmartPtrCPropertyCollectionDoc echoPropertyBag = SmartPtrCPropertyCollectionDoc(),
		const std::string version = "1.0",
		const std::string createdDateTime = CDateTimeUtils::getCurrentDateTime(),
		const UUID sessionId = CAFCOMMON_GUID_NULL);

public:
	/// Accessor for the RequestConfig
	SmartPtrCRequestConfigDoc getRequestConfig() const;

	/// Accessor for the Authentication / Authorization Collection
	SmartPtrCAuthnAuthzCollectionDoc getAuthnAuthzCollection() const;

	/// Accessor for the Protocol Collection
	SmartPtrCProtocolCollectionDoc getProtocolCollection() const;

	/// Accessor for the EchoPropertyBag
	SmartPtrCPropertyCollectionDoc getEchoPropertyBag() const;

	/// Accessor for the version
	std::string getVersion() const;

	/// Accessor for the date/time when the request was created
	std::string getCreatedDateTime() const;

	/// Accessor for the session ID
	UUID getSessionId() const;

private:
	bool _isInitialized;

	SmartPtrCRequestConfigDoc _requestConfig;
	SmartPtrCAuthnAuthzCollectionDoc _authnAuthzCollection;
	SmartPtrCProtocolCollectionDoc _protocolCollection;
	SmartPtrCPropertyCollectionDoc _echoPropertyBag;
	std::string _version;
	std::string _createdDateTime;
	UUID _sessionId;

private:
	CAF_CM_DECLARE_NOCOPY(CRequestHeaderDoc);
};

CAF_DECLARE_SMART_POINTER(CRequestHeaderDoc);

}

#endif
