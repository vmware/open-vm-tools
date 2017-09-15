/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CRequestInstanceParameterDoc_h_
#define CRequestInstanceParameterDoc_h_

namespace Caf {

/// A simple container for objects of type RequestInstanceParameter
class CAFCORETYPESDOC_LINKAGE CRequestInstanceParameterDoc {
public:
	CRequestInstanceParameterDoc();
	virtual ~CRequestInstanceParameterDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string name,
		const std::string classNamespace,
		const std::string className,
		const std::string classVersion,
		const std::deque<std::string> value);

public:
	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the ClassNamespace
	std::string getClassNamespace() const;

	/// Accessor for the ClassName
	std::string getClassName() const;

	/// Accessor for the ClassVersion
	std::string getClassVersion() const;

	/// Accessor for the Value
	std::deque<std::string> getValue() const;

private:
	bool _isInitialized;

	std::string _name;
	std::string _classNamespace;
	std::string _className;
	std::string _classVersion;
	std::deque<std::string> _value;

private:
	CAF_CM_DECLARE_NOCOPY(CRequestInstanceParameterDoc);
};

CAF_DECLARE_SMART_POINTER(CRequestInstanceParameterDoc);

}

#endif
