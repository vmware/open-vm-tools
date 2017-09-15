/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CFileReadingMessageSource_h_
#define CFileReadingMessageSource_h_

#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/Core/CAbstractPollableChannel.h"

namespace Caf {

class CFileReadingMessageSource :
	public CAbstractPollableChannel
{
private:
	typedef std::map<std::string, bool> CFileCollection;
	CAF_DECLARE_SMART_POINTER(CFileCollection);

public:
	CFileReadingMessageSource();
	virtual ~CFileReadingMessageSource();

public:
	void initialize(
		const SmartPtrIDocument& configSection);

protected: // CAbstractPollableChannel
	bool doSend(
			const SmartPtrIIntMessage& message,
			int32 timeout);

	SmartPtrIIntMessage doReceive(const int32 timeout);

private:
	SmartPtrCFileCollection itemsInDirectory(
		const std::string& directory,
		const std::string& filenameRegex) const;

	SmartPtrCFileCollection merge(
		const SmartPtrCFileCollection& newFileCollection,
		const SmartPtrCFileCollection& existingFileCollection) const;

	std::string calcNextFile(
		SmartPtrCFileCollection& fileCollection) const;

	uint64 getTimeSec() const;

	bool isRefreshNecessary(
		const uint32 refreshSec,
		const uint64 lastRefreshSec) const;

private:
	bool _isInitialized;
	std::string _id;
	std::string _directory;
	std::string _filenameRegex;
	bool _preventDuplicates;
	uint32 _refreshSec;
	uint64 _lastRefreshSec;

	SmartPtrCFileCollection _fileCollection;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CFileReadingMessageSource);
};

CAF_DECLARE_SMART_POINTER(CFileReadingMessageSource);

}

#endif // #ifndef CFileReadingMessageSource_h_
