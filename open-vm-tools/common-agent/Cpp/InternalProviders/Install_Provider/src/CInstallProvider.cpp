/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Doc/CafInstallRequestDoc/CafInstallRequestDocTypes.h"

#include "CProviderInstaller.h"
#include "Doc/CafCoreTypesDoc/COperationDoc.h"
#include "Doc/CafCoreTypesDoc/CParameterCollectionDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallProviderJobDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallProviderSpecDoc.h"
#include "Doc/CafInstallRequestDoc/CMinPackageElemDoc.h"
#include "Doc/CafInstallRequestDoc/CUninstallProviderJobDoc.h"
#include "Doc/ProviderRequestDoc/CProviderInvokeOperationDoc.h"
#include "Doc/ProviderResultsDoc/CSchemaDoc.h"
#include "Doc/SchemaTypesDoc/CActionClassDoc.h"
#include "Doc/SchemaTypesDoc/CClassInstancePropertyDoc.h"
#include "Doc/SchemaTypesDoc/CClassPropertyDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassInstanceDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassPropertyDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassSubInstanceDoc.h"
#include "Doc/SchemaTypesDoc/CInstanceParameterDoc.h"
#include "Doc/SchemaTypesDoc/CMethodDoc.h"
#include "Doc/SchemaTypesDoc/CMethodParameterDoc.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Exception/CCafException.h"
#include "CInstallProvider.h"
#include "IProviderResponse.h"
#include "IProviderRequest.h"
#include "Integration/Caf/CCafMessagePayloadParser.h"
#include "Integration/Caf/CCafMessagePayload.h"

using namespace Caf;

CInstallProvider::CInstallProvider() :
	CAF_CM_INIT_LOG("CInstallProvider") {
}

CInstallProvider::~CInstallProvider() {
}

const SmartPtrCSchemaDoc CInstallProvider::getSchema() const {
	std::deque<SmartPtrCClassPropertyDoc> dc1Props;
	dc1Props.push_back(CProviderDocHelper::createClassProperty("index", PROPERTY_UINT32, true));
	dc1Props.push_back(CProviderDocHelper::createClassProperty("packageNamespace", PROPERTY_STRING, true));
	dc1Props.push_back(CProviderDocHelper::createClassProperty("packageName", PROPERTY_STRING, true));
	dc1Props.push_back(CProviderDocHelper::createClassProperty("packageVersion", PROPERTY_STRING, true));
	dc1Props.push_back(CProviderDocHelper::createClassProperty("startupAttachmentName", PROPERTY_STRING, true));
	dc1Props.push_back(CProviderDocHelper::createClassProperty("packageAttachmentName", PROPERTY_STRING, true));
	dc1Props.push_back(CProviderDocHelper::createClassProperty("arguments", PROPERTY_STRING, true));
	dc1Props.push_back(CProviderDocHelper::createClassProperty("supportingAttachmentName", PROPERTY_STRING, false, false, true));

	std::deque<SmartPtrCClassPropertyDoc> dc2Props;
	dc2Props.push_back(CProviderDocHelper::createClassProperty("clientId", PROPERTY_STRING, true));
	dc2Props.push_back(CProviderDocHelper::createClassProperty("providerNamespace", PROPERTY_STRING, true));
	dc2Props.push_back(CProviderDocHelper::createClassProperty("providerName", PROPERTY_STRING, true));
	dc2Props.push_back(CProviderDocHelper::createClassProperty("providerVersion", PROPERTY_STRING, true));
	dc2Props.push_back(CProviderDocHelper::createClassProperty("packageOSType", PROPERTY_STRING, true));

	std::deque<SmartPtrCClassInstancePropertyDoc> instanceProperties;
	instanceProperties.push_back(CProviderDocHelper::createClassInstanceProperty(
			"fullPackageElem",
			CProviderDocHelper::createClassIdentifier("caf", "FullPackageElem", "1.0.0"),
			true,
			false,
			true));
	instanceProperties.push_back(CProviderDocHelper::createClassInstanceProperty(
			"unfullPackageElem",
			CProviderDocHelper::createClassIdentifier("caf", "FullPackageElem", "1.0.0"),
			true,
			false,
			true));

	std::deque<SmartPtrCClassPropertyDoc> dc3Props;
	dc3Props.push_back(CProviderDocHelper::createClassProperty("packageOSType", PROPERTY_STRING, true));
	dc3Props.push_back(CProviderDocHelper::createClassProperty("clientId", PROPERTY_STRING, true));
	dc3Props.push_back(CProviderDocHelper::createClassProperty("providerNamespace", PROPERTY_STRING, true));
	dc3Props.push_back(CProviderDocHelper::createClassProperty("providerName", PROPERTY_STRING, true));
	dc3Props.push_back(CProviderDocHelper::createClassProperty("providerVersion", PROPERTY_STRING, true));
	dc3Props.push_back(CProviderDocHelper::createClassProperty("packageOSType", PROPERTY_STRING, true));

	std::deque<SmartPtrCDataClassDoc> dataClasses;
	dataClasses.push_back(CProviderDocHelper::createDataClass("caf", "FullPackageElem", "1.0.0", dc1Props));
	dataClasses.push_back(CProviderDocHelper::createDataClass("caf", "InstallProviderJob", "1.0.0", dc2Props, instanceProperties));
	dataClasses.push_back(CProviderDocHelper::createDataClass("caf", "UninstallProviderJob", "1.0.0", dc3Props));

	std::deque<SmartPtrCInstanceParameterDoc> m1InstanceParams;
	m1InstanceParams.push_back(CProviderDocHelper::createInstanceParameter(
			"installProviderJob",
			"caf",
			"InstallProviderJob",
			"1.0.0",
			false));

	std::deque<SmartPtrCInstanceParameterDoc> m2InstanceParams;
	m2InstanceParams.push_back(CProviderDocHelper::createInstanceParameter(
			"uninstallProviderJob",
			"caf",
			"UninstallProviderJob",
			"1.0.0",
			false));

	std::deque<SmartPtrCMethodParameterDoc> emptyParams;

	std::deque<SmartPtrCMethodDoc> methods;
	methods.push_back(CProviderDocHelper::createMethod("installProviderJob", emptyParams, m1InstanceParams));
	methods.push_back(CProviderDocHelper::createMethod("uninstallProviderJob", emptyParams, m2InstanceParams));

	std::deque<SmartPtrCActionClassDoc> actionClasses;
	actionClasses.push_back(
			CProviderDocHelper::createActionClass(
			"caf",
			"InstallActions",
			"1.0.0",
			CProviderDocHelper::createCollectMethod("collectInstances"),
			methods));

	return CProviderDocHelper::createSchema(dataClasses, actionClasses);
}

void CInstallProvider::collect(const IProviderRequest& request, IProviderResponse& response) const {

	const CProviderInstaller::SmartPtrCInstallProviderSpecCollection installProviderSpecCollection =
		CProviderInstaller::readInstallProviderSpecs();
	if (!installProviderSpecCollection.IsNull()) {
		for (TConstIterator<std::deque<SmartPtrCInstallProviderSpecDoc> >
			installProviderSpecIter(*installProviderSpecCollection); installProviderSpecIter; installProviderSpecIter++) {
			const SmartPtrCInstallProviderSpecDoc installProviderSpec =
				*installProviderSpecIter;

			const SmartPtrCDataClassInstanceDoc dataClassInstance = createDataClassInstance(
				installProviderSpec);

			response.addInstance(dataClassInstance);
		}
	}
}

void CInstallProvider::invoke(const IProviderRequest& request, IProviderResponse& response) const {
	CAF_CM_FUNCNAME("invoke");

	SmartPtrCProviderInvokeOperationDoc doc = request.getInvokeOperations();
	CAF_CM_VALIDATE_SMARTPTR(doc);

	const SmartPtrCOperationDoc operation = doc->getOperation();
	const std::string operationName = operation->getName();

	const SmartPtrCParameterCollectionDoc parameterCollection =
		operation->getParameterCollection();

	const std::string outputDir = doc->getOutputDir();

	if (operationName.compare("installProviderJob") == 0) {
		const std::string installProviderJobStr =
			ParameterUtils::findRequiredInstanceParameterAsString("installProviderJob",
				parameterCollection);
		CAF_CM_VALIDATE_STRING(installProviderJobStr);

		const SmartPtrCDynamicByteArray payload =
				CCafMessagePayload::createBufferFromStr(installProviderJobStr);
		const SmartPtrCInstallProviderJobDoc installProviderJob =
				CCafMessagePayloadParser::getInstallProviderJob(payload);

		if (isCurrentOS(installProviderJob->getPackageOSType())) {
			CProviderInstaller::installProvider(installProviderJob, request.getAttachments(), outputDir);
		}
	} else if (operationName.compare("uninstallProviderJob") == 0) {
		const std::string uninstallProviderJobStr =
			ParameterUtils::findRequiredInstanceParameterAsString("uninstallProviderJob",
				parameterCollection);

		const SmartPtrCDynamicByteArray payload =
				CCafMessagePayload::createBufferFromStr(uninstallProviderJobStr);
		const SmartPtrCUninstallProviderJobDoc uninstallProviderJob =
				CCafMessagePayloadParser::getUninstallProviderJob(payload);

		if (isCurrentOS(uninstallProviderJob->getPackageOSType())) {
			CProviderInstaller::uninstallProvider(uninstallProviderJob, outputDir);
		}
	} else {
		CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
			"Invalid operation name (must be \'installProviderJob\' or \'uninstallProviderJob\') - %s", operationName.c_str())
	}
}

SmartPtrCDataClassInstanceDoc CInstallProvider::createDataClassInstance(
	const SmartPtrCInstallProviderSpecDoc& installProviderSpec) const {
	CAF_CM_FUNCNAME_VALIDATE("createDataClassInstance");
	CAF_CM_VALIDATE_SMARTPTR(installProviderSpec);

	const std::deque<SmartPtrCMinPackageElemDoc> minPackageElemCollection =
		installProviderSpec->getPackageCollection();

	std::deque<SmartPtrCDataClassSubInstanceDoc> subInstances;
	for (TConstIterator<std::deque<SmartPtrCMinPackageElemDoc> > minPackageElemIter(
		minPackageElemCollection); minPackageElemIter; minPackageElemIter++) {
		const SmartPtrCMinPackageElemDoc minPackageElem = *minPackageElemIter;

		const std::string packageNamespace = minPackageElem->getPackageNamespace();
		const std::string packageName = minPackageElem->getPackageName();
		const std::string packageVersion = minPackageElem->getPackageVersion();

		std::deque<SmartPtrCDataClassPropertyDoc> siProperties;
		siProperties.push_back(CProviderDocHelper::createDataClassProperty("packageNamespace", packageNamespace));
		siProperties.push_back(CProviderDocHelper::createDataClassProperty("packageName", packageName));
		siProperties.push_back(CProviderDocHelper::createDataClassProperty("packageVersion", packageVersion));
		subInstances.push_back(CProviderDocHelper::createDataClassSubInstance(
				"fullPackageElem",
				siProperties));
	}

	const std::string clientId =
		BasePlatform::UuidToString(installProviderSpec->getClientId());
	const std::string providerNamespace = installProviderSpec->getProviderNamespace();
	const std::string providerName = installProviderSpec->getProviderName();
	const std::string providerVersion = installProviderSpec->getProviderVersion();

	std::deque<SmartPtrCDataClassPropertyDoc> dataClassProperties;
	dataClassProperties.push_back(CProviderDocHelper::createDataClassProperty("clientId", clientId));
	dataClassProperties.push_back(CProviderDocHelper::createDataClassProperty("providerNamespace", providerNamespace));
	dataClassProperties.push_back(CProviderDocHelper::createDataClassProperty("providerName", providerName));
	dataClassProperties.push_back(CProviderDocHelper::createDataClassProperty("providerVersion", providerVersion));
	return CProviderDocHelper::createDataClassInstance(
			"caf",
			"CafProviderInventory",
			"1.0.0",
			dataClassProperties,
			subInstances);
}

bool CInstallProvider::isCurrentOS(const PACKAGE_OS_TYPE& packageOSType) const {
	CAF_CM_FUNCNAME_VALIDATE("isCurrentOS");

#ifdef WIN32
	const PACKAGE_OS_TYPE packageOSTypeLocal = PACKAGE_OS_WIN;
#else
	const PACKAGE_OS_TYPE packageOSTypeLocal = PACKAGE_OS_NIX;
#endif

	bool rc = false;
	if ((packageOSType == packageOSTypeLocal) || (packageOSType == PACKAGE_OS_ALL)) {
		rc = true;
	} else {
		CAF_CM_LOG_WARN_VA0("Wrong OS... Skipping package");
	}

	return rc;
}
