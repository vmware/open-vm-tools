/*
 *	Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Exception/CCafException.h"
#include "Common/IAppConfig.h"
#include "CStringUtils.h"

#include <algorithm>
#include <cctype>
#include <functional>

using namespace Caf;

Cdeqstr CStringUtils::split(const std::string &str, const char delim) {
	Cdeqstr rc;
	std::string token;
	std::stringstream stream(str);
	while(std::getline(stream, token, delim)) {
		rc.push_back(token);
	}

	return rc;
}

// trim from both ends
std::string CStringUtils::trim(const std::string &s) {
	std::string sTmp = s;
	return trimLeft(trimRight(sTmp));
}

// trim from start
std::string CStringUtils::trimLeft(const std::string &s) {
	std::string sTmp = s;
	sTmp.erase(
		sTmp.begin(),
		std::find_if(
			sTmp.begin(),
			sTmp.end(),
			std::not1(std::ptr_fun<int32, int32>(std::isspace))));
	return sTmp;
}

// trim from end
std::string CStringUtils::trimRight(const std::string &s) {
	std::string sTmp = s;
	sTmp.erase(
		std::find_if(
			sTmp.rbegin(),
			sTmp.rend(),
			std::not1(std::ptr_fun<int32, int32>(std::isspace))).base(), sTmp.end());
	return sTmp;
}

// expand the environment variable in the string.
std::string CStringUtils::expandEnv(const std::string &envStr) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CStringUtils", "expandEnv");
	CAF_CM_VALIDATE_STRING(envStr);

	return getAppConfig()->resolveValue(envStr);
}

UUID CStringUtils::createRandomUuidRaw() {
	CAF_CM_STATIC_FUNC_LOG("CStringUtils", "createRandomUuidRaw");

	UUID randomUuid;
	if (S_OK != ::UuidCreate(&randomUuid)) {
		CAF_CM_EXCEPTIONEX_VA0(InvalidHandleException, E_UNEXPECTED,
			"Failed to create the UUID");
	}

	return randomUuid;
}

std::string CStringUtils::createRandomUuid() {
	return BasePlatform::UuidToString(createRandomUuidRaw());
}

inline bool caseInsCharCompare(char a, char b) {
	return(::toupper(a) == ::toupper(b));
}

bool CStringUtils::isEqualIgnoreCase(
	const std::string & src,
	const std::string & srch) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CStringUtils", "isEqualIgnoreCase");
	CAF_CM_VALIDATE_STRING(src);
	CAF_CM_VALIDATE_STRING(srch);

	return((src.size() == srch.size()) &&
		std::equal(src.begin(), src.end(), srch.begin(), caseInsCharCompare));

//	std::string::iterator it = std::search(
//		src.begin(), src.end(),
//		srch.begin(), srch.end(),
//		[](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
//	);
//
//	return (it != src.end());
}

#ifdef WIN32
std::string CStringUtils::convertWideToNarrow(
	const std::wstring& src) {
	CAF_CM_STATIC_FUNC_LOG("CStringUtils", "convertWideToNarrow");

	std::string rc;

	// deal with trivial case of empty string
	if (! src.empty()) {
		// determine required length of new string
		const int32 srcLength = static_cast<int32>(src.length());
		const int32 reqLength = ::WideCharToMultiByte(CP_UTF8, 0, src.c_str(), srcLength, NULL, 0, NULL, NULL);

		// construct new string of required length
		std::string dst(reqLength, '\0');

		// convert old string to new string
		const int32 dstLength = static_cast<int32>(dst.length());
		::WideCharToMultiByte(CP_UTF8, 0, src.c_str(), srcLength, &dst[0], dstLength, NULL, NULL);

		rc = dst;
	}

	return rc;
}

std::wstring CStringUtils::convertNarrowToWide(
	const std::string& src) {
	CAF_CM_STATIC_FUNC_LOG("CStringUtils", "convertNarrowToWide");

	std::wstring rc;

	// deal with trivial case of empty string
	if (! src.empty()) {
		// determine required length of new string
		const int32 srcLength = static_cast<int32>(src.length());
		const int32 reqLength = ::MultiByteToWideChar(CP_UTF8, 0, src.c_str(), srcLength, NULL, 0);

		// construct new string of required length
		std::wstring dst(reqLength, L'\0');

		// convert old string to new string
		const int32 dstLength = static_cast<int32>(dst.length());
		::MultiByteToWideChar(CP_UTF8, 0, src.c_str(), srcLength, &dst[0], dstLength);

		rc = dst;
	}

	return rc;
}
#endif

std::string CStringUtils::toLower(
		const std::string& str) {
	CAF_CM_STATIC_FUNC_VALIDATE("CStringUtils", "toLower");
	CAF_CM_VALIDATE_STRING(str);

	std::string rc(str);
	std::transform(
			str.begin(),
			str.end(),
			rc.begin(),
			std::ptr_fun<int, int>(std::tolower));

	return rc;
}

std::string CStringUtils::toUpper(
		const std::string& str) {
	CAF_CM_STATIC_FUNC_VALIDATE("CStringUtils", "toUpper");
	CAF_CM_VALIDATE_STRING(str);

	std::string rc(str);
	std::transform(
			str.begin(),
			str.end(),
			rc.begin(),
			std::ptr_fun<int, int>(std::toupper));

	return rc;
}
