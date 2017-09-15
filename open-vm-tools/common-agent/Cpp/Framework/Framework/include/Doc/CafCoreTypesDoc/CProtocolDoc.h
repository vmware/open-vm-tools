/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CProtocolDoc_h_
#define CProtocolDoc_h_

namespace Caf {

/// A simple container for objects of Protocol
class CAFCORETYPESDOC_LINKAGE CProtocolDoc {
public:
	CProtocolDoc();
	virtual ~CProtocolDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string uri,
		const std::string name = std::string(),
		const int32 sequenceNumber = 0);

public:
	/// Accessor for the Uri
	std::string getUri() const;

	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the Value
	int32 getSequenceNumber() const;

private:
	int32 _sequenceNumber;
	bool _isInitialized;

	std::string _uri;
	std::string _name;

private:
	CAF_CM_DECLARE_NOCOPY(CProtocolDoc);
};

CAF_DECLARE_SMART_POINTER(CProtocolDoc);

}

#endif
