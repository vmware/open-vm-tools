/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CRelationshipDoc_h_
#define CRelationshipDoc_h_


#include "Doc/SchemaTypesDoc/CClassCardinalityDoc.h"
#include "Doc/SchemaTypesDoc/SchemaTypesDocTypes.h"

namespace Caf {

/// Definition of a relationship between data classes
class SCHEMATYPESDOC_LINKAGE CRelationshipDoc {
public:
	CRelationshipDoc();
	virtual ~CRelationshipDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string namespaceVal,
		const std::string name,
		const std::string version,
		const ARITY_TYPE arity,
		const SmartPtrCClassCardinalityDoc dataClassLeft,
		const SmartPtrCClassCardinalityDoc dataClassRight,
		const std::string description = std::string());

public:
	/// Accessor for the NamespaceVal
	std::string getNamespaceVal() const;

	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the Version
	std::string getVersion() const;

	/// Number of parts (sides) to relationship. Restricted to a two-sided relationship for now
	ARITY_TYPE getArity() const;

	/// Identifies the two classes that make up the relationship
	SmartPtrCClassCardinalityDoc getDataClassLeft() const;

	/// Identifies the two classes that make up the relationship
	SmartPtrCClassCardinalityDoc getDataClassRight() const;

	/// A short human-readable description of the relationship
	std::string getDescription() const;

private:
	std::string _namespaceVal;
	std::string _name;
	std::string _version;
	ARITY_TYPE _arity;
	SmartPtrCClassCardinalityDoc _dataClassLeft;
	SmartPtrCClassCardinalityDoc _dataClassRight;
	std::string _description;
	bool _isInitialized;


private:
	CAF_CM_DECLARE_NOCOPY(CRelationshipDoc);
};

CAF_DECLARE_SMART_POINTER(CRelationshipDoc);

}

#endif
