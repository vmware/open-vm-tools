/*
 *  Created on: Jun 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Exception/CCafException.h"
#include "Common/CVariant.h"

using namespace Caf;

CVariant::CVariant() :
	_variant(NULL),
	CAF_CM_INIT("CVariant") {
}

CVariant::~CVariant() {
	if (_variant) {
		g_variant_unref(_variant);
	}
}

void CVariant::set(GVariant *variant) {
	CAF_CM_FUNCNAME_VALIDATE("set");
	CAF_CM_VALIDATE_PTR(variant);
	if (_variant) {
		g_variant_unref(_variant);
		_variant = NULL;
	}
	_variant = g_variant_ref_sink(variant);
}

GVariant *CVariant::get() const {
	CAF_CM_FUNCNAME_VALIDATE("get");
	CAF_CM_VALIDATE_PTR(_variant);
	return _variant;
}

std::string CVariant::toString() const {
	CAF_CM_FUNCNAME("toString");
	CAF_CM_VALIDATE_PTR(_variant);
	std::string result;

	const GVariantType *variantType = g_variant_get_type(_variant);
	if (g_variant_type_equal(variantType, G_VARIANT_TYPE_BOOLEAN)) {
		result = g_variant_get_boolean(_variant) ? "true" : "false";
	} else if (g_variant_type_equal(variantType, G_VARIANT_TYPE_BYTE)) {
		result = CStringConv::toString<uint16>(g_variant_get_byte(_variant));
	} else if (g_variant_type_equal(variantType, G_VARIANT_TYPE_INT16)) {
		result = CStringConv::toString<int16>(g_variant_get_int16(_variant));
	} else if (g_variant_type_equal(variantType, G_VARIANT_TYPE_UINT16)) {
		result = CStringConv::toString<uint16>(g_variant_get_uint16(_variant));
	} else if (g_variant_type_equal(variantType, G_VARIANT_TYPE_INT32)) {
		result = CStringConv::toString<int32>(g_variant_get_int32(_variant));
	} else if (g_variant_type_equal(variantType, G_VARIANT_TYPE_UINT32)) {
		result = CStringConv::toString<uint32>(g_variant_get_uint32(_variant));
	} else if (g_variant_type_equal(variantType, G_VARIANT_TYPE_INT64)) {
		result = CStringConv::toString<int64>(g_variant_get_int64(_variant));
	} else if (g_variant_type_equal(variantType, G_VARIANT_TYPE_UINT64)) {
		result = CStringConv::toString<uint64>(g_variant_get_uint64(_variant));
	} else if (g_variant_type_equal(variantType, G_VARIANT_TYPE_STRING)) {
		result = g_variant_get_string(_variant, NULL);
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				UnsupportedOperationException,
				0,
				"Unsupported GVariant conversion to string from type '%s'",
				 g_variant_get_type_string(_variant));
	}
	return result;
}

bool CVariant::isString() const {
	return isType(G_VARIANT_TYPE_STRING);
}

bool CVariant::isBool() const {
	return isType(G_VARIANT_TYPE_BOOLEAN);
}

bool CVariant::isUint8() const {
	return isType(G_VARIANT_TYPE_BYTE);
}

bool CVariant::isInt16() const {
	return isType(G_VARIANT_TYPE_INT16);
}

bool CVariant::isUint16() const {
	return isType(G_VARIANT_TYPE_UINT16);
}

bool CVariant::isInt32() const {
	return isType(G_VARIANT_TYPE_INT32);
}

bool CVariant::isUint32() const {
	return isType(G_VARIANT_TYPE_UINT32);
}

bool CVariant::isInt64() const {
	return isType(G_VARIANT_TYPE_INT64);
}

bool CVariant::isUint64() const {
	return isType(G_VARIANT_TYPE_UINT64);
}

SmartPtrCVariant CVariant::createString(const std::string& value) {
	SmartPtrCVariant variant;
	variant.CreateInstance();
	variant->set(g_variant_new_string(value.c_str()));
	return variant;
}

SmartPtrCVariant CVariant::createBool(const bool value) {
	SmartPtrCVariant variant;
	variant.CreateInstance();
	variant->set(g_variant_new_boolean(value));
	return variant;
}

SmartPtrCVariant CVariant::createUint8(const uint8 value) {
	SmartPtrCVariant variant;
	variant.CreateInstance();
	variant->set(g_variant_new_byte(value));
	return variant;
}

SmartPtrCVariant CVariant::createInt16(const int16 value) {
	SmartPtrCVariant variant;
	variant.CreateInstance();
	variant->set(g_variant_new_int16(value));
	return variant;
}

SmartPtrCVariant CVariant::createUint16(const uint16 value) {
	SmartPtrCVariant variant;
	variant.CreateInstance();
	variant->set(g_variant_new_uint16(value));
	return variant;
}

SmartPtrCVariant CVariant::createInt32(const int32 value) {
	SmartPtrCVariant variant;
	variant.CreateInstance();
	variant->set(g_variant_new_int32(value));
	return variant;
}

SmartPtrCVariant CVariant::createUint32(const uint32 value) {
	SmartPtrCVariant variant;
	variant.CreateInstance();
	variant->set(g_variant_new_uint32(value));
	return variant;
}

SmartPtrCVariant CVariant::createInt64(const int64 value) {
	SmartPtrCVariant variant;
	variant.CreateInstance();
	variant->set(g_variant_new_int64(value));
	return variant;
}

SmartPtrCVariant CVariant::createUint64(const uint64 value) {
	SmartPtrCVariant variant;
	variant.CreateInstance();
	variant->set(g_variant_new_uint64(value));
	return variant;
}

bool CVariant::isType(const GVariantType * varType) const {
	CAF_CM_FUNCNAME_VALIDATE("isType");
	CAF_CM_VALIDATE_PTR(_variant);
	CAF_CM_VALIDATE_PTR(varType);
	return g_variant_type_equal(g_variant_get_type(_variant), varType);
}
