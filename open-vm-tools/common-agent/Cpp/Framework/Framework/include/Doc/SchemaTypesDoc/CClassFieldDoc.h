/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CClassFieldDoc_h_
#define CClassFieldDoc_h_

namespace Caf {

/// Description of a class and the field used to identify one end of a relationship
class SCHEMATYPESDOC_LINKAGE CClassFieldDoc {
public:
	CClassFieldDoc();
	virtual ~CClassFieldDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string namespaceVal,
		const std::string name,
		const std::string version,
		const std::string field);

public:
	/// Accessor for the NamespaceVal
	std::string getNamespaceVal() const;

	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the Version
	std::string getVersion() const;

	/// Description of a class field used to identify one end of a relationship
	std::string getField() const;

private:
	bool _isInitialized;

	std::string _namespaceVal;
	std::string _name;
	std::string _version;
	std::string _field;

private:
	CAF_CM_DECLARE_NOCOPY(CClassFieldDoc);
};

CAF_DECLARE_SMART_POINTER(CClassFieldDoc);

}

#endif
