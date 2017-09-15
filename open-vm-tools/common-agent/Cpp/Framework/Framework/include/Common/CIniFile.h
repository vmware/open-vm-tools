/*
 *  Author: bwilliams
 *  Created: May 18, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CINIFILE_H_
#define CINIFILE_H_


#include "Common/CCafRegex.h"

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CIniFile {
public:
	struct SIniEntry {
		std::string _name;
		std::string _valueRaw;
		std::string _valueExpanded;
	};
	CAF_DECLARE_SMART_POINTER(SIniEntry);

	struct SIniSection {
		std::string _sectionName;
		std::deque<SmartPtrSIniEntry> _entryCollection;
	};
	CAF_DECLARE_SMART_POINTER(SIniSection);

public:
	CIniFile();
	virtual ~CIniFile();

public:
	void initialize(const std::string& configFilePath);

	std::deque<SmartPtrSIniSection> getSectionCollection();

	std::deque<SmartPtrSIniEntry> getEntryCollection(
		const std::string& sectionName);

	SmartPtrSIniEntry findOptionalEntry(
		const std::string& sectionName,
		const std::string& keyName);

	SmartPtrSIniEntry findRequiredEntry(
		const std::string& sectionName,
		const std::string& keyName);

	std::string findOptionalString(
		const std::string& sectionName,
		const std::string& keyName);

	std::string findRequiredString(
		const std::string& sectionName,
		const std::string& keyName);

	std::string findOptionalRawString(
		const std::string& sectionName,
		const std::string& keyName);

	std::string findRequiredRawString(
		const std::string& sectionName,
		const std::string& keyName);

	void log();

	void setValue(
		const std::string& sectionName,
		const std::string& keyName,
		const std::string& value);

	void deleteValue(
		const std::string& sectionName,
		const std::string& keyName);

private:
	struct SReplacement {
		SmartPtrCCafRegex _regex;
		std::string _value;
	};
	CAF_DECLARE_SMART_POINTER(SReplacement);

private:
	std::deque<SmartPtrSIniSection> parse(
		const std::string& configFilePath) const;

	SmartPtrSReplacement createReplacement(
		const std::string& keyName,
		const std::string& value) const;

	SmartPtrSIniEntry createIniEntry(
		const std::string& keyName,
		const std::string& valueRaw,
		const std::string& valueExpanded) const;

	void parseValuePath(
		const std::string& valuePath,
		std::string& valueName,
		std::string& valueValue);

private:
	bool _isInitialized;
	std::string _configFilePath;
	std::deque<SmartPtrSIniSection> _sectionCollection;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CIniFile);
};

CAF_DECLARE_SMART_POINTER(CIniFile);

}

#endif /* CINIFILE_H_ */
