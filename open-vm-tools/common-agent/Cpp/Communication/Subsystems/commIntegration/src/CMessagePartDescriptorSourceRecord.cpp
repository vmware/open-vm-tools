/*
 *  Created on: Nov 26, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "CMessagePartDescriptorSourceRecord.h"

using namespace Caf;

CMessagePartDescriptorSourceRecord::CMessagePartDescriptorSourceRecord() :
	_isInitialized(false),
	_attachmentNumber(0),
	_dataOffset(0),
	_dataLength(0),
	CAF_CM_INIT("CMessagePartDescriptorSourceRecord") {
}

CMessagePartDescriptorSourceRecord::~CMessagePartDescriptorSourceRecord() {
}

void CMessagePartDescriptorSourceRecord::initialize(
	const uint16 attachmentNumber,
	const std::string filePath,
   const uint32 dataOffset,
   const uint32 dataLength) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_attachmentNumber = attachmentNumber;
	_filePath = filePath;
	_dataOffset = dataOffset;
	_dataLength = dataLength;
	_isInitialized = true;
}

uint16 CMessagePartDescriptorSourceRecord::getAttachmentNumber() const {
	CAF_CM_FUNCNAME_VALIDATE("getAttachmentNumber");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _attachmentNumber;
}

std::string CMessagePartDescriptorSourceRecord::getFilePath() const {
	CAF_CM_FUNCNAME_VALIDATE("getFilePath");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _filePath;
}

uint32 CMessagePartDescriptorSourceRecord::getDataOffset() const {
	CAF_CM_FUNCNAME_VALIDATE("getDataOffset");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _dataOffset;
}

uint32 CMessagePartDescriptorSourceRecord::getDataLength() const {
	CAF_CM_FUNCNAME_VALIDATE("getDataLength");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _dataLength;
}
