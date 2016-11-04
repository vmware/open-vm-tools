/*
 *  Author: bwilliams
 *  Created: May 24, 2015
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef CAuthnAuthzDoc_h_
#define CAuthnAuthzDoc_h_

namespace Caf {

/// A simple container for objects of type AuthnAuthz
class CAFCORETYPESDOC_LINKAGE CAuthnAuthzDoc {
public:
	CAuthnAuthzDoc();
	virtual ~CAuthnAuthzDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string type,
		const std::string value,
		const std::string name = std::string(),
		const int32 sequenceNumber = 0);

public:
	/// Accessor for the Type
	std::string getType() const;

	/// Accessor for the Value
	std::string getValue() const;

	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the SequenceNumber
	int32 getSequenceNumber() const;

private:
	bool _isInitialized;

	std::string _type;
	std::string _value;
	std::string _name;
	int32 _sequenceNumber;

private:
	CAF_CM_DECLARE_NOCOPY(CAuthnAuthzDoc);
};

CAF_DECLARE_SMART_POINTER(CAuthnAuthzDoc);

}

#endif
