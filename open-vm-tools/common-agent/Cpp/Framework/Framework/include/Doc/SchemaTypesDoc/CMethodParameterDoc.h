/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CMethodParameterDoc_h_
#define CMethodParameterDoc_h_

#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"

namespace Caf {

/// A parameter containing a simple type used by a method to control the outcome
class SCHEMATYPESDOC_LINKAGE CMethodParameterDoc {
public:
	CMethodParameterDoc();
	virtual ~CMethodParameterDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string name,
		const PARAMETER_TYPE type,
		const bool isOptional = false,
		const bool isList = false,
		const std::string defaultVal = std::string(),
		const std::string displayName = std::string(),
		const std::string description = std::string());

public:
	/// Name of parameter
	std::string getName() const;

	/// Describes the data type of the property
	PARAMETER_TYPE getType() const;

	/// Indicates this parameter need not be passed
	bool getIsOptional() const;

	/// Indicates whether to expect a list of values as opposed to a single value (the default if this attribute is not present)
	bool getIsList() const;

	/// Accessor for the DefaultVal
	std::string getDefaultVal() const;

	/// Human-readable version of the parameter name
	std::string getDisplayName() const;

	/// Short description of what the parameter is for
	std::string getDescription() const;

private:
	std::string _name;
	PARAMETER_TYPE _type;
	bool _isOptional;
	bool _isList;
	std::string _defaultVal;
	std::string _displayName;
	std::string _description;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CMethodParameterDoc);
};

CAF_DECLARE_SMART_POINTER(CMethodParameterDoc);

}

#endif
