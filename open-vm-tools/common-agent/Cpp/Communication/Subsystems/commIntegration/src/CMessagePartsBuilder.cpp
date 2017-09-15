/*  Created on: Nov 19, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "CMessagePartsBuilder.h"

using namespace Caf;

void CMessagePartsBuilder::put(
	const byte value,
	SmartPtrCDynamicByteArray& buffer) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CMessagePartsBuilder", "put(byte)");
	CAF_CM_VALIDATE_SMARTPTR(buffer);

//	const size_t currentPos = buffer->getByteCount() - buffer->getByteCountFromCurrentPos();

	buffer->memAppend(&value, sizeof(value));

//	CAF_CM_LOG_DEBUG_VA2("buffer - pos: %d, val: %02x", currentPos, value);
}

void CMessagePartsBuilder::put(
	const uint16 value,
	SmartPtrCDynamicByteArray& buffer) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CMessagePartsBuilder", "put(uint16)");
	CAF_CM_VALIDATE_SMARTPTR(buffer);

//	const size_t currentPos = buffer->getByteCount() - buffer->getByteCountFromCurrentPos();

	byte buf[2];
	buf[0] = value >> 8;
	buf[1] = value >> 0;
	putBytes(buf, 2, buffer);

//	CAF_CM_LOG_DEBUG_VA4("buffer - pos: %d, val: %04x, buf: %02x %02x", currentPos, value, buf[0], buf[1]);
}

void CMessagePartsBuilder::put(
	const uint32 value,
	SmartPtrCDynamicByteArray& buffer) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CMessagePartsBuilder", "put(uint32)");
	CAF_CM_VALIDATE_SMARTPTR(buffer);

//	const size_t currentPos = buffer->getByteCount() - buffer->getByteCountFromCurrentPos();

	byte buf[4];
	buf[0] = value >> 24;
	buf[1] = value >> 16;
	buf[2] = value >> 8;
	buf[3] = value >> 0;
	putBytes(buf, 4, buffer);

//	CAF_CM_LOG_DEBUG_VA6("buffer - pos: %d, val: %08x, buf: %02x %02x %02x %02x", currentPos, value, buf[0], buf[1], buf[2], buf[3]);
}

void CMessagePartsBuilder::put(
	const uint64 value,
	SmartPtrCDynamicByteArray& buffer) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CMessagePartsBuilder", "put(uint64)");
	CAF_CM_VALIDATE_SMARTPTR(buffer);

//	const size_t currentPos = buffer->getByteCount() - buffer->getByteCountFromCurrentPos();

	byte buf[8];
#ifdef WIN32
#pragma warning(push)
#pragma warning(disable: 4244)
#endif
	buf[0] = value >> 56;
	buf[1] = value >> 48;
	buf[2] = value >> 40;
	buf[3] = value >> 32;
	buf[4] = value >> 24;
	buf[5] = value >> 16;
	buf[6] = value >> 8;
	buf[7] = value >> 0;
#ifdef WIN32
#pragma warning(pop)
#endif
	putBytes(buf, 8, buffer);

//	CAF_CM_LOG_DEBUG_VA6("buffer - pos: %d, val: %016x, buf: %02x %02x %02x %02x", currentPos, value, buf[0], buf[1], buf[2], buf[3]);
//	CAF_CM_LOG_DEBUG_VA6("buffer - pos: %d, val: %016x, buf: %02x %02x %02x %02x", currentPos, value, buf[4], buf[5], buf[6], buf[7]);
}

void CMessagePartsBuilder::put(
	const UUID value,
	SmartPtrCDynamicByteArray& buffer) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CMessagePartsBuilder", "put(UUID)");
	CAF_CM_VALIDATE_SMARTPTR(buffer);

	put(static_cast<uint32>(value.Data1), buffer);
	put(static_cast<uint16>(value.Data2), buffer);
	put(static_cast<uint16>(value.Data3), buffer);
	putBytes(value.Data4, 8, buffer);
}

void CMessagePartsBuilder::putBytes(
	const byte* buf,
	const uint32 bufLen,
	SmartPtrCDynamicByteArray& buffer) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CMessagePartsBuilder", "putBytes");
	CAF_CM_VALIDATE_PTR(buf);
	CAF_CM_VALIDATE_SMARTPTR(buffer);

	for (uint32 index = 0; index < bufLen; index++) {
		buffer->memAppend(&buf[index], sizeof(buf[index]));
	}
}
