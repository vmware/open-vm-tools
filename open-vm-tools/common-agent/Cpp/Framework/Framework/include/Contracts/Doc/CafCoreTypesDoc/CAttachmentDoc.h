/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef CAttachmentDoc_h_
#define CAttachmentDoc_h_

namespace Caf {

/// A simple container for objects of type Attachment
class CAttachmentDoc {
public:
	CAttachmentDoc() :
		_isInitialized(false),
		_isReference(false),
		_cmsPolicy(CMS_POLICY_CAF_ENCRYPTED_AND_SIGNED) {}
	virtual ~CAttachmentDoc() {}

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string name,
		const std::string type,
		const std::string uri,
		const bool isReference,
		const CMS_POLICY cmsPolicy = CMS_POLICY_CAF_ENCRYPTED_AND_SIGNED) {
		if (! _isInitialized) {
			_name = name;
			_type = type;
			_uri = uri;
			_isReference = isReference;
			_cmsPolicy = cmsPolicy;

			_isInitialized = true;
		}
	}

public:
	/// Accessor for the Name
	std::string getName() const {
		return _name;
	}

	/// Accessor for the Type
	std::string getType() const {
		return _type;
	}

	/// Accessor for the Uri
	std::string getUri() const {
		return _uri;
	}

	/// Accessor for the IsReference
	bool getIsReference() const {
		return _isReference;
	}

	/// Accessor for the CMS Policy
	CMS_POLICY getCmsPolicy() const {
		return _cmsPolicy;
	}

private:
	bool _isInitialized;

	std::string _name;
	std::string _type;
	std::string _uri;
	bool _isReference;
	CMS_POLICY _cmsPolicy;

private:
	CAF_CM_DECLARE_NOCOPY(CAttachmentDoc);
};

CAF_DECLARE_SMART_POINTER(CAttachmentDoc);

}

#endif
