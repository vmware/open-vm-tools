/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 *
 */

#ifndef CJoinTypeDoc_h_
#define CJoinTypeDoc_h_

namespace Caf {

/// A simple container for objects of type JoinType
class SCHEMATYPESDOC_LINKAGE CJoinTypeDoc {
public:
	CJoinTypeDoc();
	virtual ~CJoinTypeDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const OPERATOR_TYPE operand,
		const SmartPtrCClassFieldDoc dataClassLeft,
		const SmartPtrCClassFieldDoc dataClassRight,
		const std::string description);

public:
	/// Defines the operand used to join the class fields. Restricted to '=' for now
	OPERATOR_TYPE getOperand() const;

	/// Identifies the fields on classes that uniquely identify relationship
	SmartPtrCClassFieldDoc getDataClassLeft() const;

	/// Identifies the fields on classes that uniquely identify relationship
	SmartPtrCClassFieldDoc getDataClassRight() const;

	/// A short human-readable description of the join
	std::string getDescription() const;

private:
	OPERATOR_TYPE _operand;
	SmartPtrCClassFieldDoc _dataClassLeft;
	SmartPtrCClassFieldDoc _dataClassRight;
	std::string _description;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CJoinTypeDoc);
};

CAF_DECLARE_SMART_POINTER(CJoinTypeDoc);

}

#endif
