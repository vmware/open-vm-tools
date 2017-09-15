/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CClassCardinalityDoc_h_
#define CClassCardinalityDoc_h_

namespace Caf {

/// Class description of one end of a relationship
class SCHEMATYPESDOC_LINKAGE CClassCardinalityDoc {
public:
	CClassCardinalityDoc();
	virtual ~CClassCardinalityDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string namespaceVal,
		const std::string name,
		const std::string version,
		const std::string cardinality);

public:
	/// Accessor for the NamespaceVal
	std::string getNamespaceVal() const;

	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the Version
	std::string getVersion() const;

	/// Cardinality of one end relationship, i.e. has one, has many, etc
	std::string getCardinality() const;

private:
	bool _isInitialized;

	std::string _namespaceVal;
	std::string _name;
	std::string _version;
	std::string _cardinality;

private:
	CAF_CM_DECLARE_NOCOPY(CClassCardinalityDoc);
};

CAF_DECLARE_SMART_POINTER(CClassCardinalityDoc);

}

#endif
