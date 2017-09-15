/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CResponseHeaderDoc_h_
#define CResponseHeaderDoc_h_

namespace Caf {

/// A simple container for objects of type ResponseHeader
class RESPONSEDOC_LINKAGE CResponseHeaderDoc {
public:
	CResponseHeaderDoc();
	virtual ~CResponseHeaderDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string version = "1.0",
		const std::string createdDateTime = CDateTimeUtils::getCurrentDateTime(),
		const uint32 sequenceNumber = 0,
		const bool isFinalResponse = true,
		const UUID sessionId = CAFCOMMON_GUID_NULL);

public:
	/// Accessor for the version
	std::string getVersion() const;

	/// Accessor for the date/time when the request was created
	std::string getCreatedDateTime() const;

	/// Accessor for the sequenceNumber
	uint32 getSequenceNumber() const;

	/// Accessor for the version
	bool getIsFinalResponse() const;

	/// Accessor for the session ID
	UUID getSessionId() const;

private:
	std::string _version;
	std::string _createdDateTime;
	uint32 _sequenceNumber;
	bool _isFinalResponse;
	UUID _sessionId;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CResponseHeaderDoc);
};

CAF_DECLARE_SMART_POINTER(CResponseHeaderDoc);

}

#endif
