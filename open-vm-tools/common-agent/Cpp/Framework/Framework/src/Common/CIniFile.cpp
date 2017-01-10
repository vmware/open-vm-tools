/*
 *  Author: bwilliams
 *  Created: May 18, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/CCafRegex.h"
#include "Common/CIniFile.h"
#include "Exception/CCafException.h"

using namespace Caf;

CIniFile::CIniFile() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CIniFile") {
}

CIniFile::~CIniFile() {
}

void CIniFile::initialize(
	const std::string& configFilePath) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(configFilePath);

	_configFilePath = configFilePath;

	_isInitialized = true;
}

std::deque<CIniFile::SmartPtrSIniSection> CIniFile::getSectionCollection() {
	CAF_CM_FUNCNAME_VALIDATE("getSectionCollection");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::deque<SmartPtrSIniSection> sectionCollection;
	if (_sectionCollection.empty()) {
		_sectionCollection = parse(_configFilePath);
	}

	sectionCollection = _sectionCollection;

	return sectionCollection;
}

std::deque<CIniFile::SmartPtrSIniEntry> CIniFile::getEntryCollection(
	const std::string& sectionName) {
	CAF_CM_FUNCNAME_VALIDATE("getEntryCollection");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sectionName);

	std::deque<SmartPtrSIniEntry> entryCollection;
	if (_sectionCollection.empty()) {
		_sectionCollection = parse(_configFilePath);
	}

	for (TConstIterator<std::deque<SmartPtrSIniSection> > iniSectionIter(_sectionCollection);
		iniSectionIter; iniSectionIter++) {
		const SmartPtrSIniSection iniSection = *iniSectionIter;
		const std::string sectionNameTmp = iniSection->_sectionName;

		if (sectionNameTmp.compare(sectionName) == 0) {
			entryCollection = iniSection->_entryCollection;
		}
	}

	return entryCollection;
}

CIniFile::SmartPtrSIniEntry CIniFile::findOptionalEntry(
	const std::string& sectionName,
	const std::string& keyName) {
	CAF_CM_FUNCNAME_VALIDATE("findOptionalEntry");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sectionName);
	CAF_CM_VALIDATE_STRING(keyName);

	CIniFile::SmartPtrSIniEntry iniEntry;
	if (_sectionCollection.empty()) {
		_sectionCollection = parse(_configFilePath);
	}

	for (TConstIterator<std::deque<SmartPtrSIniSection> > iniSectionIter(_sectionCollection);
		iniSectionIter; iniSectionIter++) {
		const SmartPtrSIniSection iniSection = *iniSectionIter;
		const std::string sectionNameTmp = iniSection->_sectionName;

		if (sectionNameTmp.compare(sectionName) == 0) {
			for (TConstIterator<std::deque<SmartPtrSIniEntry> > iniEntryIter(iniSection->_entryCollection);
				iniEntryIter; iniEntryIter++) {
				const SmartPtrSIniEntry iniEntryTmp = *iniEntryIter;
				const std::string keyNameTmp = iniEntryTmp->_name;
				if (keyNameTmp.compare(keyName) == 0) {
					iniEntry = iniEntryTmp;
					break;
				}
			}
		}
	}

	return iniEntry;
}

CIniFile::SmartPtrSIniEntry CIniFile::findRequiredEntry(
	const std::string& sectionName,
	const std::string& keyName) {
	CAF_CM_FUNCNAME("findRequiredEntry");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sectionName);
	CAF_CM_VALIDATE_STRING(keyName);

	const SmartPtrSIniEntry iniEntry = findOptionalEntry(sectionName, keyName);
	if (iniEntry.IsNull()) {
		CAF_CM_EXCEPTIONEX_VA2(NoSuchElementException, ERROR_NOT_FOUND,
			"Value not found - sectionName: %s, keyName: %s", sectionName.c_str(), keyName.c_str());
	}

	return iniEntry;
}

std::string CIniFile::findOptionalString(
	const std::string& sectionName,
	const std::string& keyName) {
	CAF_CM_FUNCNAME_VALIDATE("findOptionalString");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sectionName);
	CAF_CM_VALIDATE_STRING(keyName);

	std::string value;
	const SmartPtrSIniEntry iniEntry = findOptionalEntry(sectionName, keyName);
	if (! iniEntry.IsNull()) {
		value = iniEntry->_valueExpanded;
	}

	return value;
}

std::string CIniFile::findRequiredString(
	const std::string& sectionName,
	const std::string& keyName) {
	CAF_CM_FUNCNAME_VALIDATE("findRequiredString");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sectionName);
	CAF_CM_VALIDATE_STRING(keyName);

	const SmartPtrSIniEntry iniEntry = findRequiredEntry(sectionName, keyName);
	return iniEntry->_valueExpanded;
}

std::string CIniFile::findOptionalRawString(
	const std::string& sectionName,
	const std::string& keyName) {
	CAF_CM_FUNCNAME_VALIDATE("findOptionalRawString");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sectionName);
	CAF_CM_VALIDATE_STRING(keyName);

	std::string value;
	const SmartPtrSIniEntry iniEntry = findOptionalEntry(sectionName, keyName);
	if (! iniEntry.IsNull()) {
		value = iniEntry->_valueRaw;
	}

	return value;
}

std::string CIniFile::findRequiredRawString(
	const std::string& sectionName,
	const std::string& keyName) {
	CAF_CM_FUNCNAME_VALIDATE("findRequiredRawString");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sectionName);
	CAF_CM_VALIDATE_STRING(keyName);

	const SmartPtrSIniEntry iniEntry = findRequiredEntry(sectionName, keyName);
	return iniEntry->_valueRaw;
}

void CIniFile::log() {
	CAF_CM_FUNCNAME_VALIDATE("log");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	if (_sectionCollection.empty()) {
		_sectionCollection = parse(_configFilePath);
	}

	for (TConstIterator<std::deque<SmartPtrSIniSection> > iniSectionIter(_sectionCollection);
		iniSectionIter; iniSectionIter++) {
		const SmartPtrSIniSection iniSection = *iniSectionIter;
		CAF_CM_LOG_DEBUG_VA1("Section - %s", iniSection->_sectionName.c_str());

		for (TConstIterator<std::deque<SmartPtrSIniEntry> > iniEntryIter(iniSection->_entryCollection);
			iniEntryIter; iniEntryIter++) {
			const SmartPtrSIniEntry iniEntry = *iniEntryIter;
			CAF_CM_LOG_DEBUG_VA3("  Entry - %s=%s (%s)",
				iniEntry->_name.c_str(), iniEntry->_valueRaw.c_str(), iniEntry->_valueExpanded.c_str());
		}
	}
}

void CIniFile::setValue(
	const std::string& sectionName,
	const std::string& keyName,
	const std::string& value) {
	CAF_CM_FUNCNAME("setValue");

	GKeyFile* gKeyFile = NULL;
	GError* gError = NULL;
	gchar* gFileContents = NULL;

	try {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(sectionName);
		CAF_CM_VALIDATE_STRING(keyName);
		CAF_CM_VALIDATE_STRING(value);

		try {
			gKeyFile = g_key_file_new();
			g_key_file_load_from_file(
				gKeyFile,
				_configFilePath.c_str(),
				G_KEY_FILE_NONE,
				&gError);
			if (gError != NULL) {
				throw gError;
			}

			g_key_file_set_string(
				gKeyFile,
				sectionName.c_str(),
				keyName.c_str(),
				value.c_str());

			gFileContents = g_key_file_to_data(
				gKeyFile,
				NULL,
				&gError);
			if (gError != NULL) {
				throw gError;
			}
			if (gKeyFile) {
				g_key_file_free(gKeyFile);
				gKeyFile = NULL;
			}

			g_file_set_contents(
				_configFilePath.c_str(),
				gFileContents,
				-1,
				&gError);
			if (gError != NULL) {
				throw gError;
			}

			_sectionCollection.clear();
		} catch (GError *gErrorExc) {
			CAF_CM_EXCEPTION_VA0(gErrorExc->code, gErrorExc->message);
		}
	}
    CAF_CM_CATCH_CAF
    CAF_CM_CATCH_DEFAULT

	try {
		if (gKeyFile) {
			g_key_file_free(gKeyFile);
		}

		if (gError) {
			g_error_free(gError);
		}

		if (gFileContents) {
			g_free(gFileContents);
		}
	}
    CAF_CM_CATCH_DEFAULT
    CAF_CM_LOG_CRIT_CAFEXCEPTION;
    CAF_CM_THROWEXCEPTION;
}

void CIniFile::deleteValue(
	const std::string& sectionName,
	const std::string& keyName) {
	CAF_CM_FUNCNAME("deleteValue");

	GKeyFile* gKeyFile = NULL;
	GError* gError = NULL;
	gchar* gFileContents = NULL;

	try {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(sectionName);
		CAF_CM_VALIDATE_STRING(keyName);

		try {
			gKeyFile = g_key_file_new();
			g_key_file_load_from_file(
				gKeyFile,
				_configFilePath.c_str(),
				G_KEY_FILE_NONE,
				&gError);
			if (gError != NULL) {
				throw gError;
			}

			g_key_file_remove_key(
				gKeyFile,
				sectionName.c_str(),
				keyName.c_str(),
				&gError);
			if (gError != NULL) {
				throw gError;
			}

			gFileContents = g_key_file_to_data(
				gKeyFile,
				NULL,
				&gError);
			if (gError != NULL) {
				throw gError;
			}
			if (gKeyFile) {
				g_key_file_free(gKeyFile);
				gKeyFile = NULL;
			}

			g_file_set_contents(
				_configFilePath.c_str(),
				gFileContents,
				-1,
				&gError);
			if (gError != NULL) {
				throw gError;
			}

			_sectionCollection.clear();
		} catch (GError *gErrorExc) {
			CAF_CM_EXCEPTION_VA0(gErrorExc->code, gErrorExc->message);
		}
	}
    CAF_CM_CATCH_CAF
    CAF_CM_CATCH_DEFAULT

	try {
		if (gKeyFile) {
			g_key_file_free(gKeyFile);
		}

		if (gError) {
			g_error_free(gError);
		}

		if (gFileContents) {
			g_free(gFileContents);
		}
	}
    CAF_CM_CATCH_DEFAULT
    CAF_CM_LOG_CRIT_CAFEXCEPTION;
    CAF_CM_THROWEXCEPTION;
}

std::deque<CIniFile::SmartPtrSIniSection> CIniFile::parse(
	const std::string& configFilePath) const {
	CAF_CM_FUNCNAME("parse");

	std::deque<SmartPtrSIniSection> iniSectionCollection;
	std::deque<SmartPtrSReplacement> replacementCollection;

	GKeyFile* gKeyFile = NULL;
	gchar** gGroupStrCollection = NULL;
	gchar** gKeyStrCollection = NULL;
	GError* gError = NULL;
	gchar* gValueStr = NULL;

	try {
		CAF_CM_VALIDATE_STRING(configFilePath);

		try {
			gKeyFile = g_key_file_new();
			g_key_file_load_from_file(
				gKeyFile,
				configFilePath.c_str(),
				G_KEY_FILE_NONE,
				&gError);
			if (gError != NULL) {
				throw gError;
			}

			gsize numGroups = 0;
			gGroupStrCollection = g_key_file_get_groups(gKeyFile, &numGroups);
			for (gsize groupNum = 0; groupNum < numGroups; groupNum++) {
				const gchar* gGroupNameStr = gGroupStrCollection[groupNum];
				const std::string groupName = gGroupNameStr;

				SmartPtrSIniSection iniSection;
				iniSection.CreateInstance();
				iniSection->_sectionName = groupName;

				gsize numKeys = 0;
				gKeyStrCollection = g_key_file_get_keys(
					gKeyFile,
					gGroupNameStr,
					&numKeys,
					&gError);

				for (gsize keyNum = 0; keyNum < numKeys; keyNum++) {
					const gchar* gKeyNameStr = gKeyStrCollection[keyNum];
					const std::string keyName = gKeyNameStr;

					gValueStr = g_key_file_get_string(
						gKeyFile,
						gGroupNameStr,
						gKeyNameStr,
						&gError);
					if (gError != NULL) {
						throw gError;
					}

					std::string valueRaw = gValueStr;
					g_free(gValueStr);
					gValueStr = NULL;

					std::string valueExpanded;
					if (valueRaw.empty()) {
						// TODO: Need a way to represent NULL strings as opposed to empty strings.
						valueRaw = " ";
						valueExpanded = " ";
					} else {
						valueExpanded = CStringUtils::trim(valueRaw);
						valueExpanded = CStringUtils::expandEnv(valueExpanded);
						for (TConstIterator<std::deque<SmartPtrSReplacement> > replacementIter(replacementCollection);
							replacementIter; replacementIter++ ) {
							const SmartPtrSReplacement replacement = *replacementIter;
							if (replacement->_regex->isMatched(valueExpanded)) {
								valueExpanded = replacement->_regex->replaceLiteral(valueExpanded, replacement->_value);
								break;
							}
						}

						if (groupName.compare("globals") == 0) {
							const SmartPtrSReplacement replacement = createReplacement(keyName, valueExpanded);
							replacementCollection.push_back(replacement);
						}
					}

					const SmartPtrSIniEntry iniEntry = createIniEntry(keyName, valueRaw, valueExpanded);
					iniSection->_entryCollection.push_back(iniEntry);
				}

				iniSectionCollection.push_back(iniSection);
			}
		} catch (GError *gErrorExc) {
			CAF_CM_EXCEPTION_VA0(gErrorExc->code, gErrorExc->message);
		}
	}
    CAF_CM_CATCH_CAF
    CAF_CM_CATCH_DEFAULT

	try {
		if (gKeyFile) {
			g_key_file_free(gKeyFile);
		}

		if (gError) {
			g_error_free(gError);
		}

		if (gGroupStrCollection) {
			g_strfreev(gGroupStrCollection);
		}

		if (gKeyStrCollection) {
			g_strfreev(gKeyStrCollection);
		}

		if (gValueStr) {
			g_free(gValueStr);
		}
	}
    CAF_CM_CATCH_DEFAULT
    CAF_CM_LOG_CRIT_CAFEXCEPTION;
    CAF_CM_THROWEXCEPTION;

	return iniSectionCollection;
}

CIniFile::SmartPtrSReplacement CIniFile::createReplacement(
	const std::string& keyName,
	const std::string& value) const {
	CAF_CM_FUNCNAME_VALIDATE("createReplacement");
	CAF_CM_VALIDATE_STRING(keyName);

	std::string pattern("\\$\\{");
	pattern += keyName;
	pattern += "\\}";

	SmartPtrCCafRegex regex;
	regex.CreateInstance();
	regex->initialize(pattern);

	SmartPtrSReplacement replacement;
	replacement.CreateInstance();
	replacement->_regex = regex;
	replacement->_value = value;

	return replacement;
}

CIniFile::SmartPtrSIniEntry CIniFile::createIniEntry(
	const std::string& keyName,
	const std::string& valueRaw,
	const std::string& valueExpanded) const {
	CAF_CM_FUNCNAME_VALIDATE("createIniEntry");
	CAF_CM_VALIDATE_STRING(keyName);
	CAF_CM_VALIDATE_STRING(valueRaw);
	CAF_CM_VALIDATE_STRING(valueExpanded);

	SmartPtrSIniEntry iniEntry;
	iniEntry.CreateInstance();
	iniEntry->_name = keyName;
	iniEntry->_valueRaw = valueRaw;
	iniEntry->_valueExpanded = valueExpanded;

	return iniEntry;
}
