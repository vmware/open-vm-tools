/*
 *	Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Common/IAppConfig.h"
#include "AppConfigUtils.h"

using namespace Caf;

std::string AppConfigUtils::getRequiredString(
	const std::string& parameterName) {
	std::string rc;
	getAppConfig()->getGlobalString(parameterName, rc, IConfigParams::PARAM_REQUIRED);
	return rc;
}

uint32 AppConfigUtils::getRequiredUint32(
	const std::string& parameterName) {
	uint32 rc;
	getAppConfig()->getGlobalUint32(parameterName, rc, IConfigParams::PARAM_REQUIRED);
	return rc;
}

int32 AppConfigUtils::getRequiredInt32(
	const std::string& parameterName) {
	int32 rc;
	getAppConfig()->getGlobalInt32(parameterName, rc, IConfigParams::PARAM_REQUIRED);
	return rc;
}

bool AppConfigUtils::getRequiredBoolean(
	const std::string& parameterName) {
	bool rc;
	getAppConfig()->getGlobalBoolean(parameterName, rc, IConfigParams::PARAM_REQUIRED);
	return rc;
}

std::string AppConfigUtils::getOptionalString(
	const std::string& parameterName) {
	std::string rc;
	getAppConfig()->getGlobalString(parameterName, rc, IConfigParams::PARAM_OPTIONAL);
	return rc;
}

uint32 AppConfigUtils::getOptionalUint32(
	const std::string& parameterName) {
	uint32 rc = 0;
	getAppConfig()->getGlobalUint32(parameterName, rc, IConfigParams::PARAM_OPTIONAL);
	return rc;
}

int32 AppConfigUtils::getOptionalInt32(
	const std::string& parameterName) {
	int32 rc = 0;
	getAppConfig()->getGlobalInt32(parameterName, rc, IConfigParams::PARAM_OPTIONAL);
	return rc;
}

bool AppConfigUtils::getOptionalBoolean(
	const std::string& parameterName) {
	bool rc = false;
	getAppConfig()->getGlobalBoolean(parameterName, rc, IConfigParams::PARAM_OPTIONAL);
	return rc;
}

std::string AppConfigUtils::getRequiredString(
	const std::string& sectionName,
	const std::string& parameterName) {
	std::string rc;
	getAppConfig()->getString(sectionName, parameterName, rc, IConfigParams::PARAM_REQUIRED);
	return rc;
}

uint32 AppConfigUtils::getRequiredUint32(
	const std::string& sectionName,
	const std::string& parameterName) {
	uint32 rc;
	getAppConfig()->getUint32(sectionName, parameterName, rc, IConfigParams::PARAM_REQUIRED);
	return rc;
}

int32 AppConfigUtils::getRequiredInt32(
	const std::string& sectionName,
	const std::string& parameterName) {
	int32 rc;
	getAppConfig()->getInt32(sectionName, parameterName, rc, IConfigParams::PARAM_REQUIRED);
	return rc;
}

bool AppConfigUtils::getRequiredBoolean(
	const std::string& sectionName,
	const std::string& parameterName) {
	bool rc;
	getAppConfig()->getBoolean(sectionName, parameterName, rc, IConfigParams::PARAM_REQUIRED);
	return rc;
}

std::string AppConfigUtils::getOptionalString(
	const std::string& sectionName,
	const std::string& parameterName) {
	std::string rc;
	getAppConfig()->getString(sectionName, parameterName, rc, IConfigParams::PARAM_OPTIONAL);
	return rc;
}

uint32 AppConfigUtils::getOptionalUint32(
	const std::string& sectionName,
	const std::string& parameterName) {
	uint32 rc = 0;
	getAppConfig()->getUint32(sectionName, parameterName, rc, IConfigParams::PARAM_OPTIONAL);
	return rc;
}

int32 AppConfigUtils::getOptionalInt32(
	const std::string& sectionName,
	const std::string& parameterName) {
	int32 rc = 0;
	getAppConfig()->getInt32(sectionName, parameterName, rc, IConfigParams::PARAM_OPTIONAL);
	return rc;
}

bool AppConfigUtils::getOptionalBoolean(
	const std::string& sectionName,
	const std::string& parameterName) {
	bool rc = false;
	getAppConfig()->getBoolean(sectionName, parameterName, rc, IConfigParams::PARAM_OPTIONAL);
	return rc;
}
