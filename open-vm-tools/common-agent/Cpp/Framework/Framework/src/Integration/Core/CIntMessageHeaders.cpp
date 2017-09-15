/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "ICafObject.h"
#include "IVariant.h"
#include "Integration/IIntMessage.h"
#include "Integration/Core/CIntMessageHeaders.h"

using namespace Caf;

CIntMessageHeaders::CIntMessageHeaders() :
	CAF_CM_INIT("CIntMessageHeaders") {
	_headers.CreateInstance();
}

CIntMessageHeaders::~CIntMessageHeaders() {
}

IIntMessage::SmartPtrCHeaders CIntMessageHeaders::getHeaders() const {
	return _headers;
}

void CIntMessageHeaders::clear() {
	_headers.CreateInstance();
}

void CIntMessageHeaders::insertString(
	const std::string& key,
	const std::string& value) {
	CAF_CM_FUNCNAME_VALIDATE("insertString");
	CAF_CM_VALIDATE_STRING(key);
	CAF_CM_VALIDATE_STRING(value);
	_headers->insert(std::make_pair(key, std::make_pair(CVariant::createString(value), SmartPtrICafObject())));
}

void CIntMessageHeaders::insertStringOpt(
	const std::string& key,
	const std::string& value) {
	CAF_CM_FUNCNAME_VALIDATE("insertStringOpt");
	CAF_CM_VALIDATE_STRING(key);
	_headers->insert(std::make_pair(key, std::make_pair(CVariant::createString(value), SmartPtrICafObject())));
}

void CIntMessageHeaders::insertInt64(
	const std::string& key,
	const int64& value) {
	CAF_CM_FUNCNAME_VALIDATE("insertInt64");
	CAF_CM_VALIDATE_STRING(key);
	_headers->insert(std::make_pair(key, std::make_pair(CVariant::createInt64(value), SmartPtrICafObject())));
}

void CIntMessageHeaders::insertUint64(
	const std::string& key,
	const uint64& value) {
	CAF_CM_FUNCNAME_VALIDATE("insertUint64");
	CAF_CM_VALIDATE_STRING(key);
	_headers->insert(std::make_pair(key, std::make_pair(CVariant::createUint64(value), SmartPtrICafObject())));
}

void CIntMessageHeaders::insertInt32(
	const std::string& key,
	const int32& value) {
	CAF_CM_FUNCNAME_VALIDATE("insertInt32");
	CAF_CM_VALIDATE_STRING(key);
	_headers->insert(std::make_pair(key, std::make_pair(CVariant::createInt32(value), SmartPtrICafObject())));
}

void CIntMessageHeaders::insertUint32(
	const std::string& key,
	const uint32& value) {
	CAF_CM_FUNCNAME_VALIDATE("insertUint32");
	CAF_CM_VALIDATE_STRING(key);
	_headers->insert(std::make_pair(key, std::make_pair(CVariant::createUint32(value), SmartPtrICafObject())));
}

void CIntMessageHeaders::insertInt16(
	const std::string& key,
	const int16& value) {
	CAF_CM_FUNCNAME_VALIDATE("insertInt16");
	CAF_CM_VALIDATE_STRING(key);
	_headers->insert(std::make_pair(key, std::make_pair(CVariant::createInt16(value), SmartPtrICafObject())));
}

void CIntMessageHeaders::insertUint16(
	const std::string& key,
	const uint16& value) {
	CAF_CM_FUNCNAME_VALIDATE("insertUint16");
	CAF_CM_VALIDATE_STRING(key);
	_headers->insert(std::make_pair(key, std::make_pair(CVariant::createUint16(value), SmartPtrICafObject())));
}

void CIntMessageHeaders::insertUint8(
	const std::string& key,
	const uint8& value) {
	CAF_CM_FUNCNAME_VALIDATE("insertUint8");
	CAF_CM_VALIDATE_STRING(key);
	_headers->insert(std::make_pair(key, std::make_pair(CVariant::createUint8(value), SmartPtrICafObject())));
}

void CIntMessageHeaders::insertBool(
	const std::string& key,
	const bool& value) {
	CAF_CM_FUNCNAME_VALIDATE("insertBool");
	CAF_CM_VALIDATE_STRING(key);
	_headers->insert(std::make_pair(key, std::make_pair(CVariant::createBool(value), SmartPtrICafObject())));
}

void CIntMessageHeaders::insertVariant(
	const std::string& key,
	const SmartPtrIVariant& variant) {
	CAF_CM_FUNCNAME_VALIDATE("insertVariant");
	CAF_CM_VALIDATE_STRING(key);
	CAF_CM_VALIDATE_SMARTPTR(variant);
	_headers->insert(std::make_pair(key, std::make_pair(variant, SmartPtrICafObject())));
}

void CIntMessageHeaders::insertObject(
	const std::string& key,
	const SmartPtrICafObject& cafObject) {
	CAF_CM_FUNCNAME_VALIDATE("insertObject");
	CAF_CM_VALIDATE_STRING(key);
	CAF_CM_VALIDATE_SMARTPTR(cafObject);
	_headers->insert(std::make_pair(key, std::make_pair(SmartPtrIVariant(), cafObject)));
}
