/*
 *	 Author: mdonahue
 *  Created: Feb 4, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Exception/CCafException.h"
#include "UriUtils.h"

using namespace Caf;

void UriUtils::parseUriString(
		const std::string& uri,
		UriUtils::SUriRecord& data) {
	CAF_CM_STATIC_FUNC("UriUtils", "parseUriString");
	CAF_CM_VALIDATE_STRING(uri);

	// examples:
	//
	// vmcf:service_id@tcp:host=hostname,port=portnum?timeout=timeoutval
	// tunnel:localhost:6672/amqp_queue_name?vhost=caf;connection_timeout=150000;connection_retries=10;connection_seconds_to_wait=10;channel_cache_size=3
	// amqp:guest:guest@10.25.91.81:5672/amqp_queue_name?vhost=caf;connection_timeout=150000;connection_retries=10;connection_seconds_to_wait=10;channel_cache_size=3

	static const std::string uriPattern("^(?P<protocol>[^:]+?):(?P<address>[^?]+)\\?\?(?P<parameters>.*)");
	static const std::string parmPattern("(?P<name>[^=]+)=(?P<value>[^;]+);?");
	static const std::string namePattern("^(?P<username>[^:]+):(?P<password>[^@]+)@(?P<hostpath>.*)");
	static const std::string hostPattern("^(?P<host>[^:]+):(?P<port>[^/]+)/");
	static const std::string pathPattern("/(?P<path>[^?]+)");

	CAF_CM_ENTER {
		data.protocol = std::string();
		data.address = std::string();
		data.parameters.clear();

		GRegex *regexUri = NULL;
		GRegex *regexParms = NULL;
		GRegex *regexName = NULL;
		GRegex *regexHost = NULL;
		GRegex *regexPath = NULL;
		GMatchInfo* matchInfo = NULL;

		try {
			CAF_CM_VALIDATE_STRING(uri);
			GError *error = NULL;
			regexUri = g_regex_new(uriPattern.c_str(),
								   (GRegexCompileFlags)(G_REGEX_RAW),
								   (GRegexMatchFlags)0,
								   &error);
			if (error) {
				throw error;
			}

			regexParms = g_regex_new(parmPattern.c_str(),
									 (GRegexCompileFlags)(G_REGEX_OPTIMIZE | G_REGEX_RAW),
									 (GRegexMatchFlags)0,
									 &error);
			if (error) {
				throw error;
			}

			regexName = g_regex_new(namePattern.c_str(),
									 (GRegexCompileFlags)(G_REGEX_OPTIMIZE | G_REGEX_RAW),
									 (GRegexMatchFlags)0,
									 &error);
			if (error) {
				throw error;
			}

			regexHost = g_regex_new(hostPattern.c_str(),
									 (GRegexCompileFlags)(G_REGEX_OPTIMIZE | G_REGEX_RAW),
									 (GRegexMatchFlags)0,
									 &error);
			if (error) {
				throw error;
			}

			regexPath = g_regex_new(pathPattern.c_str(),
									 (GRegexCompileFlags)(G_REGEX_OPTIMIZE | G_REGEX_RAW),
									 (GRegexMatchFlags)0,
									 &error);
			if (error) {
				throw error;
			}

			if (g_regex_match(regexUri, uri.c_str(), (GRegexMatchFlags)0, &matchInfo)) {
				gchar *value = g_match_info_fetch_named(matchInfo, "protocol");
				data.protocol = value;
				g_free(value);
				value = g_match_info_fetch_named(matchInfo, "address");
				data.address = value;
				g_free(value);
				value = g_match_info_fetch_named(matchInfo, "parameters");
				std::string params(value ? value : "");
				g_free(value);
				g_match_info_free(matchInfo);
				matchInfo = NULL;

				if (! data.address.empty()) {
					std::string hostpath = data.address;
					if (g_regex_match(regexName, data.address.c_str(), (GRegexMatchFlags)0, &matchInfo)) {
						value = g_match_info_fetch_named(matchInfo, "username");
						data.username = value;
						g_free(value);
						value = g_match_info_fetch_named(matchInfo, "password");
						data.password = value;
						g_free(value);
						value = g_match_info_fetch_named(matchInfo, "hostpath");
						hostpath = value;
						g_free(value);
					}
					g_match_info_free(matchInfo);
					matchInfo = NULL;

					if (g_regex_match(regexHost, hostpath.c_str(), (GRegexMatchFlags)0, &matchInfo)) {
						value = g_match_info_fetch_named(matchInfo, "host");
						data.host = value;
						g_free(value);
						value = g_match_info_fetch_named(matchInfo, "port");
						data.portStr = value;
						data.port = CStringConv::fromString<uint32>(value);
						g_free(value);
					}
					g_match_info_free(matchInfo);
					matchInfo = NULL;

					if (g_regex_match(regexPath, hostpath.c_str(), (GRegexMatchFlags)0, &matchInfo)) {
						value = g_match_info_fetch_named(matchInfo, "path");
						data.path = value;
						g_free(value);
					}
					g_match_info_free(matchInfo);
					matchInfo = NULL;
				}

				if (params.length()) {
					if (g_regex_match(regexParms, params.c_str(), (GRegexMatchFlags)0, &matchInfo)) {
						while (g_match_info_matches(matchInfo)) {
							value = g_match_info_fetch_named(matchInfo, "name");
							std::string paramName(value);
							g_free(value);
							value = g_match_info_fetch_named(matchInfo, "value");
							std::string paramValue(value);
							g_free(value);
							if (!data.parameters.insert(
									std::map<std::string, std::string>::value_type(paramName, paramValue)).second) {
								CAF_CM_EXCEPTION_VA2(ERROR_DUPLICATE_TAG,
													 "Duplicate parameter name %s in %s",
													 paramName.c_str(),
													 uri.c_str());
							}
							g_match_info_next(matchInfo, &error);
							if (error) {
								throw error;
							}
						}
					}
					g_match_info_free(matchInfo);
					matchInfo = NULL;
				}
			}
		}
		CAF_CM_CATCH_CAF
		CAF_CM_CATCH_GERROR

		if (matchInfo) {
			g_match_info_free(matchInfo);
		}
		if (regexUri) {
			g_regex_unref(regexUri);
		}
		if (regexParms) {
			g_regex_unref(regexParms);
		}
		if (regexName) {
			g_regex_unref(regexName);
		}
		if (regexHost) {
			g_regex_unref(regexHost);
		}
		if (regexPath) {
			g_regex_unref(regexPath);
		}
		CAF_CM_THROWEXCEPTION;
	}
	CAF_CM_EXIT;
}

std::string UriUtils::buildUriString(
		UriUtils::SUriRecord& data) {
	CAF_CM_STATIC_FUNC_VALIDATE("UriUtils", "buildUriString");
	CAF_CM_VALIDATE_STRING(data.protocol);
	CAF_CM_VALIDATE_STRING(data.host);
	CAF_CM_VALIDATE_STRING(data.path);

	std::string rc = data.protocol + ":";
	if (! data.username.empty() || ! data.password.empty()) {
		rc += data.username + ":" + data.password + "@";
	}

	rc += data.host;

	if (! data.portStr.empty()) {
		rc += ":" + data.portStr;
	}

	rc += "/" + data.path;

	rc = appendParameters(rc, data.parameters);

	return rc;
}

void UriUtils::parseFileAddress(const std::string& fileUri, UriUtils::SFileUriRecord& data) {
	CAF_CM_STATIC_FUNC_LOG("UriUtils", "parseFileAddress");
	CAF_CM_VALIDATE_STRING(fileUri);

	// examples:
	//
	// ///c:/tmp

	static const std::string addressPattern("^//(?P<hostname>[^/]*)/(?P<path>.*)");
	static const std::string drivePattern("^[a-zA-Z]:");

	CAF_CM_ENTER {
		data.hostname = std::string();
		data.path = std::string();

		GRegex *regexAddress = NULL;
		GRegex *regexDrive = NULL;
		GMatchInfo* matchInfo = NULL;

		try {
			CAF_CM_VALIDATE_STRING(fileUri);

			GError *error = NULL;
			regexAddress = g_regex_new(addressPattern.c_str(),
					(GRegexCompileFlags)(G_REGEX_RAW),
					(GRegexMatchFlags)0,
					&error);
			if (error) {
				throw error;
			}

			if (g_regex_match(regexAddress, fileUri.c_str(), (GRegexMatchFlags)0, &matchInfo)) {
				gchar *value = g_match_info_fetch_named(matchInfo, "hostname");
				data.hostname = value;
				g_free(value);
				value = g_match_info_fetch_named(matchInfo, "path");
				data.path = value;
				g_free(value);
				g_match_info_free(matchInfo);
				matchInfo = NULL;

				regexDrive = g_regex_new(drivePattern.c_str(),
						(GRegexCompileFlags)(G_REGEX_RAW),
						(GRegexMatchFlags)0,
						&error);
				if (error) {
					throw error;
				}

				if (! g_regex_match(regexDrive, data.path.c_str(), (GRegexMatchFlags)0, &matchInfo)) {
					data.path = "/" + data.path;
				}
				g_match_info_free(matchInfo);
				matchInfo = NULL;
			}
		}
		CAF_CM_CATCH_CAF
		CAF_CM_CATCH_GERROR

		if (matchInfo) {
			g_match_info_free(matchInfo);
		}
		if (regexAddress) {
			g_regex_unref(regexAddress);
		}
		if (regexDrive) {
			g_regex_unref(regexDrive);
		}
		CAF_CM_THROWEXCEPTION;
	}
	CAF_CM_EXIT;
}

std::string UriUtils::parseRequiredFilePath(
	const std::string& uriStr) {
	CAF_CM_STATIC_FUNC("UriUtils", "parseRequiredFilePath");
	CAF_CM_VALIDATE_STRING(uriStr);

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(uriStr);

		SUriRecord uriRecord;
		parseUriString(uriStr, uriRecord);

		if(uriRecord.protocol.compare("file") != 0) {
			CAF_CM_EXCEPTIONEX_VA2(InvalidArgumentException, ERROR_INVALID_DATA,
				"Unsupported protocol (%s != \"file\") - %s",
				uriRecord.protocol.c_str(), uriStr.c_str());
		}

		UriUtils::SFileUriRecord fileUriRecord;
		UriUtils::parseFileAddress(uriRecord.address, fileUriRecord);

		rc = CStringUtils::expandEnv(fileUriRecord.path);
		if(! FileSystemUtils::doesFileExist(rc)) {
			CAF_CM_EXCEPTIONEX_VA1(FileNotFoundException, ERROR_FILE_NOT_FOUND,
				"File in URI not found - %s", rc.c_str());
		}
	}
	CAF_CM_EXIT;

	return rc;
}

std::string UriUtils::parseOptionalFilePath(
	const std::string& uriStr) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("UriUtils", "parseOptionalFilePath");
	CAF_CM_VALIDATE_STRING(uriStr);

	std::string rc = "";

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(uriStr);

		SUriRecord uriRecord;
		parseUriString(uriStr, uriRecord);

		if(uriRecord.protocol.compare("file") != 0) {
			CAF_CM_LOG_DEBUG_VA2(
				"Unsupported protocol (%s != \"file\") - %s",
				uriRecord.protocol.c_str(), uriStr.c_str());
		} else {
			UriUtils::SFileUriRecord fileUriRecord;
			UriUtils::parseFileAddress(uriRecord.address, fileUriRecord);

			const std::string filePath = CStringUtils::expandEnv(fileUriRecord.path);
			if(! FileSystemUtils::doesFileExist(filePath)) {
				CAF_CM_LOG_DEBUG_VA2(
					"File in URI not found - uri: %s, path: %s", uriStr.c_str(), filePath.c_str());
			} else {
				rc = filePath;
			}
		}
	}
	CAF_CM_EXIT;

	return rc;
}

std::string UriUtils::appendParameters(
	const std::string& uriStr,
	const std::map<std::string, std::string>& parameters) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("UriUtils", "appendParameters");
	CAF_CM_VALIDATE_STRING(uriStr);

	std::string rc = uriStr;
	if (!parameters.empty()) {
		rc += "?";
		for (TConstMapIterator<std::map<std::string, std::string> > parameter(parameters);
			parameter; parameter++) {
			rc += parameter.getKey() + "=" + *parameter + ";";
		}
	}

	CAF_CM_LOG_DEBUG_VA1("Appended parameters - num: %d", parameters.size());

	return rc;
}

std::string UriUtils::findOptParameter(
		const UriUtils::SUriRecord& uri,
		const std::string& name,
		const std::string& defaultValue) {
	CAF_CM_STATIC_FUNC_VALIDATE("UriUtils", "findOptParameter");
	CAF_CM_VALIDATE_STRING(name);

	std::string rc = defaultValue;
	const std::map<std::string, std::string>::const_iterator param =
			uri.parameters.find(name);
	if(param != uri.parameters.end()) {
		rc = param->second;
	}

	return rc;
}

std::string UriUtils::findReqParameter(
		const UriUtils::SUriRecord& uri,
		const std::string& name) {
	CAF_CM_STATIC_FUNC("UriUtils", "findReqParameter");
	CAF_CM_VALIDATE_STRING(name);

	const std::map<std::string, std::string>::const_iterator param =
			uri.parameters.find(name);
	if(param == uri.parameters.end()) {
		CAF_CM_EXCEPTION_VA1(E_INVALIDARG, "param not found - %s", name.c_str());
	}

	return param->second;
}
