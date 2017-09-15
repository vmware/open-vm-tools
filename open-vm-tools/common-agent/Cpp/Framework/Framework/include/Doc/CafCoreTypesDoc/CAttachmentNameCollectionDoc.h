/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CAttachmentNameCollectionDoc_h_
#define CAttachmentNameCollectionDoc_h_

namespace Caf {

/// A simple container for objects of type AttachmentNameCollection
class CAFCORETYPESDOC_LINKAGE CAttachmentNameCollectionDoc {
public:
	CAttachmentNameCollectionDoc();
	virtual ~CAttachmentNameCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<std::string> name = std::deque<std::string>());

public:
	/// Accessor for the Name
	std::deque<std::string> getName() const;

private:
	bool _isInitialized;

	std::deque<std::string> _name;

private:
	CAF_CM_DECLARE_NOCOPY(CAttachmentNameCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CAttachmentNameCollectionDoc);

}

#endif
