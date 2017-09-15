/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CClassIdentifierDoc_h_
#define CClassIdentifierDoc_h_

namespace Caf {

/// Tuple of values to uniquely identify a class
class SCHEMATYPESDOC_LINKAGE CClassIdentifierDoc {
public:
	CClassIdentifierDoc();
	virtual ~CClassIdentifierDoc();

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
	CAF_CM_DECLARE_NOCOPY(CClassIdentifierDoc);
};

CAF_DECLARE_SMART_POINTER(CClassIdentifierDoc);

}

#endif
