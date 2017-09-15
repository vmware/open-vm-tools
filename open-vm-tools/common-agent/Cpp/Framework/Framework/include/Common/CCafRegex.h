/*
 *	 Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CCAFREGEX_H_
#define CCAFREGEX_H_

#include "Common/CCafRegex.h"

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CCafRegex {
public:
	CCafRegex();
	virtual ~CCafRegex();

public:
	void initialize(const std::string& regex);

	bool isMatched(const std::string& source);

	std::map<std::string, std::string> matchNames(
		const std::string& source,
		const std::set<std::string>& names);

	std::deque<std::string> matchName(
		const std::string& source,
		const std::string& name);

	std::string match(
		const std::string& source,
		const int32 matchNum);

	std::string replaceLiteral(
		const std::string& source,
		const std::string& replacement);

public:
	static std::string replaceLiteral(
		const std::string& regex,
		const std::string& source,
		const std::string& replacement);

private:
	bool _isInitialized;
	GRegex* _gRegex;
	std::string _regex;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CCafRegex);
};

CAF_DECLARE_SMART_POINTER(CCafRegex);

}

#endif /* CCAFREGEX_H_ */
