/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CCertPathCollectionDoc_h_
#define CCertPathCollectionDoc_h_

namespace Caf {

/// A simple container for objects of type CertPathCollection
class PERSISTENCEDOC_LINKAGE CCertPathCollectionDoc {
public:
	CCertPathCollectionDoc();
	virtual ~CCertPathCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<std::string> certPathCollection = std::deque<std::string>());

public:
	/// Accessor for the Cert
	std::deque<std::string> getCertPath() const;

private:
	bool _isInitialized;

	std::deque<std::string> _certPathCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CCertPathCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CCertPathCollectionDoc);

}

#endif
