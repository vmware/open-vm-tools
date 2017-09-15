/*
 *	Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Common/CCafRegex.h"
#include "Exception/CCafException.h"

using namespace Caf;

CCafRegex::CCafRegex() :
	_isInitialized(false),
	_gRegex(NULL),
	CAF_CM_INIT_LOG("CCafRegex") {
}

CCafRegex::~CCafRegex() {
	CAF_CM_FUNCNAME("~CCafRegex");

	try {
		if(NULL != _gRegex) {
			g_regex_unref(_gRegex);
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
}

void CCafRegex::initialize(const std::string& regex) {
	CAF_CM_FUNCNAME("initialize");

	GError* gError = NULL;
	try {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(regex);

		_gRegex = g_regex_new(
			regex.c_str(),
			(GRegexCompileFlags)(G_REGEX_OPTIMIZE | G_REGEX_RAW),
			(GRegexMatchFlags)0,
			&gError);
		if(NULL == _gRegex) {
			const std::string errorMessage = (gError == NULL) ? "" : gError->message;
			const int32 errorCode = (gError == NULL) ? 0 : gError->code;
			CAF_CM_EXCEPTION_VA1(errorCode, "g_regex_new Failed: \"%s\"", errorMessage.c_str());
		}

		if(NULL != gError) {
			g_error_free(gError);
		}

		_regex = regex;
		_isInitialized = true;
	}
	catch(...) {
		if(NULL != gError) {
			g_error_free(gError);
		}
		throw;
	}
}

bool CCafRegex::isMatched(const std::string& source) {
	CAF_CM_FUNCNAME_VALIDATE("isMatched");

	bool rc = false;
	GMatchInfo* gMatchInfo = NULL;

	try {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(source);

		gboolean gIsMatch = g_regex_match(_gRegex, source.c_str(), (GRegexMatchFlags)0, &gMatchInfo);
		rc = (FALSE == gIsMatch) ? false : true;

		if(NULL != gMatchInfo) {
			g_match_info_free(gMatchInfo);
		}
	}
	catch(...) {
		if(NULL != gMatchInfo) {
			g_match_info_free(gMatchInfo);
		}
		throw;
	}

	return rc;
}

std::map<std::string, std::string> CCafRegex::matchNames(
	const std::string& source,
	const std::set<std::string>& names) {

	CAF_CM_FUNCNAME_VALIDATE("matchNames");

	std::map<std::string, std::string> rc;
	GMatchInfo* gMatchInfo = NULL;

	try {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(source);
		CAF_CM_VALIDATE_STL(names);

		gboolean gIsMatch = g_regex_match(_gRegex, source.c_str(), (GRegexMatchFlags)0, &gMatchInfo);
		if(GLIB_TRUE == gIsMatch) {
			CAF_CM_VALIDATE_PTR(gMatchInfo);

			for(TConstIterator<std::set<std::string> > name(names); name; name++) {
				gchar* gString = g_match_info_fetch_named(gMatchInfo, (*name).c_str());
				if(NULL != gString) {
					rc.insert(std::make_pair(*name, gString));
					g_free(gString);
				}
			}
		}

		if(NULL != gMatchInfo) {
			g_match_info_free(gMatchInfo);
		}
	}
	catch(...) {
		if(NULL != gMatchInfo) {
			g_match_info_free(gMatchInfo);
		}
		throw;
	}

	return rc;
}

std::deque<std::string> CCafRegex::matchName(
	const std::string& source,
	const std::string& name) {

	CAF_CM_FUNCNAME("matchName");

	std::deque<std::string> rc;
	GMatchInfo* gMatchInfo = NULL;
	GError* gError = NULL;

	try {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(source);
		CAF_CM_VALIDATE_STRING(name);

		gboolean gIsMatch = g_regex_match(_gRegex, source.c_str(), (GRegexMatchFlags)0, &gMatchInfo);
		if(GLIB_TRUE == gIsMatch) {
			CAF_CM_VALIDATE_PTR(gMatchInfo);

			while(g_match_info_matches(gMatchInfo)) {
				gchar* gString = g_match_info_fetch_named(gMatchInfo, name.c_str());
				rc.push_back(gString);
				g_free(gString);

				g_match_info_next(gMatchInfo, &gError);
				if(NULL != gError) {
					CAF_CM_EXCEPTION_VA1(gError->code, "g_match_info_next Failed: \"%s\"", gError->message);
				}
			}
		}

		if(NULL != gMatchInfo) {
			g_match_info_free(gMatchInfo);
		}
	}
	catch(...) {
		if(NULL != gMatchInfo) {
			g_match_info_free(gMatchInfo);
		}
		if(NULL != gError) {
			g_error_free(gError);
		}
		throw;
	}

	return rc;
}

std::string CCafRegex::match(
	const std::string& source,
	const int32 matchNum) {

	CAF_CM_FUNCNAME_VALIDATE("match");

	std::string rc;
	GMatchInfo* gMatchInfo = NULL;

	try {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(source);

		gboolean gIsMatch = g_regex_match(_gRegex, source.c_str(), (GRegexMatchFlags)0, &gMatchInfo);
		if(GLIB_TRUE == gIsMatch) {
			CAF_CM_VALIDATE_PTR(gMatchInfo);

			gchar* gString = g_match_info_fetch(gMatchInfo, matchNum);
			rc = gString;
			g_free(gString);
		}

		if(NULL != gMatchInfo) {
			g_match_info_free(gMatchInfo);
		}
	}
	catch(...) {
		if(NULL != gMatchInfo) {
			g_match_info_free(gMatchInfo);
		}
	}

	return rc;
}

std::string CCafRegex::replaceLiteral(
	const std::string& source,
	const std::string& replacement) {

	CAF_CM_FUNCNAME("replaceLiteral");

	std::string rc;
	GError* gError = NULL;

	try {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(source);
		CAF_CM_VALIDATE_STRING(replacement);

		gchar* gString = g_regex_replace_literal(
			_gRegex, source.c_str(), -1, 0, replacement.c_str(), G_REGEX_MATCH_NOTBOL, &gError);

		if(NULL != gError) {
			CAF_CM_EXCEPTION_VA1(gError->code, "g_regex_replace_literal Failed: \"%s\"", gError->message);
		}

		rc = gString;
		g_free(gString);
	}
	catch(...) {
		if(NULL != gError) {
			g_error_free(gError);
		}
		throw;
	}

	return rc;
}

std::string CCafRegex::replaceLiteral(
	const std::string& regex,
	const std::string& source,
	const std::string& replacement) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafRegex", "replaceLiteral");
	CAF_CM_VALIDATE_STRING(regex);
	CAF_CM_VALIDATE_STRING(source);
	CAF_CM_VALIDATE_STRING(replacement);

	CCafRegex cafRegex;
	cafRegex.initialize(regex);
	return cafRegex.replaceLiteral(source, replacement);
}
