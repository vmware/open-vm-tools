/*
 *  Author: bwilliams
 *  Created: May 24, 2015
 *
 *  Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef CAuthnAuthzDoc_h_
#define CAuthnAuthzDoc_h_

namespace Caf {

/// A simple container for objects of type AuthnAuthz
class CAuthnAuthzDoc {
public:
	CAuthnAuthzDoc() :
		_isInitialized(false),
		_sequenceNumber(0) {}
	virtual ~CAuthnAuthzDoc() {}

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string type,
		const std::string value,
		const std::string name = std::string(),
		const int32 sequenceNumber = 0) {
		if (! _isInitialized) {
			_type = type;
			_value = value;
			_name = name;
			_sequenceNumber = sequenceNumber;

			_isInitialized = true;
		}
	}

public:
	/// Accessor for the Type
	std::string getType() const {
		return _type;
	}

	/// Accessor for the Value
	std::string getValue() const {
		return _value;
	}

	/// Accessor for the Name
	std::string getName() const {
		return _name;
	}

	/// Accessor for the SequenceNumber
	int32 getSequenceNumber() const {
		return _sequenceNumber;
	}

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
