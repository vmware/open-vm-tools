/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Exception/CCafException.h"
#include "Integration/Caf/CBeanPropertiesHelper.h"

using namespace Caf;

SmartPtrCBeanPropertiesHelper CBeanPropertiesHelper::create(
		const IBean::Cprops& properties) {
	SmartPtrCBeanPropertiesHelper rc;
	rc.CreateInstance();
	rc->initialize(properties);

	return rc;
}

CBeanPropertiesHelper::CBeanPropertiesHelper(void) :
	m_isInitialized(false),
	CAF_CM_INIT("CBeanPropertiesHelper") {
}

CBeanPropertiesHelper::~CBeanPropertiesHelper(void) {
}

void CBeanPropertiesHelper::initialize(
		const IBean::Cprops& properties) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(m_isInitialized);

	_properties = properties;
	m_isInitialized = true;
}

std::string CBeanPropertiesHelper::getRequiredString(
		const std::string& key) const {
	CAF_CM_FUNCNAME("getRequiredString");
	CAF_CM_PRECOND_ISINITIALIZED(m_isInitialized);
	CAF_CM_VALIDATE_STRING(key);

	const IBean::Cprops::const_iterator iter = _properties.find(key);
	if (_properties.end() == iter) {
		CAF_CM_EXCEPTION_VA1(E_FAIL,
				"Required property not found - %s", key.c_str());
	}

	return iter->second;
}

std::string CBeanPropertiesHelper::getOptionalString(
		const std::string& key,
		const std::string& defaultVal) const {
	CAF_CM_FUNCNAME_VALIDATE("getOptionalString");
	CAF_CM_PRECOND_ISINITIALIZED(m_isInitialized);
	CAF_CM_VALIDATE_STRING(key);

	std::string rc = defaultVal;
	const IBean::Cprops::const_iterator iter = _properties.find(key);
	if (_properties.end() != iter) {
		rc = iter->second;
	}

	return rc;
}

bool CBeanPropertiesHelper::getRequiredBool(
		const std::string& key) const {
	CAF_CM_FUNCNAME_VALIDATE("getRequiredBool");
	CAF_CM_PRECOND_ISINITIALIZED(m_isInitialized);
	CAF_CM_VALIDATE_STRING(key);

	const std::string val = getRequiredString(key);
	return (val.compare("true") == 0);
}

bool CBeanPropertiesHelper::getOptionalBool(
		const std::string& key,
		const bool defaultValue) const {
	CAF_CM_FUNCNAME_VALIDATE("getOptionalBool");
	CAF_CM_PRECOND_ISINITIALIZED(m_isInitialized);
	CAF_CM_VALIDATE_STRING(key);

	bool rc = defaultValue;
	const IBean::Cprops::const_iterator iter = _properties.find(key);
	if (_properties.end() != iter) {
		rc = (iter->second.compare("true") == 0);
	}

	return rc;
}
