/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CInlineAttachmentDoc_h_
#define CInlineAttachmentDoc_h_

namespace Caf {

/// A simple container for objects of type InlineAttachment
class CAFCORETYPESDOC_LINKAGE CInlineAttachmentDoc {
public:
	CInlineAttachmentDoc();
	virtual ~CInlineAttachmentDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string name,
		const std::string type,
		const std::string value);

public:
	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the Type
	std::string getType() const;

	/// Accessor for the Value
	std::string getValue() const;

private:
	bool _isInitialized;

	std::string _name;
	std::string _type;
	std::string _value;

private:
	CAF_CM_DECLARE_NOCOPY(CInlineAttachmentDoc);
};

CAF_DECLARE_SMART_POINTER(CInlineAttachmentDoc);

}

#endif
