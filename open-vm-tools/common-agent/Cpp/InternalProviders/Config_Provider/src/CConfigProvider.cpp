/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CIniFileWithoutSection.h"
#include "Common/CIniFile.h"
#include "Doc/CafCoreTypesDoc/COperationDoc.h"
#include "Doc/CafCoreTypesDoc/CParameterCollectionDoc.h"
#include "Doc/ProviderRequestDoc/CProviderCollectInstancesDoc.h"
#include "Doc/ProviderRequestDoc/CProviderInvokeOperationDoc.h"
#include "Doc/ProviderResultsDoc/CSchemaDoc.h"
#include "Doc/SchemaTypesDoc/CActionClassDoc.h"
#include "Doc/SchemaTypesDoc/CClassInstancePropertyDoc.h"
#include "Doc/SchemaTypesDoc/CClassPropertyDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassInstanceDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassPropertyDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassSubInstanceDoc.h"
#include "Doc/SchemaTypesDoc/CMethodDoc.h"
#include "Doc/SchemaTypesDoc/CMethodParameterDoc.h"
#include "Integration/IDocument.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Exception/CCafException.h"
#include "CConfigProvider.h"
#include "IProviderRequest.h"
#include "IProviderResponse.h"

using namespace Caf;

CConfigProvider::CConfigProvider() :
	_fileAliasPrefix("diagFileAlias_"),
	_keyPathDelimStr("/"),
	_keyPathDelimChar('/'),
	CAF_CM_INIT_LOG("CConfigProvider") {
}

CConfigProvider::~CConfigProvider() {
}

const SmartPtrCSchemaDoc CConfigProvider::getSchema() const {

	std::deque<SmartPtrCClassPropertyDoc> dc1Props;
	dc1Props.push_back(CProviderDocHelper::createClassProperty("name", PROPERTY_STRING, true));
	dc1Props.push_back(CProviderDocHelper::createClassProperty("value", PROPERTY_STRING, true));

	std::deque<SmartPtrCClassPropertyDoc> dc2Props;
	dc2Props.push_back(CProviderDocHelper::createClassProperty("filePath", PROPERTY_STRING, true));
	dc2Props.push_back(CProviderDocHelper::createClassProperty("encoding", PROPERTY_STRING, true));

	std::deque<SmartPtrCClassInstancePropertyDoc> instanceProperties;
	instanceProperties.push_back(CProviderDocHelper::createClassInstanceProperty(
			"configEntry",
			CProviderDocHelper::createClassIdentifier("caf", "ConfigEntry", "1.0.0"),
			true,
			false,
			true));

	std::deque<SmartPtrCDataClassDoc> dataClasses;
	dataClasses.push_back(CProviderDocHelper::createDataClass("caf", "ConfigEntry", "1.0.0", dc1Props));
	dataClasses.push_back(CProviderDocHelper::createDataClass("caf", "ConfigData", "1.0.0", dc2Props, instanceProperties));

	std::deque<SmartPtrCMethodParameterDoc> collectMethodParams;
	collectMethodParams.push_back(CProviderDocHelper::createMethodParameter("filePath", PARAMETER_STRING, false));
	collectMethodParams.push_back(CProviderDocHelper::createMethodParameter("encoding", PARAMETER_STRING, false));

	std::deque<SmartPtrCMethodParameterDoc> m1Params;
	m1Params.push_back(CProviderDocHelper::createMethodParameter("filePath", PARAMETER_STRING, false));
	m1Params.push_back(CProviderDocHelper::createMethodParameter("encoding", PARAMETER_STRING, false));
	m1Params.push_back(CProviderDocHelper::createMethodParameter("valueName", PARAMETER_STRING, false));
	m1Params.push_back(CProviderDocHelper::createMethodParameter("valueData", PARAMETER_STRING, false));

	std::deque<SmartPtrCMethodParameterDoc> m2Params;
	m2Params.push_back(CProviderDocHelper::createMethodParameter("filePath", PARAMETER_STRING, false));
	m2Params.push_back(CProviderDocHelper::createMethodParameter("encoding", PARAMETER_STRING, false));
	m2Params.push_back(CProviderDocHelper::createMethodParameter("valueName", PARAMETER_STRING, false));

	std::deque<SmartPtrCMethodDoc> methods;
	methods.push_back(CProviderDocHelper::createMethod("setValue", m1Params));
	methods.push_back(CProviderDocHelper::createMethod("deleteValue", m2Params));

	std::deque<SmartPtrCActionClassDoc> actionClasses;
	actionClasses.push_back(
			CProviderDocHelper::createActionClass(
			"caf",
			"ConfigActions",
			"1.0.0",
			CProviderDocHelper::createCollectMethod("collectInstances", collectMethodParams),
			methods));

	return CProviderDocHelper::createSchema(dataClasses, actionClasses);
}

void CConfigProvider::collect(const IProviderRequest& request, IProviderResponse& response) const {
	CAF_CM_FUNCNAME("collect");

	CAF_CM_ENTER {
		SmartPtrCProviderCollectInstancesDoc doc = request.getCollectInstances();
		CAF_CM_VALIDATE_SMARTPTR(doc);

		const SmartPtrCParameterCollectionDoc parameterCollection =
				doc->getParameterCollection();
		const std::string filePath = ParameterUtils::findRequiredParameterAsString(
			"filePath", parameterCollection);
		const std::string encoding = ParameterUtils::findRequiredParameterAsString(
			"encoding", parameterCollection);

		if (FileSystemUtils::doesFileExist(filePath)) {
			CAF_CM_LOG_DEBUG_VA2("Parsing file - path: %s, encoding: %s", filePath.c_str(), encoding.c_str());

			std::deque<std::pair<std::string, std::string> > propertyCollection;
			if (encoding.compare("iniFileWithoutSection") == 0) {
				propertyCollection = createIniFileWithoutSectionPropertyCollection(filePath);
			} else if (encoding.compare("iniFile") == 0) {
				propertyCollection = createIniFilePropertyCollection(filePath);
			} else if (encoding.compare("xmlFile") == 0) {
				propertyCollection = createXmlFilePropertyCollection(filePath);
			} else {
				CAF_CM_EXCEPTIONEX_VA2(InvalidArgumentException, E_INVALIDARG,
					"URI encoding is not recognized - path: %s, encoding: %s", filePath.c_str(), encoding.c_str());
			}

			const SmartPtrCDataClassInstanceDoc dataClassInstance =
				createDataClassInstance(filePath, encoding, propertyCollection);
			response.addInstance(dataClassInstance);
		} else {
			CAF_CM_LOG_WARN_VA2(
				"File not found - path: %s, encoding: %s", filePath.c_str(), encoding.c_str());
		}
	}
	CAF_CM_EXIT;
}

void CConfigProvider::invoke(const IProviderRequest& request, IProviderResponse& response) const {
	CAF_CM_FUNCNAME("invoke");

	CAF_CM_ENTER {
		SmartPtrCProviderInvokeOperationDoc doc = request.getInvokeOperations();
		CAF_CM_VALIDATE_SMARTPTR(doc);

		const SmartPtrCOperationDoc operation = doc->getOperation();
		const std::string operationName = operation->getName();

		const SmartPtrCParameterCollectionDoc parameterCollection =
			operation->getParameterCollection();
		const std::string filePath = ParameterUtils::findRequiredParameterAsString(
			"filePath", parameterCollection);
		const std::string encoding = ParameterUtils::findRequiredParameterAsString(
			"encoding", parameterCollection);
		const std::string valueName = ParameterUtils::findRequiredParameterAsString(
			"valueName", parameterCollection);

		if (operationName.compare("setValue") == 0) {
			const std::string valueData = ParameterUtils::findRequiredParameterAsString(
				"valueData", parameterCollection);

			setValue(filePath, encoding, valueName, valueData);
		} else if (operationName.compare("deleteValue") == 0) {
			deleteValue(filePath, encoding, valueName);
		} else {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Invalid operation name (must be \'setValue\' or \'deleteValue\') - %s", operationName.c_str())
		}
	}
	CAF_CM_EXIT;
}

SmartPtrCDataClassInstanceDoc CConfigProvider::createDataClassInstance(
	const std::string& filePath,
	const std::string& encoding,
	const std::deque<std::pair<std::string, std::string> >& propertyCollection) const {
	CAF_CM_FUNCNAME_VALIDATE("createDataClassInstance");

	CAF_CM_VALIDATE_STRING(filePath);
	CAF_CM_VALIDATE_STRING(encoding);
	CAF_CM_VALIDATE_STL(propertyCollection);

	std::deque<SmartPtrCDataClassSubInstanceDoc> subInstances;
	for(TConstIterator<std::deque<std::pair<std::string, std::string> > > propertyIter(propertyCollection);
		propertyIter; propertyIter++) {

		std::deque<SmartPtrCDataClassPropertyDoc> siProperties;
		siProperties.push_back(CProviderDocHelper::createDataClassProperty("name", propertyIter->first));
		siProperties.push_back(CProviderDocHelper::createDataClassProperty("value", propertyIter->second));
		subInstances.push_back(CProviderDocHelper::createDataClassSubInstance(
				"configEntry",
				siProperties));
	}

	std::deque<SmartPtrCDataClassPropertyDoc> dataClassProperties;
	dataClassProperties.push_back(CProviderDocHelper::createDataClassProperty("filePath", filePath));
	dataClassProperties.push_back(CProviderDocHelper::createDataClassProperty("encoding", encoding));

	return CProviderDocHelper::createDataClassInstance(
			"caf",
			"ConfigData",
			"1.0.0",
			dataClassProperties,
			subInstances);
}

void CConfigProvider::setValue(
	const std::string& filePath,
	const std::string& encoding,
	const std::string& valueName,
	const std::string& valueData) const {
	CAF_CM_FUNCNAME("setValue");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(filePath);
		CAF_CM_VALIDATE_STRING(encoding);
		CAF_CM_VALIDATE_STRING(valueName);
		CAF_CM_VALIDATE_STRING(valueData);

		if (encoding.compare("iniFileWithoutSection") == 0) {
			SmartPtrCIniFileWithoutSection iniFileWithoutSection;
			iniFileWithoutSection.CreateInstance();
			iniFileWithoutSection->initialize(filePath);
			iniFileWithoutSection->setValue(valueName, valueData);
		} else if (encoding.compare("iniFile") == 0) {
			std::string sectionName;
			std::string keyName;
			parseIniFileValuePath(valueName, sectionName, keyName);

			SmartPtrCIniFile iniFile;
			iniFile.CreateInstance();
			iniFile->initialize(filePath);
			iniFile->setValue(sectionName, keyName, valueData);
		} else if (encoding.compare("xmlFile") == 0) {
			std::string keyName;
			std::deque<std::string> keyPathCollection;
			parseKeyPath(valueName, keyPathCollection, keyName);

			const SmartPtrCXmlElement rootXml = CXmlUtils::parseFile(filePath, std::string());
			const SmartPtrCXmlElement parentXml = findXmlElement(keyPathCollection, rootXml);

			const SmartPtrCXmlElement foundElement = parentXml->findOptionalChild(keyName);
			if (! foundElement.IsNull()) {
				foundElement->setValue(valueData);
			} else {
				const std::string foundAttribute = parentXml->findOptionalAttribute(keyName);
				if (! foundAttribute.empty()) {
					parentXml->setAttribute(keyName, valueData);
				} else {
					const SmartPtrCXmlElement createdElement = parentXml->createAndAddElement(keyName);
					createdElement->setValue(valueData);
				}
			}
			rootXml->saveToFile(filePath);
		} else {
			CAF_CM_EXCEPTIONEX_VA2(InvalidArgumentException, E_INVALIDARG,
				"URI encoding is not recognized - filePath: %s, encoding: %s",
				filePath.c_str(), encoding.c_str());
		}
	}
	CAF_CM_EXIT;
}

void CConfigProvider::deleteValue(
	const std::string& filePath,
	const std::string& encoding,
	const std::string& valueName) const {
	CAF_CM_FUNCNAME("deleteValue");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(filePath);
		CAF_CM_VALIDATE_STRING(encoding);
		CAF_CM_VALIDATE_STRING(valueName);

		if (encoding.compare("iniFileWithoutSection") == 0) {
			SmartPtrCIniFileWithoutSection iniFileWithoutSection;
			iniFileWithoutSection.CreateInstance();
			iniFileWithoutSection->initialize(filePath);
			iniFileWithoutSection->deleteValue(valueName);
		} else if (encoding.compare("iniFile") == 0) {
			std::string sectionName;
			std::string keyName;
			parseIniFileValuePath(valueName, sectionName, keyName);

			SmartPtrCIniFile iniFile;
			iniFile.CreateInstance();
			iniFile->initialize(filePath);
			iniFile->deleteValue(sectionName, keyName);
		} else if (encoding.compare("xmlFile") == 0) {
			std::string keyName;
			std::deque<std::string> keyPathCollection;
			parseKeyPath(valueName, keyPathCollection, keyName);

			const SmartPtrCXmlElement rootXml = CXmlUtils::parseFile(filePath, std::string());
			const SmartPtrCXmlElement parentXml = findXmlElement(keyPathCollection, rootXml);

			const SmartPtrCXmlElement foundElement = parentXml->findOptionalChild(keyName);
			if (! foundElement.IsNull()) {
				parentXml->removeChild(keyName);
			} else {
				parentXml->removeAttribute(keyName);
			}
			rootXml->saveToFile(filePath);
		} else {
			CAF_CM_EXCEPTIONEX_VA2(InvalidArgumentException, E_INVALIDARG,
				"URI encoding is not recognized - filePath: %s, encoding: %s",
				filePath.c_str(), encoding.c_str());
		}
	}
	CAF_CM_EXIT;
}

std::deque<std::pair<std::string, std::string> > CConfigProvider::createIniFileWithoutSectionPropertyCollection(
	const std::string& filePath) const {
	CAF_CM_FUNCNAME_VALIDATE("createIniFileWithoutSectionPropertyCollection");

	std::deque<std::pair<std::string, std::string> > propertyCollection;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(filePath);

		SmartPtrCIniFileWithoutSection iniFileWithoutSection;
		iniFileWithoutSection.CreateInstance();
		iniFileWithoutSection->initialize(filePath);

		const std::deque<CIniFileWithoutSection::SmartPtrSIniEntry> entryCollection =
			iniFileWithoutSection->getEntryCollection();

		for(TConstIterator<std::deque<CIniFileWithoutSection::SmartPtrSIniEntry> > iniEntryIter(entryCollection);
			iniEntryIter; iniEntryIter++) {
			const CIniFileWithoutSection::SmartPtrSIniEntry iniEntry = *iniEntryIter;
			propertyCollection.push_back(std::make_pair(iniEntry->_name, iniEntry->_valueExpanded));
		}
	}
	CAF_CM_EXIT;

	return propertyCollection;
}

std::deque<std::pair<std::string, std::string> > CConfigProvider::createIniFilePropertyCollection(
	const std::string& filePath) const {
	CAF_CM_FUNCNAME_VALIDATE("createIniFilePropertyCollection");

	std::deque<std::pair<std::string, std::string> > propertyCollection;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(filePath);

		SmartPtrCIniFile iniFile;
		iniFile.CreateInstance();
		iniFile->initialize(filePath);

		const std::deque<CIniFile::SmartPtrSIniSection> sectionCollection = iniFile->getSectionCollection();
		for (TConstIterator<std::deque<CIniFile::SmartPtrSIniSection> > iniSectionIter(sectionCollection);
			iniSectionIter; iniSectionIter++) {
			const CIniFile::SmartPtrSIniSection iniSection = *iniSectionIter;
			const std::string sectionName = iniSection->_sectionName;

			for (TConstIterator<std::deque<CIniFile::SmartPtrSIniEntry> > iniEntryIter(iniSection->_entryCollection);
				iniEntryIter; iniEntryIter++) {
				const CIniFile::SmartPtrSIniEntry iniEntry = *iniEntryIter;
				const std::string keyName = iniEntry->_name;
				const std::string value = iniEntry->_valueRaw;

				const std::string keyPath = sectionName + _keyPathDelimStr + keyName;
				propertyCollection.push_back(std::make_pair(keyPath, value));
			}
		}
	}
	CAF_CM_EXIT;

	return propertyCollection;
}

std::deque<std::pair<std::string, std::string> > CConfigProvider::createXmlFilePropertyCollection(
	const std::string& filePath) const {
	CAF_CM_FUNCNAME_VALIDATE("createXmlFilePropertyCollection");

	std::deque<std::pair<std::string, std::string> > propertyCollection;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(filePath);

		const SmartPtrCXmlElement rootXml = CXmlUtils::parseFile(filePath, std::string());

		const std::string keyPath = rootXml->getName();
		createXmlPropertyCollection(keyPath, rootXml, propertyCollection);
	}
	CAF_CM_EXIT;

	return propertyCollection;
}

void CConfigProvider::createXmlPropertyCollection(
	const std::string& keyPath,
	const SmartPtrCXmlElement& thisXml,
	std::deque<std::pair<std::string, std::string> >& propertyCollection) const {
	CAF_CM_FUNCNAME_VALIDATE("createXmlPropertyCollection");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(keyPath);
		CAF_CM_VALIDATE_SMARTPTR(thisXml);
		// propertyCollection is optional

		const CXmlElement::SmartPtrCAttributeCollection attributeCollection = thisXml->getAllAttributes();
		if (! attributeCollection.IsNull() && ! attributeCollection->empty()) {
			for (TConstIterator<CXmlElement::CAttributeCollection> attributeXmlIter(*attributeCollection);
				attributeXmlIter; attributeXmlIter++) {
				const std::string attributeName = attributeXmlIter->first;
				const std::string attributeValue = attributeXmlIter->second;

				const std::string newKeyPath = keyPath + _keyPathDelimStr + attributeName;
				propertyCollection.push_back(std::make_pair(newKeyPath, attributeValue));
			}
		}

		const CXmlElement::SmartPtrCElementCollection childrenXml = thisXml->getAllChildren();
		if (! childrenXml.IsNull() && ! childrenXml->empty()) {
			for (TConstIterator<CXmlElement::CElementCollection > childrenXmlIter(*childrenXml);
				childrenXmlIter; childrenXmlIter++) {
				const SmartPtrCXmlElement childXml = childrenXmlIter->second;
				const std::string newKeyPath = keyPath + _keyPathDelimStr + childXml->getName();

				const std::string value = childXml->getValue();
				if (! value.empty()) {
					propertyCollection.push_back(std::make_pair(newKeyPath, value));
				}

				createXmlPropertyCollection(newKeyPath, childXml, propertyCollection);
			}
		}
	}
	CAF_CM_EXIT;
}

void CConfigProvider::parseIniFileValuePath(
	const std::string& valuePath,
	std::string& valueName,
	std::string& valueValue) const {
	CAF_CM_FUNCNAME("parseIniFileValuePath");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(valuePath);

		const std::string::size_type delimPos = valuePath.find_first_of("/");
		if (delimPos == std::string::npos) {
			CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_NOT_FOUND,
				"Delim not found in valuePath - valuePath: %s", valuePath.c_str());
		}

		valueName = valuePath.substr(0, delimPos);
		valueValue = valuePath.substr(delimPos + 1);

		CAF_CM_LOG_DEBUG_VA3("Parsed valuePath - valuePath: %s, valueName: %s, valueValue: %s",
			valuePath.c_str(), valueName.c_str(), valueValue.c_str());
	}
	CAF_CM_EXIT;
}

void CConfigProvider::parseKeyPath(
	const std::string& keyPath,
	std::deque<std::string>& keyPathCollection,
	std::string& keyName) const {
	CAF_CM_FUNCNAME_VALIDATE("parseKeyPath");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(keyPath);

		std::string token;
		std::stringstream stream(keyPath);
		while(std::getline(stream, token, _keyPathDelimChar)) {
			keyPathCollection.push_back(token);
		}

		keyName = keyPathCollection.back();
		keyPathCollection.pop_back();
	}
	CAF_CM_EXIT;
}

SmartPtrCXmlElement CConfigProvider::findXmlElement(
	const std::deque<std::string>& keyPathCollection,
	const SmartPtrCXmlElement& rootXml) const {
	CAF_CM_FUNCNAME("findXmlElement");

	SmartPtrCXmlElement xmlRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STL(keyPathCollection);
		CAF_CM_VALIDATE_SMARTPTR(rootXml);

		xmlRc = rootXml;
		for (std::deque<std::string>::const_iterator keyPathIter = keyPathCollection.begin();
			keyPathIter != keyPathCollection.end();
			keyPathIter++) {
			const std::string keyPath = *keyPathIter;

			if (keyPathIter == keyPathCollection.begin()) {
				if (xmlRc->getName().compare(keyPath) != 0) {
					CAF_CM_EXCEPTIONEX_VA2(NoSuchElementException, ERROR_NOT_FOUND,
						"Root element does not match - %s != %s", keyPath.c_str(), xmlRc->getName().c_str());
				}
			} else {
				xmlRc = xmlRc->findRequiredChild(keyPath);
			}
		}
	}
	CAF_CM_EXIT;

	return xmlRc;
}
