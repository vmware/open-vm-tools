/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CLogicalRelationshipDoc_h_
#define CLogicalRelationshipDoc_h_


#include "Doc/SchemaTypesDoc/CClassCardinalityDoc.h"
#include "Doc/SchemaTypesDoc/SchemaTypesDocTypes.h"
#include "Doc/SchemaTypesDoc/CJoinTypeDoc.h"

namespace Caf {

/// Definition of a relationship between classes that can be described by identifying the fields on the classes that uniquely identify the relationship
class SCHEMATYPESDOC_LINKAGE CLogicalRelationshipDoc {
public:
	CLogicalRelationshipDoc();
	virtual ~CLogicalRelationshipDoc();

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
		const std::deque<SmartPtrCJoinTypeDoc> joinCollection,
		const std::string description = std::string());

public:
	/// Accessor for the NamespaceVal
	std::string getNamespaceVal() const;

	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the Version
	std::string getVersion() const;

	/// Accessor for the Arity
	ARITY_TYPE getArity() const;

	/// Accessor for the DataClassLeft
	SmartPtrCClassCardinalityDoc getDataClassLeft() const;

	/// Accessor for the DataClassRight
	SmartPtrCClassCardinalityDoc getDataClassRight() const;

	/// Defines a join condition of the relationship
	std::deque<SmartPtrCJoinTypeDoc> getJoinCollection() const;

	/// Accessor for the Description
	std::string getDescription() const;

private:
	std::string _namespaceVal;
	std::string _name;
	std::string _version;
	ARITY_TYPE _arity;
	SmartPtrCClassCardinalityDoc _dataClassLeft;
	SmartPtrCClassCardinalityDoc _dataClassRight;
	std::deque<SmartPtrCJoinTypeDoc> _joinCollection;
	std::string _description;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CLogicalRelationshipDoc);
};

CAF_DECLARE_SMART_POINTER(CLogicalRelationshipDoc);

}

#endif
