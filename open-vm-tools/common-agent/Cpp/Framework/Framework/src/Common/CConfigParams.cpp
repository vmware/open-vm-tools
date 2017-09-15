/*
 *	 Author: mdonahue
 *  Created: Jan 17, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Common/CConfigParams.h"
#include "Exception/CCafException.h"
#include "Common/IConfigParams.h"

using namespace Caf;

CConfigParams::CConfigParams() :
	_table(NULL),
	CAF_CM_INIT("CConfigParams") {
}

CConfigParams::~CConfigParams() {
	if (_table)
		g_hash_table_unref( _table);
}

void CConfigParams::initialize(
	const std::string& sectionName,
	EKeyManagement keyManagement,
	EValueManagement valueManagement) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER
	{
		CAF_CM_PRECOND_ISNOTINITIALIZED(_table);
		CAF_CM_VALIDATE_STRING(sectionName);
		_sectionName = sectionName;
		_table = g_hash_table_new_full(
			g_str_hash,
			g_str_equal,
			EKeysManaged == keyManagement ? destroyKeyCallback : NULL,
			EValuesManaged == valueManagement ? destroyValueCallback : NULL);
	}
	CAF_CM_EXIT;
}

void CConfigParams::insert(const char* key, GVariant* value) {
	CAF_CM_FUNCNAME_VALIDATE("insert");

	CAF_CM_ENTER
	{
		CAF_CM_PRECOND_ISINITIALIZED(_table);
		CAF_CM_VALIDATE_STRINGPTRA(key);
		CAF_CM_VALIDATE_PTR(value);
		g_hash_table_insert(_table, (void*)key, value);
	}
	CAF_CM_EXIT;
}

GVariant* CConfigParams::lookup(
	const char* key,
	const EParamDisposition disposition) const {
	CAF_CM_FUNCNAME("lookup");

	GVariant* value = NULL;
	CAF_CM_ENTER
	{
		CAF_CM_PRECOND_ISINITIALIZED(_table);
		CAF_CM_VALIDATE_STRINGPTRA(key);
		value = (GVariant*)g_hash_table_lookup(_table, key);
		if (!value && (disposition == PARAM_REQUIRED)) {
			CAF_CM_EXCEPTION_VA2(
				ERROR_TAG_NOT_FOUND,
				"Required config parameter [%s] is missing from section [%s]",
				key,
				_sectionName.c_str());
		}
	}
	CAF_CM_EXIT;

	return value;
}

std::string CConfigParams::getSectionName() const {
	CAF_CM_FUNCNAME_VALIDATE("getSectionName");
	CAF_CM_ENTER
	{
		CAF_CM_PRECOND_ISINITIALIZED(_table);
	}
	CAF_CM_EXIT;
	return _sectionName;
}

void CConfigParams::destroyKeyCallback(gpointer ptr) {
	g_free(ptr);
}

void CConfigParams::destroyValueCallback(gpointer ptr) {
	g_variant_unref((GVariant*)ptr);
}
