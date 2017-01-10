/*
 *  Created on: Jun 6, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/CVariant.h"
#include "Integration/IIntMessage.h"
#include "HeaderUtils.h"

using namespace Caf::AmqpIntegration;

SmartPtrCVariant HeaderUtils::getHeaderString(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag) {
	SmartPtrCVariant variant;
	IIntMessage::CHeaders::const_iterator header =
			headers->find(tag);
	if (header != headers->end()) {
		variant = CVariant::createString(header->second.first->toString());
	}
	return variant;
}

SmartPtrCVariant HeaderUtils::getHeaderUint8(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag) {
	SmartPtrCVariant variant;
	IIntMessage::CHeaders::const_iterator header =
			headers->find(tag);
	if (header != headers->end()) {
		if (g_variant_is_of_type(header->second.first->get(), G_VARIANT_TYPE_BYTE)) {
			variant = CVariant::createUint8(g_variant_get_byte(header->second.first->get()));
		} else {
			variant = CVariant::createUint8(
					CStringConv::fromString<uint8>(header->second.first->toString()));
		}
	}
	return variant;
}

SmartPtrCVariant HeaderUtils::getHeaderUint16(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag) {
	SmartPtrCVariant variant;
	IIntMessage::CHeaders::const_iterator header =
			headers->find(tag);
	if (header != headers->end()) {
		if (g_variant_is_of_type(header->second.first->get(), G_VARIANT_TYPE_UINT16)) {
			variant = CVariant::createUint16(g_variant_get_uint16(header->second.first->get()));
		} else {
			variant = CVariant::createUint16(
					CStringConv::fromString<uint16>(header->second.first->toString()));
		}
	}
	return variant;
}

SmartPtrCVariant HeaderUtils::getHeaderUint32(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag) {
	SmartPtrCVariant variant;
	IIntMessage::CHeaders::const_iterator header =
			headers->find(tag);
	if (header != headers->end()) {
		if (g_variant_is_of_type(header->second.first->get(), G_VARIANT_TYPE_UINT32)) {
			variant = CVariant::createUint32(g_variant_get_uint32(header->second.first->get()));
		} else {
			variant = CVariant::createUint32(
					CStringConv::fromString<uint32>(header->second.first->toString()));
		}
	}
	return variant;
}

SmartPtrCVariant HeaderUtils::getHeaderUint64(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag) {
	SmartPtrCVariant variant;
	IIntMessage::CHeaders::const_iterator header =
			headers->find(tag);
	if (header != headers->end()) {
		if (g_variant_is_of_type(header->second.first->get(), G_VARIANT_TYPE_UINT64)) {
			variant = CVariant::createUint64(g_variant_get_uint64(header->second.first->get()));
		} else {
			variant = CVariant::createUint64(
					CStringConv::fromString<uint64>(header->second.first->toString()));
		}
	}
	return variant;
}

SmartPtrCVariant HeaderUtils::getHeaderBool(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag) {
	SmartPtrCVariant variant;
	IIntMessage::CHeaders::const_iterator header =
			headers->find(tag);
	if (header != headers->end()) {
		if (g_variant_is_of_type(header->second.first->get(), G_VARIANT_TYPE_BOOLEAN)) {
			variant = CVariant::createBool(g_variant_get_boolean(header->second.first->get()));
		} else {
			const std::string val = header->second.first->toString();
			variant = CVariant::createBool(
					(val == "0") ||
					(g_ascii_strncasecmp(val.c_str(), "false", val.length()) == 0)
					? false : true);
		}
	}
	return variant;
}
