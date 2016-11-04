/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CConfigProvider_h_
#define CConfigProvider_h_


#include "IInvokedProvider.h"

#include "Doc/ProviderResultsDoc/CSchemaDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassInstanceDoc.h"
#include "Xml/XmlUtils/CXmlElement.h"

namespace Caf {

/// Sends responses/errors back to the client.
class CConfigProvider : public IInvokedProvider {
public:
	CConfigProvider();
	virtual ~CConfigProvider();

public: // IInvokedProvider
	const std::string getProviderNamespace() const {
		return "caf";
	}

	const std::string getProviderName() const {
		return "ConfigProvider";
	}

	const std::string getProviderVersion() const {
		return "1.0.0";
	}

	const SmartPtrCSchemaDoc getSchema() const;

	void collect(const IProviderRequest& request, IProviderResponse& response) const;

	void invoke(const IProviderRequest& request, IProviderResponse& response) const;

private:
	std::string _fileAliasPrefix;
	std::string _keyPathDelimStr;
	char _keyPathDelimChar;

private:
	SmartPtrCDataClassInstanceDoc createDataClassInstance(
		const std::string& filePath,
		const std::string& encoding,
		const std::deque<std::pair<std::string, std::string> >& propertyCollection) const;

	void setValue(
		const std::string& filePath,
		const std::string& encoding,
		const std::string& valueName,
		const std::string& valueValue) const;

	void deleteValue(
		const std::string& filePath,
		const std::string& encoding,
		const std::string& valueName) const;

	std::deque<std::pair<std::string, std::string> > createIniFileWithoutSectionPropertyCollection(
		const std::string& filePath) const;

	std::deque<std::pair<std::string, std::string> > createIniFilePropertyCollection(
		const std::string& filePath) const;

	std::deque<std::pair<std::string, std::string> > createXmlFilePropertyCollection(
		const std::string& filePath) const;

	void createXmlPropertyCollection(
		const std::string& keyPath,
		const SmartPtrCXmlElement& thisXml,
		std::deque<std::pair<std::string, std::string> >& propertyCollection) const;

	void parseIniFileValuePath(
		const std::string& valuePath,
		std::string& valueName,
		std::string& valueValue) const;

	void parseKeyPath(
		const std::string& keyPath,
		std::deque<std::string>& keyPathCollection,
		std::string& keyName) const;

	SmartPtrCXmlElement findXmlElement(
		const std::deque<std::string>& keyPathCollection,
		const SmartPtrCXmlElement& rootXml) const;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CConfigProvider);
};

}

#endif // #ifndef CConfigProvider_h_
