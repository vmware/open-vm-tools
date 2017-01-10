/*
 *	 Author: mdonahue
 *  Created: Feb 4, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef UriUtils_h_
#define UriUtils_h_

namespace Caf {

namespace UriUtils {

	struct SUriRecord {
		std::string protocol;
		std::string address;
		std::string username;
		std::string password;
		std::string host;
		uint32 port;
		std::string portStr;
		std::string path;
		std::map<std::string, std::string> parameters;
	};

	struct SFileUriRecord {
		std::string hostname;
		std::string path;
	};

	void COMMONAGGREGATOR_LINKAGE parseUriString(const std::string& uri, SUriRecord& data);
	std::string COMMONAGGREGATOR_LINKAGE buildUriString(SUriRecord& data);
	void COMMONAGGREGATOR_LINKAGE parseFileAddress(const std::string& fileUri, SFileUriRecord& data);
	std::string COMMONAGGREGATOR_LINKAGE parseRequiredFilePath(const std::string& uriStr);
	std::string COMMONAGGREGATOR_LINKAGE parseOptionalFilePath(const std::string& uriStr);
	std::string COMMONAGGREGATOR_LINKAGE appendParameters(const std::string& uriStr,
		const std::map<std::string, std::string>& parameters);

	std::string COMMONAGGREGATOR_LINKAGE findOptParameter(
			const UriUtils::SUriRecord& uri,
			const std::string& name,
			const std::string& defaultValue = std::string());
	std::string COMMONAGGREGATOR_LINKAGE findReqParameter(
			const UriUtils::SUriRecord& uri,
			const std::string& name);
};

}

#endif /* UriUtils_h_ */
