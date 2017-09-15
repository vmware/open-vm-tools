/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CClassPropertyDoc_h_
#define CClassPropertyDoc_h_

#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"
#include "Doc/SchemaTypesDoc/SchemaTypesDocTypes.h"

namespace Caf {

/// Definition of an attribute (field) of a class
class SCHEMATYPESDOC_LINKAGE CClassPropertyDoc {
public:
	CClassPropertyDoc();
	virtual ~CClassPropertyDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string name,
		const PROPERTY_TYPE type,
		const std::deque<std::string> value,
		const bool required = false,
		const bool key = false,
		const bool list = false,
		const bool caseSensitive = false,
		const bool transientVal = false,
		const std::string defaultVal = std::string(),
		const VALIDATOR_TYPE validator = VALIDATOR_NONE,
		const std::string upperRange = std::string(),
		const std::string lowerRange = std::string(),
		const std::string displayName = std::string(),
		const std::string description = std::string());

public:
	/// Property name
	std::string getName() const;

	/// Describes the data type of the property
	PROPERTY_TYPE getType() const;

	/// The contents of a validator used on this property
	std::deque<std::string> getValue() const;

	/// Whether this is a required property, i.e. this property must always be non-empty
	bool getRequired() const;

	/// Indicates this property may be used as a key identifying field
	bool getKey() const;

	/// Indicates whether to expect a list of properties in the provider response
	bool getList() const;

	/// Indicates whether a string field should be treated in a case-sensitive manner
	bool getCaseSensitive() const;

	/// Accessor for the TransientVal
	bool getTransientVal() const;

	/// Accessor for the DefaultVal
	std::string getDefaultVal() const;

	/// The type of validator described in the 'value' sub-elements
	VALIDATOR_TYPE getValidator() const;

	/// If a 'range' validator is in use, this describes the upper limit of allowable values for the property. QUESTIONABLE: how do we determine inclusive or exclusive range
	std::string getUpperRange() const;

	/// If a 'range' validator is in use, this describes the lower limit of allowable values for the property. QUESTIONABLE: how do we determine inclusive or exclusive range
	std::string getLowerRange() const;

	/// A hint as to what this property should be called when displaying it to a human
	std::string getDisplayName() const;

	/// A phrase to describe the property for mouse-over text, etc
	std::string getDescription() const;

private:
	std::string _name;
	PROPERTY_TYPE _type;
	std::deque<std::string> _value;
	bool _required;
	bool _key;
	bool _list;
	bool _caseSensitive;
	bool _transientVal;
	std::string _defaultVal;
	VALIDATOR_TYPE _validator;
	std::string _upperRange;
	std::string _lowerRange;
	std::string _displayName;
	std::string _description;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CClassPropertyDoc);
};

CAF_DECLARE_SMART_POINTER(CClassPropertyDoc);

}

#endif
