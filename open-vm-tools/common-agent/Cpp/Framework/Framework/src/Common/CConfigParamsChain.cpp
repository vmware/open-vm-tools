/*
 *	 Author: mdonahue
 *  Created: Jan 17, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Common/CConfigParamsChain.h"
#include "Exception/CCafException.h"
#include "Common/IConfigParams.h"

using namespace Caf;

CConfigParamsChain::CConfigParamsChain() :
	CAF_CM_INIT("CConfigParamsChain") {
}

CConfigParamsChain::~CConfigParamsChain() {
}

void CConfigParamsChain::initialize(
	CConfigParams::EKeyManagement keyManagement,
	CConfigParams::EValueManagement valueManagement,
	const SmartPtrIConfigParams& baseParams) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_theseParams);
	CAF_CM_VALIDATE_INTERFACE(baseParams);

	_baseParams = baseParams;
	_theseParams.CreateInstance();
	_theseParams->initialize(
		_baseParams->getSectionName(),
		keyManagement,
		valueManagement);
}

void CConfigParamsChain::insert(const char* key, GVariant* value) {
	CAF_CM_FUNCNAME_VALIDATE("insert");
	CAF_CM_PRECOND_ISINITIALIZED(_theseParams);

	_theseParams->insert(key, value);
}

GVariant* CConfigParamsChain::lookup(
	const char* key,
	const EParamDisposition disposition) const {
	CAF_CM_FUNCNAME("lookup");
	CAF_CM_PRECOND_ISINITIALIZED(_theseParams);

	GVariant* value = _theseParams->lookup(key, PARAM_OPTIONAL);
	if (!value) {
		value = _baseParams->lookup(key, PARAM_OPTIONAL);
	}

	if (!value && (disposition == PARAM_REQUIRED)) {
		CAF_CM_EXCEPTION_VA2(
			ERROR_TAG_NOT_FOUND,
			"Required config parameter [%s] is missing from section [%s]",
			key,
			_baseParams->getSectionName().c_str());
	}

	return value;
}

std::string CConfigParamsChain::getSectionName() const {
	CAF_CM_FUNCNAME_VALIDATE("lookup");
	CAF_CM_PRECOND_ISINITIALIZED(_theseParams);

	return _theseParams->getSectionName();
}
