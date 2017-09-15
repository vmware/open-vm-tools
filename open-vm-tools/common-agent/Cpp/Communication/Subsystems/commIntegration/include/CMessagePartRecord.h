/*
 *  Created on: Nov 26, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CMessagePartRecord_h
#define CMessagePartRecord_h

namespace Caf {

class CMessagePartRecord {
public:
	CMessagePartRecord();
	virtual ~CMessagePartRecord();

public:
	void initialize(
		const uint16 attachmentNumber,
		const std::string filePath,
      const uint64 dataOffset,
      const uint64 dataLength);

public:
	uint16 getAttachmentNumber() const;

	std::string getFilePath() const;

	uint64 getDataOffset() const;

	uint64 getDataLength() const;

	void setAttachmentNumber(const uint16 attachmentNumber);

	void setFilePath(const std::string& filePath);

	void setDataOffset(const uint64 dataOffset);

	void setDataLength(const uint64 dataLength);

private:
	bool _isInitialized;
	uint16 _attachmentNumber;
	std::string _filePath;
   uint64 _dataOffset;
   uint64 _dataLength;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CMessagePartRecord);
};

CAF_DECLARE_SMART_POINTER(CMessagePartRecord);

}

#endif /* CMessagePartRecord_h */
