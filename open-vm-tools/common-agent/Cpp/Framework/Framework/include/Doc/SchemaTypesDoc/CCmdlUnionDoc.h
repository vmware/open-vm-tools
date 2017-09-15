/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CCmdlUnionDoc_h_
#define CCmdlUnionDoc_h_

namespace Caf {

/// A simple container for objects of type CmdlUnion
class SCHEMATYPESDOC_LINKAGE CCmdlUnionDoc {
public:
	CCmdlUnionDoc();
	virtual ~CCmdlUnionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string namespaceVal,
		const std::string name,
		const std::string version);

public:
	/// Accessor for the NamespaceVal
	std::string getNamespaceVal() const;

	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the Version
	std::string getVersion() const;

private:
	bool _isInitialized;

	std::string _namespaceVal;
	std::string _name;
	std::string _version;

private:
	CAF_CM_DECLARE_NOCOPY(CCmdlUnionDoc);
};

CAF_DECLARE_SMART_POINTER(CCmdlUnionDoc);

}

#endif
