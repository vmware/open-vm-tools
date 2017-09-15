/*
 *  Author: bwilliams
 *  Created: May 18, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CINIFILEWITHOUTSECTION_H_
#define CINIFILEWITHOUTSECTION_H_


#include "Common/CCafRegex.h"
#include "Common/CIniFile.h"

namespace Caf {

class CIniFileWithoutSection {
public:
	struct SIniEntry {
		std::string _name;
		std::string _valueRaw;
		std::string _valueExpanded;
	};
	CAF_DECLARE_SMART_POINTER(SIniEntry);

public:
	CIniFileWithoutSection();
	virtual ~CIniFileWithoutSection();

public:
	void initialize(const std::string& configFilePath);

	std::deque<SmartPtrSIniEntry> getEntryCollection();

	SmartPtrSIniEntry findOptionalEntry(
		const std::string& keyName);

	SmartPtrSIniEntry findRequiredEntry(
		const std::string& keyName);

	std::string findOptionalString(
		const std::string& keyName);

	std::string findRequiredString(
		const std::string& keyName);

	std::string findOptionalRawString(
		const std::string& keyName);

	std::string findRequiredRawString(
		const std::string& keyName);

	void log();

	void setValue(
		const std::string valueName,
		const std::string valueValue);

	void deleteValue(
		const std::string valueName);

private:
	struct SReplacement {
		SmartPtrCCafRegex _regex;
		std::string _value;
	};
	CAF_DECLARE_SMART_POINTER(SReplacement);

private:
	std::deque<SmartPtrSIniEntry> parse(
		const std::string& configFilePath) const;

	SmartPtrSReplacement createReplacement(
		const std::string& keyName,
		const std::string& value) const;

	SmartPtrSIniEntry createIniEntry(
		const std::string& keyName,
		const std::string& valueRaw,
		const std::string& valueExpanded) const;

	std::deque<std::string> loadTextFileIntoCollection(
		const std::string& filePath) const;

	void saveTextFile(
		const std::deque<std::string> fileContents,
		const std::string filePath) const;

private:
	bool _isInitialized;
	std::string _configFilePath;
	std::deque<SmartPtrSIniEntry> _entryCollection;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CIniFileWithoutSection);
};

CAF_DECLARE_SMART_POINTER(CIniFileWithoutSection);

}

#endif /* CINIFILEWITHOUTSECTION_H_ */
