/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Common/CCmdLineOptions.h"
#include "Exception/CCafException.h"

using namespace Caf;

CCmdLineOptions::CCmdLineOptions(void) :
	_isInitialized(false),
	_optionCnt(0),
	_maxOptions(0),
	_gOptions(NULL),
	CAF_CM_INIT_LOG("CCmdLineOptions") {
}

CCmdLineOptions::~CCmdLineOptions(void) {
	CAF_CM_FUNCNAME("~CCmdLineOptions");
	try {
		for(TConstIterator<CStringOptions> option(_stringOptions); option && (option->second != NULL); option++) {
			g_free(option->second);
		}
	}
	CAF_CM_CATCH_ALL
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
}

void CCmdLineOptions::initialize(
	const std::string& cmdDescription,
	const uint32 maxOptions) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(cmdDescription);
		CAF_CM_VALIDATE_POSITIVE(maxOptions);

		_cmdDescription = cmdDescription;
		_maxOptions = maxOptions;
		_gOptions = new GOptionEntry[_maxOptions + 1];

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

void CCmdLineOptions::addStringOption(
	const std::string& longName,
	const char shortName,
	const std::string& optionDescription) {
	CAF_CM_FUNCNAME_VALIDATE("addStringOption");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(longName);
		CAF_CM_VALIDATE_STRING(optionDescription);

		checkOptionCnt(longName, _optionCnt, _maxOptions);
		populateOption(_gOptions[_optionCnt], longName, shortName, optionDescription);

		_stringOptions.insert(std::make_pair(longName, static_cast<gchar*>(NULL)));
		_gOptions[_optionCnt].arg_data = &(_stringOptions.find(longName)->second);
		_gOptions[_optionCnt].arg = G_OPTION_ARG_STRING;

		_optionCnt++;
	}
	CAF_CM_EXIT;
}

void CCmdLineOptions::addIntOption(
	const std::string& longName,
	const char shortName,
	const std::string& optionDescription) {
	CAF_CM_FUNCNAME_VALIDATE("addIntOption");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(longName);
		CAF_CM_VALIDATE_STRING(optionDescription);

		checkOptionCnt(longName, _optionCnt, _maxOptions);
		populateOption(_gOptions[_optionCnt], longName, shortName, optionDescription);

		_intOptions.insert(std::make_pair(longName, 0));
		_gOptions[_optionCnt].arg_data = &(_intOptions.find(longName)->second);
		_gOptions[_optionCnt].arg = G_OPTION_ARG_INT;

		_optionCnt++;
	}
	CAF_CM_EXIT;
}

void CCmdLineOptions::addBoolOption(
	const std::string& longName,
	const char shortName,
	const std::string& optionDescription) {
	CAF_CM_FUNCNAME_VALIDATE("addBoolOption");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(longName);
		CAF_CM_VALIDATE_STRING(optionDescription);

		checkOptionCnt(longName, _optionCnt, _maxOptions);
		populateOption(_gOptions[_optionCnt], longName, shortName, optionDescription);

		_boolOptions.insert(std::make_pair(longName, FALSE));
		_gOptions[_optionCnt].arg_data = &(_boolOptions.find(longName)->second);
		_gOptions[_optionCnt].arg = G_OPTION_ARG_NONE;

		_optionCnt++;
	}
	CAF_CM_EXIT;
}

void CCmdLineOptions::parse(
	int32 argc,
	char* argv[]) {
	CAF_CM_FUNCNAME("parse");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		_gOptions[_optionCnt] = GOptionEntry();

		GError *gError = NULL;
		GOptionContext *gContext = g_option_context_new(_cmdDescription.c_str());
		g_option_context_add_main_entries(gContext, _gOptions, NULL);
		if(!g_option_context_parse(gContext, &argc, &argv, &gError)) {
			CAF_CM_VALIDATE_PTR(gError);

			const std::string errorMessage = gError->message;
			const int32 errorCode = gError->code;

			g_error_free(gError);
			g_option_context_free(gContext);

			CAF_CM_EXCEPTION_VA1(errorCode, "option parsing failed: %s", errorMessage.c_str());
		}

		g_option_context_free(gContext);
	}
	CAF_CM_EXIT;
}

std::string CCmdLineOptions::findStringOption(
	const std::string& longName) {
	CAF_CM_FUNCNAME("findStringOption");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(longName);

		CStringOptions::const_iterator iter = _stringOptions.find(longName);
		if(iter == _stringOptions.end()) {
			CAF_CM_EXCEPTION_VA1(E_FAIL, "String option not found: %s", longName.c_str());
		}

		if(iter->second != NULL) {
			rc = iter->second;
		}
	}
	CAF_CM_EXIT;

	return rc;
}

int32 CCmdLineOptions::findIntOption(
	const std::string& longName) {
	CAF_CM_FUNCNAME("findIntOption");

	int32 rc = 0;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(longName);

		CIntOptions::const_iterator iter = _intOptions.find(longName);
		if(iter == _intOptions.end()) {
			CAF_CM_EXCEPTION_VA1(E_FAIL, "Int option not found: %s", longName.c_str());
		}

		rc = iter->second;
	}
	CAF_CM_EXIT;

	return rc;
}

bool CCmdLineOptions::findBoolOption(
	const std::string& longName) {
	CAF_CM_FUNCNAME("findBoolOption");

	bool rc = false;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(longName);

		CBoolOptions::const_iterator iter = _boolOptions.find(longName);
		if(iter == _boolOptions.end()) {
			CAF_CM_EXCEPTION_VA1(E_FAIL, "Bool option not found: %s", longName.c_str());
		}

		rc = iter->second == GLIB_TRUE ? true : false;
	}
	CAF_CM_EXIT;

	return rc;
}

void CCmdLineOptions::checkOptionCnt(
	const std::string& longName,
	const uint32 optionCnt,
	const uint32 maxOptions) const {
	CAF_CM_FUNCNAME("checkOptionCnt");

	CAF_CM_ENTER {
		if(optionCnt >= maxOptions) {
			CAF_CM_EXCEPTION_VA3(E_INVALIDARG, "\"%s\" exceeded the maximum number of allowed options (%d >= %d)",
				longName.c_str(), optionCnt, maxOptions);
		}
	}
	CAF_CM_EXIT;
}

void CCmdLineOptions::populateOption(
	GOptionEntry& optionEntry,
	const std::string& longName,
	const char shortName,
	const std::string& optionDescription) {

	CAF_CM_ENTER {
		// We can't guarantee that longName's and optionDescription's scopes will be sufficient, so store the strings.
		_longNames.push_back(longName);
		_optionDescriptions.push_back(optionDescription);

		optionEntry.long_name = _longNames.back().c_str();
		optionEntry.short_name = shortName;
		optionEntry.flags = 0;
		optionEntry.description = _optionDescriptions.back().c_str();
		optionEntry.arg_description = NULL;
	}
	CAF_CM_EXIT;
}
