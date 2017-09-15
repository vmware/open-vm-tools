/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CErrorResponseDoc_h_
#define CErrorResponseDoc_h_


#include "Doc/ResponseDoc/CResponseHeaderDoc.h"

namespace Caf {

/// A simple container for objects of type ErrorResponse
class RESPONSEDOC_LINKAGE CErrorResponseDoc {
public:
	CErrorResponseDoc();
	virtual ~CErrorResponseDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID clientId,
		const UUID requestId,
		const std::string pmeId,
		const SmartPtrCResponseHeaderDoc responseHeader,
		const std::string errorMessage);

public:
	/// Accessor for the ClientId
	UUID getClientId() const;

	/// Accessor for the RequestId
	UUID getRequestId() const;

	/// Accessor for the PmeId
	std::string getPmeId() const;

	/// Accessor for the Response Header
	SmartPtrCResponseHeaderDoc getResponseHeader() const;

	/// Accessor for the ErrorMessage
	std::string getErrorMessage() const;

private:
	UUID _clientId;
	UUID _requestId;
	std::string _pmeId;
	SmartPtrCResponseHeaderDoc _responseHeader;
	std::string _errorMessage;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CErrorResponseDoc);
};

CAF_DECLARE_SMART_POINTER(CErrorResponseDoc);

}

#endif
