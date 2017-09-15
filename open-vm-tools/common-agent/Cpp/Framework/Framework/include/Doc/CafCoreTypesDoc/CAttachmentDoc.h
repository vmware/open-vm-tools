/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef CAttachmentDoc_h_
#define CAttachmentDoc_h_

#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"

namespace Caf {

/// A simple container for objects of type Attachment
class CAFCORETYPESDOC_LINKAGE CAttachmentDoc {
public:
	CAttachmentDoc();
	virtual ~CAttachmentDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string name,
		const std::string type,
		const std::string uri,
		const bool isReference,
		const CMS_POLICY cmsPolicy = CMS_POLICY_CAF_ENCRYPTED_AND_SIGNED);

public:
	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the Type
	std::string getType() const;

	/// Accessor for the Uri
	std::string getUri() const;

	/// Accessor for the IsReference
	bool getIsReference() const;

	/// Accessor for the CMS Policy
	CMS_POLICY getCmsPolicy() const;

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
