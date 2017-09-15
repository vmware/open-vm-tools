/*  Created on: Nov 19, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "CMessagePartDescriptor.h"
#include "Exception/CCafException.h"

using namespace Caf;

/**
 * Converts the BLOCK_SIZE data in a ByteBuffer into a MessagePartsDescriptor
 * <p>
 * The incoming ByteBuffer position will be modified.
 * @param buffer ByteBuffer to convert
 * @return a MessagePartsDescriptor
 */
SmartPtrCMessagePartDescriptor CMessagePartDescriptor::fromByteBuffer(
	SmartPtrCDynamicByteArray& buffer) {
	CAF_CM_STATIC_FUNC("CMessagePartDescriptor", "fromByteBuffer");
	CAF_CM_VALIDATE_SMARTPTR(buffer);

	if (buffer->getByteCountFromCurrentPos() < BLOCK_SIZE) {
		CAF_CM_EXCEPTION_VA2(E_INVALIDARG,
			"Input data block is too small - rem: %d, tot: %d",
			buffer->getByteCountFromCurrentPos(), buffer->getByteCount());
	}

	SmartPtrCDynamicByteArray data;
	data.CreateInstance();
	data->allocateBytes(BLOCK_SIZE);
	data->memCpy(buffer->getPtrAtCurrentPos(), BLOCK_SIZE);

	buffer->incrementCurrentPos(BLOCK_SIZE);
	return fromArray(data);
}

/**
 * Converts a byte array into a MessagePartsDescriptor
 * @param blockData byte array to convert
 * @return a MessagePartsDescriptor
 */
SmartPtrCMessagePartDescriptor CMessagePartDescriptor::fromArray(
	SmartPtrCDynamicByteArray& buffer) {
	CAF_CM_STATIC_FUNC("CMessagePartDescriptor", "fromArray");
	CAF_CM_VALIDATE_SMARTPTR(buffer);

	if (buffer->getByteCount() < BLOCK_SIZE) {
		CAF_CM_EXCEPTION_VA1(E_INVALIDARG, "Input data block is too small - %d",
			buffer->getByteCount());
	}

	if (CMessagePartsParser::getByte(buffer) != CAF_MSG_VERSION) {
		CAF_CM_EXCEPTION_VA0(E_INVALIDARG, "Input data block version is incorrect");
	}

	const byte resvd = CMessagePartsParser::getByte(buffer);
	if (resvd != RESERVED) {
		CAF_CM_EXCEPTION_VA0(E_INVALIDARG, "Input data block reserved bits are incorrect");
	}

	const uint16 attachmentNumber = CMessagePartsParser::getUint16(buffer);
	const uint32 partNumber = CMessagePartsParser::getUint32(buffer);
	const uint32 dataSize = CMessagePartsParser::getUint32(buffer);
	const uint32 dataOffset = CMessagePartsParser::getUint32(buffer);
	buffer->verify();

	SmartPtrCMessagePartDescriptor messagePartsDescriptor;
	messagePartsDescriptor.CreateInstance();
	messagePartsDescriptor->initialize(
		attachmentNumber, partNumber, dataSize, dataOffset);

	return messagePartsDescriptor;
}

SmartPtrCDynamicByteArray CMessagePartDescriptor::toArray(
		const uint16 attachmentNumber,
		const uint32 partNumber,
		const uint32 dataSize,
		const uint32 dataOffset) {
	SmartPtrCDynamicByteArray buffer;
	buffer.CreateInstance();
	buffer->allocateBytes(BLOCK_SIZE);

	CMessagePartsBuilder::put(CAF_MSG_VERSION, buffer);
	CMessagePartsBuilder::put(RESERVED, buffer);
	CMessagePartsBuilder::put(attachmentNumber, buffer);
	CMessagePartsBuilder::put(partNumber, buffer);
	CMessagePartsBuilder::put(dataSize, buffer);
	CMessagePartsBuilder::put(dataOffset, buffer);
	buffer->verify();

	return buffer;
}

CMessagePartDescriptor::CMessagePartDescriptor() :
	_isInitialized(false),
	_attachmentNumber(0),
	_partNumber(0),
	_dataSize(0),
	_dataOffset(0),
	CAF_CM_INIT("CMessagePartDescriptor") {
}

CMessagePartDescriptor::~CMessagePartDescriptor() {
}

void CMessagePartDescriptor::initialize(
	const uint16 attachmentNumber,
	const uint32 partNumber,
	const uint32 dataSize,
	const uint32 dataOffset) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	this->_attachmentNumber = attachmentNumber;
	this->_partNumber = partNumber;
	this->_dataSize = dataSize;
	this->_dataOffset = dataOffset;
	_isInitialized = true;
}

/**
 * @return the attachmentNumber
 */
uint16 CMessagePartDescriptor::getAttachmentNumber() const {
	CAF_CM_FUNCNAME_VALIDATE("getAttachmentNumber");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _attachmentNumber;
}

std::string CMessagePartDescriptor::getAttachmentNumberStr() const {
	CAF_CM_FUNCNAME_VALIDATE("getAttachmentNumber");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return CStringConv::toString<uint16>(_attachmentNumber);
}

/**
 * @return the partNumber
 */
uint32 CMessagePartDescriptor::getPartNumber() const {
	CAF_CM_FUNCNAME_VALIDATE("getPartNumber");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _partNumber;
}

/**
 * @return the dataSize
 */
uint32 CMessagePartDescriptor::getDataSize() const {
	CAF_CM_FUNCNAME_VALIDATE("getDataSize");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _dataSize;
}

/**
 * @return the dataOffset
 */
uint32 CMessagePartDescriptor::getDataOffset() const {
	CAF_CM_FUNCNAME_VALIDATE("getDataOffset");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _dataOffset;
}
