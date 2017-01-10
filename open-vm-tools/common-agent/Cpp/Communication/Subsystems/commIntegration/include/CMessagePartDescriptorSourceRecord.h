/*
 *  Created on: Nov 26, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CMessagePartDescriptorSourceRecord_h
#define CMessagePartDescriptorSourceRecord_h

namespace Caf {

class CMessagePartDescriptorSourceRecord {
public:
	CMessagePartDescriptorSourceRecord();
	virtual ~CMessagePartDescriptorSourceRecord();

public:
	void initialize(
		const uint16 attachmentNumber,
		const std::string filePath,
      const uint32 dataOffset,
      const uint32 dataLength);

public:
	uint16 getAttachmentNumber() const;

	std::string getFilePath() const;

	uint32 getDataOffset() const;

	uint32 getDataLength() const;

private:
	bool _isInitialized;
	uint16 _attachmentNumber;
	std::string _filePath;
	uint32 _dataOffset;
	uint32 _dataLength;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CMessagePartDescriptorSourceRecord);
};

CAF_DECLARE_SMART_POINTER(CMessagePartDescriptorSourceRecord);

}

#endif /* CMessagePartDescriptorSourceRecord_h */
