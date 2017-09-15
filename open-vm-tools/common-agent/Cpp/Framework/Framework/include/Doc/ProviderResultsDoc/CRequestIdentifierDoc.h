/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CRequestIdentifierDoc_h_
#define CRequestIdentifierDoc_h_


#include "Doc/SchemaTypesDoc/CActionClassDoc.h"

namespace Caf {

/// Fields that allow client to determine which request resulted in this response document
class PROVIDERRESULTSDOC_LINKAGE CRequestIdentifierDoc {
public:
	CRequestIdentifierDoc();
	virtual ~CRequestIdentifierDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID clientId,
		const UUID requestId,
		const std::string pmeId,
		const UUID jobId,
		const SmartPtrCActionClassDoc actionClass,
		const UUID sessionId);

public:
	/// Accessor for the ClientId
	UUID getClientId() const;

	/// Accessor for the RequestId
	UUID getRequestId() const;

	/// Accessor for the PmeId
	std::string getPmeId() const;

	/// Identifier of the specific job within the request
	UUID getJobId() const;

	/// Accessor for the ActionClass
	SmartPtrCActionClassDoc getActionClass() const;

	/// Client-configurable identifier that is opaque (not used) by the Common Agent Framework
	UUID getSessionId() const;

private:
	UUID _clientId;
	UUID _requestId;
	std::string _pmeId;
	UUID _jobId;
	SmartPtrCActionClassDoc _actionClass;
	UUID _sessionId;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CRequestIdentifierDoc);
};

CAF_DECLARE_SMART_POINTER(CRequestIdentifierDoc);

}

#endif
