/*  Created on: Nov 19, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "CMessagePartsParser.h"

using namespace Caf;

byte CMessagePartsParser::getByte(SmartPtrCDynamicByteArray& buffer) {
	CAF_CM_STATIC_FUNC_VALIDATE("CMessagePartsParser", "getByte");
	CAF_CM_VALIDATE_SMARTPTR(buffer);

//	const size_t currentPos = buffer->getByteCount() - buffer->getByteCountFromCurrentPos();

	const byte rc = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);

//	CAF_CM_LOG_DEBUG_VA2("buffer - pos: %d, val: %02x", currentPos, rc);

	return rc;
}

uint16 CMessagePartsParser::getUint16(SmartPtrCDynamicByteArray& buffer) {
	CAF_CM_STATIC_FUNC_VALIDATE("CMessagePartsParser", "getUint16");
	CAF_CM_VALIDATE_SMARTPTR(buffer);

//	const size_t currentPos = buffer->getByteCount() - buffer->getByteCountFromCurrentPos();

	byte buf[2];
	buf[0] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[1] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);

	// Little and Big Endian
	//const uint16 val = (buf[0]<<0) | (buf[1]<<8);
	const uint16 val = (buf[0]<<8) | (buf[1]<<0);

//	CAF_CM_LOG_DEBUG_VA4("buffer - pos: %d, val: %04x, buf: %02x %02x", currentPos, val, buf[0], buf[1]);

	return val;
}

uint32 CMessagePartsParser::getUint32(SmartPtrCDynamicByteArray& buffer) {
	CAF_CM_STATIC_FUNC_VALIDATE("CMessagePartsParser", "getUint32");
	CAF_CM_VALIDATE_SMARTPTR(buffer);

//	const size_t currentPos = buffer->getByteCount() - buffer->getByteCountFromCurrentPos();

	byte buf[4];
	buf[0] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[1] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[2] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[3] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);

	// Little and Big Endian
	//const uint32 val = (buf[0]<<0) | (buf[1]<<8) | (buf[2]<<16) | (buf[3]<<24);
	const uint32 val = (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | (buf[3]<<0);

//	CAF_CM_LOG_DEBUG_VA6("buffer - pos: %d, val: %08x, buf: %02x %02x %02x %02x", currentPos, val, buf[0], buf[1], buf[2], buf[3]);

	return val;
}

uint64 CMessagePartsParser::getUint64(SmartPtrCDynamicByteArray& buffer) {
	CAF_CM_STATIC_FUNC_VALIDATE("CMessagePartsParser", "getUint64");
	CAF_CM_VALIDATE_SMARTPTR(buffer);

//	const size_t currentPos = buffer->getByteCount() - buffer->getByteCountFromCurrentPos();

	byte buf[8];
	buf[0] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[1] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[2] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[3] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[4] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[5] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[6] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[7] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);

	// Little and Big Endian
//	const uint64 val =
//		(buf[0]<<0)
//		| (buf[1]<<8)
//		| (buf[2]<<16)
//		| (buf[3]<<24)
//		| (static_cast<uint64>(buf[4])<<32)
//		| (static_cast<uint64>(buf[5])<<40)
//		| (static_cast<uint64>(buf[6])<<48)
//		| (static_cast<uint64>(buf[7])<<56);
	const uint64 val =
		(static_cast<uint64>(buf[0])<<56)
		| (static_cast<uint64>(buf[1])<<48)
		| (static_cast<uint64>(buf[2])<<40)
		| (static_cast<uint64>(buf[3])<<32)
		| (buf[4]<<24)
		| (buf[5]<<16)
		| (buf[6]<<8)
		| (buf[7]<<0);

//	CAF_CM_LOG_DEBUG_VA6("buffer - pos: %d, val: %016x, buf: %02x %02x %02x %02x", currentPos, val, buf[0], buf[1], buf[2], buf[3]);
//	CAF_CM_LOG_DEBUG_VA6("buffer - pos: %d, val: %016x, buf: %02x %02x %02x %02x", currentPos, val, buf[4], buf[5], buf[6], buf[7]);

	return val;
}

UUID CMessagePartsParser::getGuid(SmartPtrCDynamicByteArray& buffer) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CMessagePartsParser", "getGuid");
	CAF_CM_VALIDATE_SMARTPTR(buffer);

	UUID guid;
	guid.Data1 = getUint32(buffer);
	guid.Data2 = getUint16(buffer);
	guid.Data3 = getUint16(buffer);
	get8Bytes(buffer, guid.Data4);

	CAF_CM_LOG_DEBUG_VA1("guid - %s", BasePlatform::UuidToString(guid).c_str());

	return guid;
}

void CMessagePartsParser::get8Bytes(SmartPtrCDynamicByteArray& buffer, byte* buf) {
	CAF_CM_STATIC_FUNC_VALIDATE("CMessagePartsParser", "get8Bytes");
	CAF_CM_VALIDATE_SMARTPTR(buffer);

//	const size_t currentPos = buffer->getByteCount() - buffer->getByteCountFromCurrentPos();

	buf[0] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[1] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[2] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[3] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[4] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[5] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[6] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);
	buf[7] = buffer->getAtCurrentPos();
	buffer->incrementCurrentPos(1);

//	CAF_CM_LOG_DEBUG_VA5("buffer - pos: %d, buf: %02x %02x %02x %02x", currentPos, buf[0], buf[1], buf[2], buf[3]);
//	CAF_CM_LOG_DEBUG_VA5("buffer - pos: %d, buf: %02x %02x %02x %02x", currentPos, buf[4], buf[5], buf[6], buf[7]);
}
