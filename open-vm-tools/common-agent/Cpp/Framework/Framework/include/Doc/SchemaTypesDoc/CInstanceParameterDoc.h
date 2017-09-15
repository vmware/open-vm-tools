/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CInstanceParameterDoc_h_
#define CInstanceParameterDoc_h_

namespace Caf {

/// A parameter containing a data class instance used by a method to control the outcome
class SCHEMATYPESDOC_LINKAGE CInstanceParameterDoc {
public:
	CInstanceParameterDoc();
	virtual ~CInstanceParameterDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string name,
		const std::string instanceNamespace,
		const std::string instanceName,
		const std::string instanceVersion,
		const bool isOptional = false,
		const bool isList = false,
		const std::string displayName = std::string(),
		const std::string description = std::string());

public:
	/// Name of parameter
	std::string getName() const;

	/// Namespace of instance object type
	std::string getInstanceNamespace() const;

	/// Name of instance object type
	std::string getInstanceName() const;

	/// Version of instance object type
	std::string getInstanceVersion() const;

	/// Indicates this parameter need not be passed
	bool getIsOptional() const;

	/// Indicates whether to expect a list of values as opposed to a single value (the default if this attribute is not present)
	bool getIsList() const;

	/// Human-readable version of the parameter name
	std::string getDisplayName() const;

	/// Short description of what the parameter is for
	std::string getDescription() const;

private:
	std::string _name;
	std::string _instanceNamespace;
	std::string _instanceName;
	std::string _instanceVersion;
	bool _isOptional;
	bool _isList;
	std::string _displayName;
	std::string _description;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CInstanceParameterDoc);
};

CAF_DECLARE_SMART_POINTER(CInstanceParameterDoc);

}

#endif
