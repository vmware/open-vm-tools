/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Integration/IIntMessage.h"
#include "Exception/CCafException.h"
#include "Integration/Core/CMessageHeaderUtils.h"

using namespace Caf;

std::string CMessageHeaderUtils::getStringReq(
	const IIntMessage::SmartPtrCHeaders& headers,
	const std::string& tag) {
	CAF_CM_STATIC_FUNC("CMessageHeaderUtils", "getStringReq");

	if (headers->find(tag) == headers->end()) {
		CAF_CM_EXCEPTION_VA1(ERROR_NOT_FOUND, "Header not found: %s", tag.c_str());
	}
	return getStringOpt(headers, tag);
}

std::string CMessageHeaderUtils::getStringOpt(
	const IIntMessage::SmartPtrCHeaders& headers,
	const std::string& tag) {

	std::string rc = 0;
	IIntMessage::CHeaders::const_iterator iter = headers->find(tag);
	if (iter != headers->end()) {
		rc = iter->second.first->toString();
	}
	return rc;
}

uint8 CMessageHeaderUtils::getUint8Req(
	const IIntMessage::SmartPtrCHeaders& headers,
	const std::string& tag) {
	CAF_CM_STATIC_FUNC("CMessageHeaderUtils", "getUint8Req");

	if (headers->find(tag) == headers->end()) {
		CAF_CM_EXCEPTION_VA1(ERROR_NOT_FOUND, "Header not found: %s", tag.c_str());
	}
	return getUint8Opt(headers, tag);
}

uint8 CMessageHeaderUtils::getUint8Opt(
	const IIntMessage::SmartPtrCHeaders& headers,
	const std::string& tag) {

	uint8 rc = 0;
	IIntMessage::CHeaders::const_iterator header = headers->find(tag);
	if (header != headers->end()) {
		if (g_variant_is_of_type(header->second.first->get(), G_VARIANT_TYPE_BYTE)) {
			rc = g_variant_get_byte(header->second.first->get());
		} else {
			rc = CStringConv::fromString<uint8>(header->second.first->toString());
		}
	}
	return rc;
}

uint16 CMessageHeaderUtils::getUint16Req(
	const IIntMessage::SmartPtrCHeaders& headers,
	const std::string& tag) {
	CAF_CM_STATIC_FUNC("CMessageHeaderUtils", "getUint16Req");

	if (headers->find(tag) == headers->end()) {
		CAF_CM_EXCEPTION_VA1(ERROR_NOT_FOUND, "Header not found: %s", tag.c_str());
	}
	return getUint16Opt(headers, tag);
}

uint16 CMessageHeaderUtils::getUint16Opt(
	const IIntMessage::SmartPtrCHeaders& headers,
	const std::string& tag) {

	uint16 rc = 0;
	IIntMessage::CHeaders::const_iterator header = headers->find(tag);
	if (header != headers->end()) {
		if (g_variant_is_of_type(header->second.first->get(), G_VARIANT_TYPE_UINT16)) {
			rc = g_variant_get_uint16(header->second.first->get());
		} else {
			rc = CStringConv::fromString<uint16>(header->second.first->toString());
		}
	}
	return rc;
}

uint32 CMessageHeaderUtils::getUint32Req(
	const IIntMessage::SmartPtrCHeaders& headers,
	const std::string& tag) {
	CAF_CM_STATIC_FUNC("CMessageHeaderUtils", "getUint32Req");

	if (headers->find(tag) == headers->end()) {
		CAF_CM_EXCEPTION_VA1(ERROR_NOT_FOUND, "Header not found: %s", tag.c_str());
	}
	return getUint32Opt(headers, tag);
}

uint32 CMessageHeaderUtils::getUint32Opt(
	const IIntMessage::SmartPtrCHeaders& headers,
	const std::string& tag) {

	uint32 rc = 0;
	IIntMessage::CHeaders::const_iterator header = headers->find(tag);
	if (header != headers->end()) {
		if (g_variant_is_of_type(header->second.first->get(), G_VARIANT_TYPE_UINT32)) {
			rc = g_variant_get_uint32(header->second.first->get());
		} else {
			rc = CStringConv::fromString<uint32>(header->second.first->toString());
		}
	}
	return rc;
}

uint64 CMessageHeaderUtils::getUint64Req(
	const IIntMessage::SmartPtrCHeaders& headers,
	const std::string& tag) {
	CAF_CM_STATIC_FUNC("CMessageHeaderUtils", "getUint64Req");

	if (headers->find(tag) == headers->end()) {
		CAF_CM_EXCEPTION_VA1(ERROR_NOT_FOUND, "Header not found: %s", tag.c_str());
	}
	return getUint64Opt(headers, tag);
}

uint64 CMessageHeaderUtils::getUint64Opt(
	const IIntMessage::SmartPtrCHeaders& headers,
	const std::string& tag) {

	uint64 rc = 0;
	IIntMessage::CHeaders::const_iterator header = headers->find(tag);
	if (header != headers->end()) {
		if (g_variant_is_of_type(header->second.first->get(), G_VARIANT_TYPE_UINT64)) {
			rc = g_variant_get_uint64(header->second.first->get());
		} else {
			rc = CStringConv::fromString<uint64>(header->second.first->toString());
		}
	}
	return rc;
}

bool CMessageHeaderUtils::getBoolReq(
	const IIntMessage::SmartPtrCHeaders& headers,
	const std::string& tag) {
	CAF_CM_STATIC_FUNC("CMessageHeaderUtils", "getBoolReq");

	if (headers->find(tag) == headers->end()) {
		CAF_CM_EXCEPTION_VA1(ERROR_NOT_FOUND, "Header not found: %s", tag.c_str());
	}
	return getBoolOpt(headers, tag);
}

bool CMessageHeaderUtils::getBoolOpt(
	const IIntMessage::SmartPtrCHeaders& headers,
	const std::string& tag) {

	bool rc = false;
	IIntMessage::CHeaders::const_iterator header = headers->find(tag);
	if (header != headers->end()) {
		if (g_variant_is_of_type(header->second.first->get(), G_VARIANT_TYPE_BOOLEAN)) {
			rc = g_variant_get_boolean(header->second.first->get());
		} else {
			const std::string val = header->second.first->toString();

			rc = true;
			if ((val == "0")
				|| (g_ascii_strncasecmp(val.c_str(), "false", val.length()) == 0)) {
				rc = false;
			}
		}
	}
	return rc;
}

void CMessageHeaderUtils::log(
	const IIntMessage::SmartPtrCHeaders& headers,
	const log4cpp::Priority::PriorityLevel priorityLevel) {
	CAF_CM_STATIC_FUNC_LOG_ONLY("CMessageHeaderUtils", "log");

	if (_logger.isPriorityEnabled(priorityLevel)) {
			for (IIntMessage::CHeaders::const_iterator headerIter = headers->begin();
			headerIter != headers->end();
			headerIter++) {
			std::stringstream logMessage;
			logMessage << '['
				<< headerIter->first
				<< '='
				<< headerIter->second.first->toString()
				<< ']';
			_logger.logVA(priorityLevel, _cm_funcName_,  __LINE__, logMessage.str().c_str());
		}
	}
}
