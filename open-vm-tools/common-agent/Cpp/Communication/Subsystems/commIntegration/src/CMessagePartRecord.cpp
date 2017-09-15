/*
 *  Created on: Nov 26, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "CMessagePartRecord.h"

using namespace Caf;

CMessagePartRecord::CMessagePartRecord() :
	_isInitialized(false),
	_attachmentNumber(0),
	_dataOffset(0),
	_dataLength(0),
	CAF_CM_INIT("CMessagePartRecord") {
}

CMessagePartRecord::~CMessagePartRecord() {
}

void CMessagePartRecord::initialize(
	const uint16 attachmentNumber,
	const std::string filePath,
   const uint64 dataOffset,
   const uint64 dataLength) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

   _attachmentNumber = attachmentNumber;
   _filePath = filePath;
   _dataOffset = dataOffset;
   _dataLength = dataLength;
	_isInitialized = true;
}

uint16 CMessagePartRecord::getAttachmentNumber() const {
	CAF_CM_FUNCNAME_VALIDATE("getAttachmentNumber");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _attachmentNumber;
}

std::string CMessagePartRecord::getFilePath() const {
	CAF_CM_FUNCNAME_VALIDATE("getFilePath");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _filePath;
}

uint64 CMessagePartRecord::getDataOffset() const {
	CAF_CM_FUNCNAME_VALIDATE("getDataOffset");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _dataOffset;
}

uint64 CMessagePartRecord::getDataLength() const {
	CAF_CM_FUNCNAME_VALIDATE("getDataLength");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _dataLength;
}

void CMessagePartRecord::setAttachmentNumber(const uint16 attachmentNumber) {
	CAF_CM_FUNCNAME_VALIDATE("setAttachmentNumber");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	_attachmentNumber = attachmentNumber;
}

void CMessagePartRecord::setFilePath(const std::string& filePath) {
	CAF_CM_FUNCNAME_VALIDATE("setFilePath");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	_filePath = filePath;
}

void CMessagePartRecord::setDataOffset(const uint64 dataOffset) {
	CAF_CM_FUNCNAME_VALIDATE("setDataOffset");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	_dataOffset = dataOffset;
}

void CMessagePartRecord::setDataLength(const uint64 dataLength) {
	CAF_CM_FUNCNAME_VALIDATE("setDataLength");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	_dataLength = dataLength;
}
