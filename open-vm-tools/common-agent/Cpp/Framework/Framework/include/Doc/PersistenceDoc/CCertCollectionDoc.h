/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CCertCollectionDoc_h_
#define CCertCollectionDoc_h_

namespace Caf {

/// A simple container for objects of type CertCollection
class PERSISTENCEDOC_LINKAGE CCertCollectionDoc {
public:
	CCertCollectionDoc();
	virtual ~CCertCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<std::string> certCollection = std::deque<std::string>());

public:
	/// Accessor for the Cert
	std::deque<std::string> getCert() const;

private:
	bool _isInitialized;

	std::deque<std::string> _certCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CCertCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CCertCollectionDoc);

}

#endif
