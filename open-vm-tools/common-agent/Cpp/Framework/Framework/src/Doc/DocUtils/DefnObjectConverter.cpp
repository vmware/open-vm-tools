/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Doc/SchemaTypesDoc/CCmdlMetadataDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassInstanceDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassPropertyDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassSubInstanceDoc.h"
#include "Integration/IDocument.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "DefnObjectConverter.h"

using namespace Caf;

std::string DefnObjectConverter::toString(
	const SmartPtrCDataClassInstanceDoc dataClassInstance) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("DefnObjectConverter", "toString");

	std::string defnObjXmlStr;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_SMARTPTR(dataClassInstance);

		const SmartPtrCXmlElement defnObjXml = CXmlUtils::createRootElement(
			dataClassInstance->getName(), "http://schemas->vmware->com/caf/schema");

		defnObjXml->addAttribute("namespace", dataClassInstance->getNamespaceVal());
		defnObjXml->addAttribute("name", dataClassInstance->getName());
		defnObjXml->addAttribute("version", dataClassInstance->getVersion());

		SmartPtrCDataClassSubInstanceDoc dataClassSubInstance;
		dataClassSubInstance.CreateInstance();
		dataClassSubInstance->initialize(
			dataClassInstance->getName(),
			dataClassInstance->getCmdlMetadataCollection(),
			dataClassInstance->getPropertyCollection(),
			dataClassInstance->getInstancePropertyCollection(),
			dataClassInstance->getCmdlUnion());

		addDataClassSubInstance(dataClassSubInstance, defnObjXml);

		defnObjXmlStr = defnObjXml->saveToStringRaw();
	}
	CAF_CM_EXIT;

	return defnObjXmlStr;
}

SmartPtrCDataClassInstanceDoc DefnObjectConverter::fromString(
	const std::string defnObjectXmlStr) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("DefnObjectConverter", "fromString");

	SmartPtrCDataClassInstanceDoc dataClassInstanceRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(defnObjectXmlStr);

		const SmartPtrCXmlElement defnObjectXml = CXmlUtils::parseString(defnObjectXmlStr, std::string());

		const SmartPtrCDataClassSubInstanceDoc dataClassSubInstance = parseDataClassSubInstance(defnObjectXml, true);

		dataClassInstanceRc.CreateInstance();
		dataClassInstanceRc->initialize(
			defnObjectXml->findRequiredAttribute("namespace"),
			defnObjectXml->findRequiredAttribute("name"),
			defnObjectXml->findRequiredAttribute("version"),
			dataClassSubInstance->getCmdlMetadataCollection(),
			dataClassSubInstance->getPropertyCollection(),
			dataClassSubInstance->getInstancePropertyCollection(),
			dataClassSubInstance->getCmdlUnion());
	}
	CAF_CM_EXIT;

	return dataClassInstanceRc;
}

void DefnObjectConverter::addDataClassSubInstance(
	const SmartPtrCDataClassSubInstanceDoc dataClassSubInstance,
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("DefnObjectConverter", "addDataClassSubInstance");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_SMARTPTR(dataClassSubInstance);
		CAF_CM_VALIDATE_SMARTPTR(thisXml);

		const std::deque<SmartPtrCCmdlMetadataDoc> cmdlMetadataCollection = dataClassSubInstance->getCmdlMetadataCollection();
		if (! cmdlMetadataCollection.empty()) {
			addCmdlMetadata(cmdlMetadataCollection, thisXml);
		}

		const std::deque<SmartPtrCDataClassPropertyDoc> dataClassPropertyCollection = dataClassSubInstance->getPropertyCollection();
		if (! dataClassPropertyCollection.empty()) {
			for (TSmartConstIterator<std::deque<SmartPtrCDataClassPropertyDoc> > dataClassPropertyIter(dataClassPropertyCollection);
				dataClassPropertyIter; dataClassPropertyIter++) {
				const SmartPtrCDataClassPropertyDoc dataClassProperty = *dataClassPropertyIter;
				const SmartPtrCXmlElement propertyXml = thisXml->createAndAddElement(dataClassProperty->getName());

				const std::deque<SmartPtrCCmdlMetadataDoc> cmdlMetadataCollectionProperty = dataClassProperty->getCmdlMetadata();
				if (! cmdlMetadataCollectionProperty.empty()) {
					addCmdlMetadata(cmdlMetadataCollectionProperty, propertyXml);
				}
				propertyXml->setValue(dataClassProperty->getValue());
			}
		}

		const std::deque<SmartPtrCDataClassSubInstanceDoc> instancePropertyCollection = dataClassSubInstance->getInstancePropertyCollection();
		if (! instancePropertyCollection.empty()) {
			for (TSmartConstIterator<std::deque<SmartPtrCDataClassSubInstanceDoc> > dataClassSubInstanceIter(instancePropertyCollection);
				dataClassSubInstanceIter; dataClassSubInstanceIter++) {
				const SmartPtrCDataClassSubInstanceDoc instanceProperty = *dataClassSubInstanceIter;
				const SmartPtrCXmlElement instancePropertyXml = thisXml->createAndAddElement(instanceProperty->getName());
				addDataClassSubInstance(instanceProperty, instancePropertyXml);
			}
		}
	}
	CAF_CM_EXIT;
}

SmartPtrCDataClassSubInstanceDoc DefnObjectConverter::parseDataClassSubInstance(
	const SmartPtrCXmlElement dataClassSubInstanceXml,
	const bool isDataClassInstance) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("DefnObjectConverter", "parseDataClassSubInstance");

	SmartPtrCDataClassSubInstanceDoc dataClassSubInstanceRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_SMARTPTR(dataClassSubInstanceXml);

		std::deque<SmartPtrCCmdlMetadataDoc> cmdlMetadataCollection;
		if (! isDataClassInstance) {
			cmdlMetadataCollection = parseCmdlMetadata(dataClassSubInstanceXml);
		}

		std::deque<SmartPtrCDataClassPropertyDoc> dataClassPropertyCollection;
		std::deque<SmartPtrCDataClassSubInstanceDoc> instancePropertyCollection;
		const CXmlElement::SmartPtrCElementCollection childrenXml = dataClassSubInstanceXml->getAllChildren();
		if (! childrenXml.IsNull() && ! childrenXml->empty()) {
			for (TConstIterator<CXmlElement::CElementCollection > childrenXmlIter(*childrenXml);
				childrenXmlIter; childrenXmlIter++) {
				const SmartPtrCXmlElement childXml = childrenXmlIter->second;

				const CXmlElement::SmartPtrCElementCollection grandChildrenXml = childXml->getAllChildren();
				if (! grandChildrenXml.IsNull() && ! grandChildrenXml->empty()) {
					const SmartPtrCDataClassSubInstanceDoc dataClassSubInstance = parseDataClassSubInstance(childXml, false);
					instancePropertyCollection.push_back(dataClassSubInstance);
				} else {
					const std::deque<SmartPtrCCmdlMetadataDoc> cmdlMetadataCollectionProperty = parseCmdlMetadata(childXml);
					SmartPtrCDataClassPropertyDoc dataClassProperty;
					dataClassProperty.CreateInstance();
					dataClassProperty->initialize(
						childXml->getName(), cmdlMetadataCollectionProperty, childXml->getValue());
					dataClassPropertyCollection.push_back(dataClassProperty);
				}
			}
		}

		dataClassSubInstanceRc.CreateInstance();
		dataClassSubInstanceRc->initialize(
			dataClassSubInstanceXml->getName(),
			cmdlMetadataCollection, dataClassPropertyCollection, instancePropertyCollection, SmartPtrCCmdlUnionDoc());
	}
	CAF_CM_EXIT;

	return dataClassSubInstanceRc;
}

std::deque<SmartPtrCCmdlMetadataDoc> DefnObjectConverter::parseCmdlMetadata(
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("DefnObjectConverter", "parseCmdlMetadata");

	std::deque<SmartPtrCCmdlMetadataDoc> cmdlMetadataCollection;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_SMARTPTR(thisXml);

		const CXmlElement::SmartPtrCAttributeCollection attributeCollection = thisXml->getAllAttributes();
		if (! attributeCollection.IsNull() && ! attributeCollection->empty()) {
			for (TConstIterator<CXmlElement::CAttributeCollection> childrenXmlIter(*attributeCollection);
				childrenXmlIter; childrenXmlIter++) {
				const std::string attributeName = childrenXmlIter->first;
				const std::string attributeValue = childrenXmlIter->second;

				SmartPtrCCmdlMetadataDoc cmdlMetadata;
				cmdlMetadata.CreateInstance();
				cmdlMetadata->initialize(attributeName, attributeValue);

				cmdlMetadataCollection.push_back(cmdlMetadata);
			}
		}
	}
	CAF_CM_EXIT;

	return cmdlMetadataCollection;
}

void DefnObjectConverter::addCmdlMetadata(
	const std::deque<SmartPtrCCmdlMetadataDoc> cmdlMetadataCollection,
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("DefnObjectConverter", "addCmdlMetadata");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STL(cmdlMetadataCollection);
		CAF_CM_VALIDATE_SMARTPTR(thisXml);

		for (TSmartConstIterator<std::deque<SmartPtrCCmdlMetadataDoc> > cmdlMetadataIter(cmdlMetadataCollection);
			cmdlMetadataIter; cmdlMetadataIter++) {
			const SmartPtrCCmdlMetadataDoc cmdlMetadata = *cmdlMetadataIter;
			thisXml->addAttribute(cmdlMetadata->getName(), cmdlMetadata->getValue());
		}
	}
	CAF_CM_EXIT;
}
