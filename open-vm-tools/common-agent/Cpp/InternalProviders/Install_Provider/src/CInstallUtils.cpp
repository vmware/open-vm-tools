/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Exception/CCafException.h"
#include "CInstallUtils.h"

using namespace Caf;

CInstallUtils::MATCH_STATUS CInstallUtils::compareVersions(
	const std::string& packageVersion1,
	const std::string& packageVersion2) {
	CAF_CM_STATIC_FUNC_LOG("CInstallUtils", "compareVersions");

	MATCH_STATUS matchStatus = MATCH_NOTEQUAL;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STRING(packageVersion1);
		CAF_CM_VALIDATE_STRING(packageVersion2);

		const Cdeqstr packageVersionTokens1 = CStringUtils::split(packageVersion1, '.');
		if (packageVersionTokens1.size() != 3) {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Package version 1 has a bad format - %s",
				packageVersion1.c_str());
		}

		const Cdeqstr packageVersionTokens2 = CStringUtils::split(packageVersion2, '.');
		if (packageVersionTokens2.size() != 3) {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Package version 2 has a bad format - %s",
				packageVersion2.c_str());
		}

		if ((packageVersionTokens1[0].compare(packageVersionTokens2[0]) == 0)
			&& (packageVersionTokens1[1].compare(packageVersionTokens2[1]) == 0)) {
			const uint32 lastVersionPos1 = CStringConv::fromString<uint32>(
				packageVersionTokens1[2]);
			const uint32 lastVersionPos2 = CStringConv::fromString<uint32>(
				packageVersionTokens2[2]);
			if (lastVersionPos1 == lastVersionPos2) {
				matchStatus = MATCH_VERSION_EQUAL;
			} else if (lastVersionPos1 < lastVersionPos2) {
				matchStatus = MATCH_VERSION_LESS;
			} else {
				matchStatus = MATCH_VERSION_GREATER;
			}
		}
	}
	CAF_CM_EXIT;

	return matchStatus;
}
