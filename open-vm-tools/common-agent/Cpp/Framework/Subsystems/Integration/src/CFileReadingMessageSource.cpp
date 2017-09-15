/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/Core/CIntMessage.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "CFileReadingMessageSource.h"
#include "Exception/CCafException.h"

using namespace Caf;

CFileReadingMessageSource::CFileReadingMessageSource() :
	_isInitialized(false),
	_preventDuplicates(true),
	_refreshSec(0),
	_lastRefreshSec(0),
	CAF_CM_INIT_LOG("CFileReadingMessageSource") {
}

CFileReadingMessageSource::~CFileReadingMessageSource() {
}

void CFileReadingMessageSource::initialize(
	const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	_id = configSection->findRequiredAttribute("id");
	const std::string directoryStr = configSection->findRequiredAttribute("directory");
	const std::string filenameRegexStr = configSection->findOptionalAttribute("filename-regex");
	const std::string preventDuplicatesStr = configSection->findOptionalAttribute("prevent-duplicates");
	const std::string autoCreateDirectoryStr = configSection->findOptionalAttribute("auto-create-directory");
	const SmartPtrIDocument pollerDoc = configSection->findOptionalChild("poller");

	_refreshSec = 0;

	_directory = CStringUtils::expandEnv(directoryStr);
	setPollerMetadata(pollerDoc);
	_preventDuplicates =
		(preventDuplicatesStr.empty() || preventDuplicatesStr.compare("true") == 0) ? true : false;

	if (filenameRegexStr.empty()) {
		_filenameRegex = FileSystemUtils::REGEX_MATCH_ALL;
	} else {
		_filenameRegex = filenameRegexStr;
	}

	const bool autoCreateDirectory =
		(autoCreateDirectoryStr.empty() || autoCreateDirectoryStr.compare("true") == 0) ? true : false;
	if (autoCreateDirectory && ! FileSystemUtils::doesDirectoryExist(_directory)) {
		FileSystemUtils::createDirectory(_directory);
	}

	CAF_CM_LOG_DEBUG_VA2(
			"Monitoring inbound directory - dir: %s, fileRegex: %s",
			_directory.c_str(), _filenameRegex.c_str());

	_fileCollection.CreateInstance();
	_isInitialized = true;
}

bool CFileReadingMessageSource::doSend(
		const SmartPtrIIntMessage&,
		int32) {
	CAF_CM_FUNCNAME("doSend");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CAF_CM_EXCEPTIONEX_VA1(
			UnsupportedOperationException,
			E_NOTIMPL,
			"This is not a sending channel: %s", _id.c_str());
}

SmartPtrIIntMessage CFileReadingMessageSource::doReceive(const int32 timeout) {
	CAF_CM_FUNCNAME("receive");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	if (timeout > 0) {
		CAF_CM_EXCEPTIONEX_VA1(UnsupportedOperationException, E_INVALIDARG,
			"Timeout not currently supported: %s", _id.c_str());
	}

	if (isRefreshNecessary(_refreshSec, _lastRefreshSec)) {
		const SmartPtrCFileCollection newFileCollection =
			itemsInDirectory(_directory, _filenameRegex);

		if (_preventDuplicates) {
			_fileCollection = merge(newFileCollection, _fileCollection);
		} else {
			_fileCollection = newFileCollection;
		}

		_lastRefreshSec = getTimeSec();
	}

	SmartPtrIIntMessage message;
	const std::string filename = calcNextFile(_fileCollection);
	if (! filename.empty()) {
		CAF_CM_LOG_DEBUG_VA1("Creating message with filename - %s", filename.c_str());

		SmartPtrCIntMessage messageImpl;
		messageImpl.CreateInstance();
		messageImpl->initializeStr(filename, IIntMessage::SmartPtrCHeaders(), IIntMessage::SmartPtrCHeaders());
		message = messageImpl;
	}

	return message;
}

CFileReadingMessageSource::SmartPtrCFileCollection
CFileReadingMessageSource::itemsInDirectory(
	const std::string& directory,
	const std::string& filenameRegex) const {
	CAF_CM_FUNCNAME_VALIDATE("itemsInDirectory");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(directory);
	// filenameRegex defaults to FileSystemUtils::REGEX_MATCH_ALL

	SmartPtrCFileCollection rc;
	rc.CreateInstance();

	const FileSystemUtils::DirectoryItems directoryItems =
		FileSystemUtils::itemsInDirectory(directory, filenameRegex);
	const FileSystemUtils::Files files = directoryItems.files;
	for (TConstIterator<FileSystemUtils::Files> fileIter(files); fileIter; fileIter++) {
		const std::string filename = *fileIter;
		const std::string filePath = FileSystemUtils::buildPath(
			directory, filename);

		rc->insert(std::make_pair(filePath, false));
	}

	return rc;
}

CFileReadingMessageSource::SmartPtrCFileCollection
CFileReadingMessageSource::merge(
	const SmartPtrCFileCollection& newFileCollection,
	const SmartPtrCFileCollection& existingFileCollection) const {
	CAF_CM_FUNCNAME_VALIDATE("merge");
	CAF_CM_VALIDATE_SMARTPTR(newFileCollection);
	CAF_CM_VALIDATE_SMARTPTR(existingFileCollection);

	SmartPtrCFileCollection newFileCollectionRc = newFileCollection;
	for (TMapIterator<CFileCollection> newFileIter(*newFileCollectionRc);
		newFileIter; newFileIter++) {
		const std::string newFile = newFileIter.getKey();

		CFileCollection::const_iterator existingFileIter =
			existingFileCollection->find(newFile);
		if (existingFileIter != existingFileCollection->end()) {
			*newFileIter = existingFileIter->second;
		}
	}

	return newFileCollectionRc;
}

std::string CFileReadingMessageSource::calcNextFile(
	SmartPtrCFileCollection& fileCollection) const {
	CAF_CM_FUNCNAME_VALIDATE("calcNextFile");
	CAF_CM_VALIDATE_SMARTPTR(fileCollection);

	std::string filename;
	for (TMapIterator<CFileCollection> fileIter(*fileCollection);
		filename.empty() && fileIter; fileIter++) {
		const bool isFileReceived = *fileIter;
		if (! isFileReceived) {
			filename = fileIter.getKey();
			*fileIter = true;
		}
	}

	return filename;
}

bool CFileReadingMessageSource::isRefreshNecessary(
	const uint32 refreshSec,
	const uint64 lastRefreshSec) const {
	bool rc = false;

	if (refreshSec == 0) {
		rc = true;
	} else {
		const uint64 currentTimeSec = getTimeSec();
		if ((currentTimeSec - lastRefreshSec) > refreshSec) {
			rc = true;
		}
	}

	return rc;
}

uint64 CFileReadingMessageSource::getTimeSec() const {
	GTimeVal curTime;
	::g_get_current_time(&curTime);
	uint64 rc = curTime.tv_sec;

	return rc;
}
