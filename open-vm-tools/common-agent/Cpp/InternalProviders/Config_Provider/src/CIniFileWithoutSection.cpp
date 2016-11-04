/*
 *  Author: bwilliams
 *  Created: May 18, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/CCafRegex.h"
#include "Common/CIniFile.h"
#include "CIniFileWithoutSection.h"
#include "Exception/CCafException.h"

using namespace Caf;

CIniFileWithoutSection::CIniFileWithoutSection() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CIniFileWithoutSection") {
}

CIniFileWithoutSection::~CIniFileWithoutSection() {
}

void CIniFileWithoutSection::initialize(
	const std::string& configFilePath) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(configFilePath);

		_configFilePath = configFilePath;

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::deque<CIniFileWithoutSection::SmartPtrSIniEntry> CIniFileWithoutSection::getEntryCollection() {
	CAF_CM_FUNCNAME_VALIDATE("getEntryCollection");

	std::deque<SmartPtrSIniEntry> entryCollection;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		if (_entryCollection.empty()) {
			_entryCollection = parse(_configFilePath);
		}

		entryCollection = _entryCollection;
	}
	CAF_CM_EXIT;

	return entryCollection;
}

CIniFileWithoutSection::SmartPtrSIniEntry CIniFileWithoutSection::findOptionalEntry(
	const std::string& keyName) {
	CAF_CM_FUNCNAME_VALIDATE("findOptionalEntry");

	CIniFileWithoutSection::SmartPtrSIniEntry iniEntry;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(keyName);

		if (_entryCollection.empty()) {
			_entryCollection = parse(_configFilePath);
		}

		for (TConstIterator<std::deque<SmartPtrSIniEntry> > iniEntryIter(_entryCollection);
			iniEntryIter; iniEntryIter++) {
			const SmartPtrSIniEntry iniEntryTmp = *iniEntryIter;
			const std::string keyNameTmp = iniEntryTmp->_name;
			if (keyNameTmp.compare(keyName) == 0) {
				iniEntry = iniEntryTmp;
				break;
			}
		}
	}
	CAF_CM_EXIT;

	return iniEntry;
}

CIniFileWithoutSection::SmartPtrSIniEntry CIniFileWithoutSection::findRequiredEntry(
	const std::string& keyName) {
	CAF_CM_FUNCNAME("findRequiredEntry");

	SmartPtrSIniEntry iniEntry;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(keyName);

		iniEntry = findOptionalEntry(keyName);
		if (iniEntry.IsNull()) {
			CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_NOT_FOUND,
				"Value not found - keyName: %s", keyName.c_str());
		}
	}
	CAF_CM_EXIT;

	return iniEntry;
}

std::string CIniFileWithoutSection::findOptionalString(
	const std::string& keyName) {
	CAF_CM_FUNCNAME_VALIDATE("findOptionalString");

	std::string value;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(keyName);

		const SmartPtrSIniEntry iniEntry = findOptionalEntry(keyName);
		if (! iniEntry.IsNull()) {
			value = iniEntry->_valueExpanded;
		}
	}
	CAF_CM_EXIT;

	return value;
}

std::string CIniFileWithoutSection::findRequiredString(
	const std::string& keyName) {
	CAF_CM_FUNCNAME_VALIDATE("findRequiredString");

	std::string value;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(keyName);

		const SmartPtrSIniEntry iniEntry = findRequiredEntry(keyName);
		value = iniEntry->_valueExpanded;
	}
	CAF_CM_EXIT;

	return value;
}

std::string CIniFileWithoutSection::findOptionalRawString(
	const std::string& keyName) {
	CAF_CM_FUNCNAME_VALIDATE("findOptionalRawString");

	std::string value;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(keyName);

		const SmartPtrSIniEntry iniEntry = findOptionalEntry(keyName);
		if (! iniEntry.IsNull()) {
			value = iniEntry->_valueRaw;
		}
	}
	CAF_CM_EXIT;

	return value;
}

std::string CIniFileWithoutSection::findRequiredRawString(
	const std::string& keyName) {
	CAF_CM_FUNCNAME_VALIDATE("findRequiredRawString");

	std::string value;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(keyName);

		const SmartPtrSIniEntry iniEntry = findRequiredEntry(keyName);
		value = iniEntry->_valueRaw;
	}
	CAF_CM_EXIT;

	return value;
}

void CIniFileWithoutSection::log() {
	CAF_CM_FUNCNAME_VALIDATE("log");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		if (_entryCollection.empty()) {
			_entryCollection = parse(_configFilePath);
		}

		for (TConstIterator<std::deque<SmartPtrSIniEntry> > iniEntryIter(_entryCollection);
			iniEntryIter; iniEntryIter++) {
			const SmartPtrSIniEntry iniEntry = *iniEntryIter;
			CAF_CM_LOG_DEBUG_VA3("Entry - %s=%s (%s)",
				iniEntry->_name.c_str(), iniEntry->_valueRaw.c_str(), iniEntry->_valueExpanded.c_str());
		}
	}
	CAF_CM_EXIT;
}

void CIniFileWithoutSection::setValue(
	const std::string valueName,
	const std::string valueValue) {
	CAF_CM_FUNCNAME_VALIDATE("setValue");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(valueName);
		CAF_CM_VALIDATE_STRING(valueValue);

		const std::string srchValueName = valueName + "=";
		const std::string newFileLine = valueName + "=" + valueValue + "\n";

		bool isValueNameFnd = false;
		std::deque<std::string> newFileContents;
		const std::deque<std::string> fileContents = loadTextFileIntoCollection(_configFilePath);
		for(TConstIterator<std::deque<std::string> > fileLineIter(fileContents);
			fileLineIter; fileLineIter++) {
			const std::string fileLine = *fileLineIter;

			if (fileLine.find(srchValueName) == 0) {
				CAF_CM_LOG_DEBUG_VA2("Matched line... changing - valueName: %s, valueValue: %s",
					valueName.c_str(), valueValue.c_str());

				isValueNameFnd = true;
				newFileContents.push_back(newFileLine);
			} else {
				newFileContents.push_back(fileLine);
			}
		}

		if (! isValueNameFnd) {
			CAF_CM_LOG_WARN_VA1("Value name not found, adding... - %s", newFileLine.c_str());
			newFileContents.push_back(newFileLine);
		}

		_entryCollection.clear();
		saveTextFile(newFileContents, _configFilePath);
	}
	CAF_CM_EXIT;
}

void CIniFileWithoutSection::deleteValue(
	const std::string valueName) {
	CAF_CM_FUNCNAME_VALIDATE("deleteValue");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(valueName);

		const std::string srchValueName = valueName + "=";

		std::deque<std::string> newFileContents;
		const std::deque<std::string> FileContents = loadTextFileIntoCollection(_configFilePath);
		for(TConstIterator<std::deque<std::string> > fileLineIter(FileContents);
			fileLineIter; fileLineIter++) {
			const std::string fileLine = *fileLineIter;

			if (fileLine.find(srchValueName) == 0) {
				CAF_CM_LOG_DEBUG_VA2("Matched line... deleting - srchValueName: %s, line: %s",
					srchValueName.c_str(), fileLine.c_str());
			} else {
				newFileContents.push_back(fileLine);
			}
		}

		_entryCollection.clear();
		saveTextFile(newFileContents, _configFilePath);
	}
	CAF_CM_EXIT;
}

std::deque<CIniFileWithoutSection::SmartPtrSIniEntry> CIniFileWithoutSection::parse(
	const std::string& configFilePath) const {
	CAF_CM_FUNCNAME_VALIDATE("parse");

	std::deque<SmartPtrSIniEntry> entryCollection;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(configFilePath);

		std::deque<SmartPtrSReplacement> replacementCollection;

		SmartPtrCCafRegex goodLineRegex;
		goodLineRegex.CreateInstance();
		goodLineRegex->initialize("^[A-Za-z0-9]");

		SmartPtrCCafRegex newLineRegex;
		newLineRegex.CreateInstance();
		newLineRegex->initialize("\n");

		const std::deque<std::string> fileContents = loadTextFileIntoCollection(configFilePath);
		for(TConstIterator<std::deque<std::string> > fileLineIter(fileContents);
			fileLineIter; fileLineIter++) {
			const std::string fileLine = *fileLineIter;
			const std::string newFileLine = newLineRegex->replaceLiteral(fileLine, " ");

			if (goodLineRegex->isMatched(newFileLine)) {
				const Cdeqstr fileLineTokens = CStringUtils::split(newFileLine, '=');
				if (fileLineTokens.size() == 2) {
					const std::string keyName = fileLineTokens[0];
					const std::string valueRaw = fileLineTokens[1];

					std::string valueExpanded = CStringUtils::expandEnv(valueRaw);
					for (TConstIterator<std::deque<SmartPtrSReplacement> > replacementIter(replacementCollection);
						replacementIter; replacementIter++ ) {
						const SmartPtrSReplacement replacement = *replacementIter;
						if (replacement->_regex->isMatched(valueExpanded)) {
							valueExpanded = replacement->_regex->replaceLiteral(valueExpanded, replacement->_value);
							break;
						}
					}

					const SmartPtrSReplacement replacement = createReplacement(keyName, valueExpanded);
					replacementCollection.push_back(replacement);

					const SmartPtrSIniEntry iniEntry = createIniEntry(keyName, valueRaw, valueExpanded);
					entryCollection.push_back(iniEntry);
				} else {
					CAF_CM_LOG_WARN_VA2("Invalid line - file: %s, line: %s", configFilePath.c_str(), newFileLine.c_str());
				}
			}
		}
	}
	CAF_CM_EXIT;

	return entryCollection;
}

CIniFileWithoutSection::SmartPtrSReplacement CIniFileWithoutSection::createReplacement(
	const std::string& keyName,
	const std::string& value) const {
	CAF_CM_FUNCNAME_VALIDATE("createReplacement");

	SmartPtrSReplacement replacement;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(keyName);

		std::string pattern("\\$\\{");
		pattern += keyName;
		pattern += "\\}";

		SmartPtrCCafRegex regex;
		regex.CreateInstance();
		regex->initialize(pattern);

		replacement.CreateInstance();
		replacement->_regex = regex;
		replacement->_value = value;
	}
	CAF_CM_EXIT;

	return replacement;
}

CIniFileWithoutSection::SmartPtrSIniEntry CIniFileWithoutSection::createIniEntry(
	const std::string& keyName,
	const std::string& valueRaw,
	const std::string& valueExpanded) const {
	CAF_CM_FUNCNAME_VALIDATE("createIniEntry");

	SmartPtrSIniEntry iniEntry;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(keyName);
		CAF_CM_VALIDATE_STRING(valueRaw);
		CAF_CM_VALIDATE_STRING(valueExpanded);

		iniEntry.CreateInstance();
		iniEntry->_name = keyName;
		iniEntry->_valueRaw = valueRaw;
		iniEntry->_valueExpanded = valueExpanded;
	}
	CAF_CM_EXIT;

	return iniEntry;
}

std::deque<std::string> CIniFileWithoutSection::loadTextFileIntoCollection(
	const std::string& filePath) const {
	CAF_CM_FUNCNAME("loadTextFileIntoCollection");

	std::deque<std::string> rc;
	const uint32 maxLineLen = 512;

	FILE* fileHandle = NULL;

	try {
		CAF_CM_VALIDATE_STRING(filePath);

		if (! FileSystemUtils::doesFileExist(filePath)) {
			CAF_CM_EXCEPTIONEX_VA1(FileNotFoundException, ERROR_FILE_NOT_FOUND,
				"File not found - %s", filePath.c_str());
		}

#ifdef WIN32
		const errno_t fopenRc = ::fopen_s(&fileHandle, filePath.c_str(), "r" );
		if ((fopenRc != 0) || (fileHandle == NULL)) {
			CAF_CM_EXCEPTIONEX_VA1(InvalidHandleException, E_UNEXPECTED,
				"Failed to open file - %s", filePath.c_str());
		}
#else
		fileHandle = ::fopen(filePath.c_str(), "r" );
		if (fileHandle == NULL) {
			CAF_CM_EXCEPTIONEX_VA1(InvalidHandleException, E_UNEXPECTED,
				"Failed to open file - %s", filePath.c_str());
		}
#endif

		char buffer[maxLineLen];
		while(::fgets(buffer, maxLineLen, fileHandle) != NULL) {
			rc.push_back(buffer);
		}
	}
	CAF_CM_CATCH_ALL;

	try {
		if (fileHandle != NULL) {
			::fclose(fileHandle);
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_THROWEXCEPTION;

	return rc;
}

void CIniFileWithoutSection::saveTextFile(
	const std::deque<std::string> fileContents,
	const std::string filePath) const {
	CAF_CM_FUNCNAME("saveTextFile");

	FILE* fileHandle = NULL;

	try {
		CAF_CM_VALIDATE_STL(fileContents);
		CAF_CM_VALIDATE_STRING(filePath);

		if (! FileSystemUtils::doesFileExist(filePath)) {
			CAF_CM_EXCEPTIONEX_VA1(FileNotFoundException, ERROR_FILE_NOT_FOUND,
				"File not found - %s", filePath.c_str());
		}

#ifdef WIN32
		const errno_t fopenRc = ::fopen_s(&fileHandle, filePath.c_str(), "w" );
		if ((fopenRc != 0) || (fileHandle == NULL)) {
			CAF_CM_EXCEPTIONEX_VA1(InvalidHandleException, E_UNEXPECTED,
				"Failed to open file - %s", filePath.c_str());
		}
#else
		fileHandle = ::fopen(filePath.c_str(), "w" );
		if (fileHandle == NULL) {
			CAF_CM_EXCEPTIONEX_VA1(InvalidHandleException, E_UNEXPECTED,
				"Failed to open file - %s", filePath.c_str());
		}
#endif

		for(TConstIterator<std::deque<std::string> > fileLineIter(fileContents);
			fileLineIter; fileLineIter++) {
			const std::string fileLine = *fileLineIter;
			::fputs(fileLine.c_str(), fileHandle);
		}
	}
	CAF_CM_CATCH_ALL;

	try {
		if (fileHandle != NULL) {
			::fclose(fileHandle);
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_THROWEXCEPTION;
}
