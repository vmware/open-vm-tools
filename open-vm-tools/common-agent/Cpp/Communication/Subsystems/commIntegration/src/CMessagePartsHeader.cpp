/*  Created on: Nov 19, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "CMessagePartsHeader.h"
#include "Exception/CCafException.h"

using namespace Caf;

/**
 * Converts the BLOCK_SIZE data in a ByteBuffer into a MessagePartsHeader
 * <p>
 * The incoming ByteBuffer position will be modified.
 * @param buffer ByteBuffer to convert
 * @return a MessagePartsHeader
 */
SmartPtrCMessagePartsHeader CMessagePartsHeader::fromByteBuffer(
	SmartPtrCDynamicByteArray& buffer) {
	CAF_CM_STATIC_FUNC_LOG("CMessagePartsHeader", "fromByteBuffer");
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
 * Converts a byte array into a MessagePartsHeader
 * @param blockData byte array to convert
 * @return a MessagePartsHeader
 */
SmartPtrCMessagePartsHeader CMessagePartsHeader::fromArray(
	SmartPtrCDynamicByteArray& buffer) {
	CAF_CM_STATIC_FUNC_LOG("CMessagePartsHeader", "fromArray");
	CAF_CM_VALIDATE_SMARTPTR(buffer);

	if (buffer->getByteCount() < BLOCK_SIZE) {
		CAF_CM_EXCEPTION_VA1(E_INVALIDARG, "Input data block is too small - %d",
			buffer->getByteCount());
	}

	const int32 version = CMessagePartsParser::getByte(buffer);
	if (version != CAF_MSG_VERSION) {
		CAF_CM_EXCEPTION_VA2(E_INVALIDARG, "Input data block version is incorrect: %d != %d", version, CAF_MSG_VERSION);
	}

	const byte resvd1 = CMessagePartsParser::getByte(buffer);
	const byte resvd2 = CMessagePartsParser::getByte(buffer);
	const byte resvd3 = CMessagePartsParser::getByte(buffer);
	if (resvd1 != RESERVED1 || resvd2 != RESERVED2 || resvd3 != RESERVED3) {
		CAF_CM_EXCEPTION_VA0(E_INVALIDARG, "Input data block reserved bits are incorrect");
	}

	const UUID correlationId = CMessagePartsParser::getGuid(buffer);
	const uint32 numberOfParts = CMessagePartsParser::getUint32(buffer);
	buffer->verify();

	SmartPtrCMessagePartsHeader messagePartsHeader;
	messagePartsHeader.CreateInstance();
	messagePartsHeader->initialize(correlationId, numberOfParts);

	return messagePartsHeader;
}

/**
 * Create a byte array representation of a descriptor block
 * @param correlationId the correlation id
 * @param numberOfParts the number of message parts
 * @return the byte array containing the encoded data
 */
SmartPtrCDynamicByteArray CMessagePartsHeader::toArray(
	const UUID correlationId,
	const uint32 numberOfParts) {
	SmartPtrCDynamicByteArray buffer;
	buffer.CreateInstance();
	buffer->allocateBytes(BLOCK_SIZE);

	CMessagePartsBuilder::put(CAF_MSG_VERSION, buffer);
	CMessagePartsBuilder::put(RESERVED1, buffer);
	CMessagePartsBuilder::put(RESERVED2, buffer);
	CMessagePartsBuilder::put(RESERVED3, buffer);
	CMessagePartsBuilder::put(correlationId, buffer);
	CMessagePartsBuilder::put(numberOfParts, buffer);
	buffer->verify();

	return buffer;
}

CMessagePartsHeader::CMessagePartsHeader() :
	_isInitialized(false),
	_correlationId(CAFCOMMON_GUID_NULL),
	_numberOfParts(0),
	CAF_CM_INIT("CMessagePartsHeader") {
}

CMessagePartsHeader::~CMessagePartsHeader() {
}

void CMessagePartsHeader::initialize(
	const UUID correlationId,
	const uint32 numberOfParts) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	this->_correlationId = correlationId;
	this->_numberOfParts = numberOfParts;
	_isInitialized = true;
}

/**
 * @return the correlationId
 */
UUID CMessagePartsHeader::getCorrelationId() const {
	CAF_CM_FUNCNAME_VALIDATE("getCorrelationId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _correlationId;
}

/**
 * @return the correlationId
 */
std::string CMessagePartsHeader::getCorrelationIdStr() const {
	CAF_CM_FUNCNAME_VALIDATE("getCorrelationId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return BasePlatform::UuidToString(_correlationId);
}

/**
 * @return the numberOfParts
 */
uint32 CMessagePartsHeader::getNumberOfParts() const {
	CAF_CM_FUNCNAME_VALIDATE("getNumberOfParts");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _numberOfParts;
}
