/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CClassFiltersDoc_h_
#define CClassFiltersDoc_h_

namespace Caf {

/// A simple container for objects of type ClassFilters
class CAFCORETYPESDOC_LINKAGE CClassFiltersDoc {
public:
	CClassFiltersDoc();
	virtual ~CClassFiltersDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string dialect = std::string(),
		const std::deque<std::string> classFilter = std::deque<std::string>());

public:
	/// Accessor for the Dialect
	std::string getDialect() const;

	/// Accessor for the ClassFilter
	std::deque<std::string> getClassFilter() const;

private:
	bool _isInitialized;

	std::string _dialect;
	std::deque<std::string> _classFilter;

private:
	CAF_CM_DECLARE_NOCOPY(CClassFiltersDoc);
};

CAF_DECLARE_SMART_POINTER(CClassFiltersDoc);

}

#endif
