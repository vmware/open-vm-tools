/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Doc/DocUtils/EnumConvertersXml.h"
#include "Doc/CafInstallRequestDoc/CafInstallRequestDocTypes.h"
#include "Doc/SchemaTypesDoc/SchemaTypesDocTypes.h"
#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"
#include "Exception/CCafException.h"

using namespace Caf;

std::string EnumConvertersXml::convertParameterTypeToString(
	const PARAMETER_TYPE parameterType) {
	CAF_CM_STATIC_FUNC_LOG("EnumConvertersXml", "convertParameterTypeToString");

	std::string rc;

	CAF_CM_ENTER {
		switch(parameterType) {
			case PARAMETER_NONE:
				CAF_CM_LOG_DEBUG_VA0("Parameter not set");
			break;
			case PARAMETER_STRING:
				rc = "String";
			break;
			case PARAMETER_SINT32:
				rc = "SInt32";
			break;
			case PARAMETER_UINT32:
				rc = "UInt32";
			break;
			case PARAMETER_SINT64:
				rc = "SInt64";
			break;
			case PARAMETER_UINT64:
				rc = "UInt64";
			break;
			case PARAMETER_DECIMAL:
				rc = "Decimal";
			break;
			case PARAMETER_DOUBLE:
				rc = "Double";
			break;
			case PARAMETER_BOOLEAN:
				rc = "Boolean";
			break;
			case PARAMETER_DATETIME:
				rc = "DateTime";
			break;
			default:
				CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
					"Unknown parameter type - %d", static_cast<int32>(parameterType));
		}
	}
	CAF_CM_EXIT;

	return rc;
}

PARAMETER_TYPE EnumConvertersXml::convertStringToParameterType(
	const std::string parameterType) {
	CAF_CM_STATIC_FUNC("EnumConvertersXml", "convertStringToParameterType");

	PARAMETER_TYPE rc = PARAMETER_NONE;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(parameterType);

		if (parameterType.compare("String") == 0) {
			rc = PARAMETER_STRING;
		} else if (parameterType.compare("SInt32") == 0) {
			rc = PARAMETER_SINT32;
		} else if (parameterType.compare("UInt32") == 0) {
			rc = PARAMETER_UINT32;
		} else if (parameterType.compare("SInt64") == 0) {
			rc = PARAMETER_SINT64;
		} else if (parameterType.compare("UInt64") == 0) {
			rc = PARAMETER_UINT64;
		} else if (parameterType.compare("Decimal") == 0) {
			rc = PARAMETER_DECIMAL;
		} else if (parameterType.compare("Double") == 0) {
			rc = PARAMETER_DOUBLE;
		} else if (parameterType.compare("Boolean") == 0) {
			rc = PARAMETER_BOOLEAN;
		} else if (parameterType.compare("DateTime") == 0) {
			rc = PARAMETER_DATETIME;
		} else {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Unknown parameter type - %s", parameterType.c_str());
		}
	}
	CAF_CM_EXIT;

	return rc;
}

std::string EnumConvertersXml::convertPropertyTypeToString(
	const PROPERTY_TYPE propertyType) {
	CAF_CM_STATIC_FUNC_LOG("EnumConvertersXml", "convertPropertyTypeToString");

	std::string rc;

	CAF_CM_ENTER {
		switch(propertyType) {
			case PROPERTY_NONE:
				CAF_CM_LOG_DEBUG_VA0("Property not set");
			break;
			case PROPERTY_STRING:
				rc = "String";
			break;
			case PROPERTY_SINT32:
				rc = "SInt32";
			break;
			case PROPERTY_UINT32:
				rc = "UInt32";
			break;
			case PROPERTY_SINT64:
				rc = "SInt64";
			break;
			case PROPERTY_UINT64:
				rc = "UInt64";
			break;
			case PROPERTY_DECIMAL:
				rc = "Decimal";
			break;
			case PROPERTY_DOUBLE:
				rc = "Double";
			break;
			case PROPERTY_BOOLEAN:
				rc = "Boolean";
			break;
			case PROPERTY_DATETIME:
				rc = "DateTime";
			break;
			default:
				CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
					"Unknown property type - %d", static_cast<int32>(propertyType));
		}
	}
	CAF_CM_EXIT;

	return rc;
}

PROPERTY_TYPE EnumConvertersXml::convertStringToPropertyType(
	const std::string propertyType) {
	CAF_CM_STATIC_FUNC("EnumConvertersXml", "convertStringToPropertyType");

	PROPERTY_TYPE rc = PROPERTY_NONE;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(propertyType);

		if (propertyType.compare("String") == 0) {
			rc = PROPERTY_STRING;
		} else if (propertyType.compare("SInt32") == 0) {
			rc = PROPERTY_SINT32;
		} else if (propertyType.compare("UInt32") == 0) {
			rc = PROPERTY_UINT32;
		} else if (propertyType.compare("SInt64") == 0) {
			rc = PROPERTY_SINT64;
		} else if (propertyType.compare("UInt64") == 0) {
			rc = PROPERTY_UINT64;
		} else if (propertyType.compare("Decimal") == 0) {
			rc = PROPERTY_DECIMAL;
		} else if (propertyType.compare("Double") == 0) {
			rc = PROPERTY_DOUBLE;
		} else if (propertyType.compare("Boolean") == 0) {
			rc = PROPERTY_BOOLEAN;
		} else if (propertyType.compare("DateTime") == 0) {
			rc = PROPERTY_DATETIME;
		} else {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Unknown property type - %s", propertyType.c_str());
		}
	}
	CAF_CM_EXIT;

	return rc;
}

std::string EnumConvertersXml::convertValidatorTypeToString(
	const VALIDATOR_TYPE validatorType) {
	CAF_CM_STATIC_FUNC_LOG("EnumConvertersXml", "convertValidatorTypeToString");

	std::string rc;

	CAF_CM_ENTER {
		switch(validatorType) {
			case VALIDATOR_NONE:
			break;
			case VALIDATOR_ENUM:
				rc = "enum";
			break;
			case VALIDATOR_RANGE:
				rc = "range";
			break;
			case VALIDATOR_REGEX:
				rc = "regex";
			break;
			case VALIDATOR_CUSTOM:
				rc = "custom";
			break;
			default:
				CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
					"Unknown validator type - %d", static_cast<int32>(validatorType));
		}
	}
	CAF_CM_EXIT;

	return rc;
}

VALIDATOR_TYPE EnumConvertersXml::convertStringToValidatorType(
	const std::string validatorType) {
	CAF_CM_STATIC_FUNC("EnumConvertersXml", "convertStringToValidatorType");

	VALIDATOR_TYPE rc = VALIDATOR_NONE;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(validatorType);

		if (validatorType.compare("enum") == 0) {
			rc = VALIDATOR_ENUM;
		} else if (validatorType.compare("range") == 0) {
			rc = VALIDATOR_RANGE;
		} else if (validatorType.compare("regex") == 0) {
			rc = VALIDATOR_REGEX;
		} else if (validatorType.compare("custom") == 0) {
			rc = VALIDATOR_CUSTOM;
		} else {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Unknown validator type - %s", validatorType.c_str());
		}
	}
	CAF_CM_EXIT;

	return rc;
}

std::string EnumConvertersXml::convertOperatorTypeToString(
	const OPERATOR_TYPE operatorType) {
	CAF_CM_STATIC_FUNC_LOG("EnumConvertersXml", "convertOperatorTypeToString");

	std::string rc;

	CAF_CM_ENTER {
		switch(operatorType) {
			case OPERATOR_NONE:
				CAF_CM_LOG_DEBUG_VA0("Operator not set");
			break;
			case OPERATOR_EQUAL:
				rc = "=";
			break;
			default:
				CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
					"Unknown operator type - %d", static_cast<int32>(operatorType));
		}
	}
	CAF_CM_EXIT;

	return rc;
}

OPERATOR_TYPE EnumConvertersXml::convertStringToOperatorType(
	const std::string operatorType) {
	CAF_CM_STATIC_FUNC("EnumConvertersXml", "convertStringToOperatorType");

	OPERATOR_TYPE rc = OPERATOR_NONE;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(operatorType);

		if (operatorType.compare("=") == 0) {
			rc = OPERATOR_EQUAL;
		} else {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Unknown operator type - %s", operatorType.c_str());
		}
	}
	CAF_CM_EXIT;

	return rc;
}

std::string EnumConvertersXml::convertArityTypeToString(
	const ARITY_TYPE arityType) {
	CAF_CM_STATIC_FUNC_LOG("EnumConvertersXml", "convertArityTypeToString");

	std::string rc;

	CAF_CM_ENTER {
		switch(arityType) {
			case ARITY_NONE:
				CAF_CM_LOG_DEBUG_VA0("Arity not set");
			break;
			case ARITY_UNSIGNED_BYTE:
				rc = "2";
			break;
			default:
				CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
					"Unknown arity type - %d", static_cast<int32>(arityType));
		}
	}
	CAF_CM_EXIT;

	return rc;
}

ARITY_TYPE EnumConvertersXml::convertStringToArityType(
	const std::string arityType) {
	CAF_CM_STATIC_FUNC("EnumConvertersXml", "convertStringToArityType");

	ARITY_TYPE rc = ARITY_NONE;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(arityType);

		if (arityType.compare("2") == 0) {
			rc = ARITY_UNSIGNED_BYTE;
		} else {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Unknown arity type - %s", arityType.c_str());
		}
	}
	CAF_CM_EXIT;

	return rc;
}

std::string EnumConvertersXml::convertPackageOSTypeToString(
	const PACKAGE_OS_TYPE packageOSType) {
	CAF_CM_STATIC_FUNC_LOG("EnumConvertersXml", "convertPackageOSTypeToString");

	std::string rc;

	CAF_CM_ENTER {
		switch(packageOSType) {
			case PACKAGE_OS_NONE:
				CAF_CM_LOG_DEBUG_VA0("Package OS not set");
			break;
			case PACKAGE_OS_ALL:
				rc = "All";
			break;
			case PACKAGE_OS_NIX:
				rc = "Nix";
			break;
			case PACKAGE_OS_WIN:
				rc = "Win";
			break;
			default:
				CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
					"Unknown Package OS type - %d", static_cast<int32>(packageOSType));
		}
	}
	CAF_CM_EXIT;

	return rc;
}

PACKAGE_OS_TYPE EnumConvertersXml::convertStringToPackageOSType(
	const std::string packageOSType) {
	CAF_CM_STATIC_FUNC("EnumConvertersXml", "convertStringToPackageOSType");

	PACKAGE_OS_TYPE rc = PACKAGE_OS_NONE;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(packageOSType);

		if (packageOSType.compare("All") == 0) {
			rc = PACKAGE_OS_ALL;
		} else if (packageOSType.compare("Nix") == 0) {
			rc = PACKAGE_OS_NIX;
		} else if (packageOSType.compare("Win") == 0) {
			rc = PACKAGE_OS_WIN;
		} else {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Unknown Package OS type - %s", packageOSType.c_str());
		}
	}
	CAF_CM_EXIT;

	return rc;
}

std::string EnumConvertersXml::convertLoggingLevelTypeToString(
	const LOGGINGLEVEL_TYPE loggingLevelType) {
	CAF_CM_STATIC_FUNC_LOG("EnumConvertersXml", "convertLoggingLevelTypeToString");

	std::string rc;

	CAF_CM_ENTER {
		switch(loggingLevelType) {
			case LOGGINGLEVEL_NONE:
				CAF_CM_LOG_DEBUG_VA0("Logging level not set");
			break;
			case LOGGINGLEVEL_DEBUG:
				rc = "DEBUG";
			break;
			case LOGGINGLEVEL_INFO:
				rc = "INFO";
			break;
			case LOGGINGLEVEL_WARN:
				rc = "WARN";
			break;
			case LOGGINGLEVEL_ERROR:
				rc = "ERROR";
			break;
			case LOGGINGLEVEL_CRITICAL:
				rc = "CRITICAL";
			break;
			default:
				CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
					"Unknown logging level type - %d", static_cast<int32>(loggingLevelType));
		}
	}
	CAF_CM_EXIT;

	return rc;
}

LOGGINGLEVEL_TYPE EnumConvertersXml::convertStringToLoggingLevelType(
	const std::string loggingLevelType) {
	CAF_CM_STATIC_FUNC("EnumConvertersXml", "convertStringToLoggingLevelType");

	LOGGINGLEVEL_TYPE rc = LOGGINGLEVEL_NONE;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(loggingLevelType);

		if (loggingLevelType.compare("DEBUG") == 0) {
			rc = LOGGINGLEVEL_DEBUG;
		} else if (loggingLevelType.compare("INFO") == 0) {
			rc = LOGGINGLEVEL_INFO;
		} else if (loggingLevelType.compare("WARN") == 0) {
			rc = LOGGINGLEVEL_WARN;
		} else if (loggingLevelType.compare("ERROR") == 0) {
			rc = LOGGINGLEVEL_ERROR;
		} else if (loggingLevelType.compare("CRITICAL") == 0) {
			rc = LOGGINGLEVEL_CRITICAL;
		} else {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Unknown logging level type - %s", loggingLevelType.c_str());
		}
	}
	CAF_CM_EXIT;

	return rc;
}

std::string EnumConvertersXml::convertLoggingComponentTypeToString(
	const LOGGINGCOMPONENT_TYPE loggingComponentType) {
	CAF_CM_STATIC_FUNC_LOG("EnumConvertersXml", "convertLoggingComponentTypeToString");

	std::string rc;

	CAF_CM_ENTER {
		switch(loggingComponentType) {
			case LOGGINGCOMPONENT_NONE:
				CAF_CM_LOG_DEBUG_VA0("Logging component not set");
			break;
			case LOGGINGCOMPONENT_COMMUNICATIONS:
				rc = "Communications";
			break;
			case LOGGINGCOMPONENT_MANAGEMENTAGENT:
				rc = "ManagementAgent";
			break;
			case LOGGINGCOMPONENT_PROVIDERFRAMEWORK:
				rc = "ProviderFramework";
			break;
			case LOGGINGCOMPONENT_PROVIDER:
				rc = "Provider";
			break;
			default:
				CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
					"Unknown logging component type - %d", static_cast<int32>(loggingComponentType));
		}
	}
	CAF_CM_EXIT;

	return rc;
}

LOGGINGCOMPONENT_TYPE EnumConvertersXml::convertStringToLoggingComponentType(
	const std::string loggingComponentType) {
	CAF_CM_STATIC_FUNC("EnumConvertersXml", "convertStringToLoggingComponentType");

	LOGGINGCOMPONENT_TYPE rc = LOGGINGCOMPONENT_NONE;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(loggingComponentType);

		if (loggingComponentType.compare("Communications") == 0) {
			rc = LOGGINGCOMPONENT_COMMUNICATIONS;
		} else if (loggingComponentType.compare("ManagementAgent") == 0) {
			rc = LOGGINGCOMPONENT_MANAGEMENTAGENT;
		} else if (loggingComponentType.compare("ProviderFramework") == 0) {
			rc = LOGGINGCOMPONENT_PROVIDERFRAMEWORK;
		} else if (loggingComponentType.compare("Provider") == 0) {
			rc = LOGGINGCOMPONENT_PROVIDER;
		} else {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Unknown logging component type - %s", loggingComponentType.c_str());
		}
	}
	CAF_CM_EXIT;

	return rc;
}

std::string EnumConvertersXml::convertCmsPolicyToString(
	const CMS_POLICY cmsPolicy) {
	CAF_CM_STATIC_FUNC_LOG("EnumConvertersXml", "convertCmsPolicyToString");

	std::string rc;

	CAF_CM_ENTER {
		switch(cmsPolicy) {
			case CMS_POLICY_NONE:
				rc = "None";
			break;
			case CMS_POLICY_CAF_ENCRYPTED:
				rc = "CAF_Encrypted";
			break;
			case CMS_POLICY_CAF_SIGNED:
				rc = "CAF_Signed";
			break;
			case CMS_POLICY_CAF_ENCRYPTED_AND_SIGNED:
				rc = "CAF_Encrypted_And_Signed";
			break;
			case CMS_POLICY_APP_ENCRYPTED:
				rc = "App_Encrypted";
			break;
			case CMS_POLICY_APP_SIGNED:
				rc = "App_Signed";
			break;
			case CMS_POLICY_APP_ENCRYPTED_AND_SIGNED:
				rc = "App_Encrypted_And_Signed";
			break;
			default:
				CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
					"Unknown CMS Policy - %d", static_cast<int32>(cmsPolicy));
		}
	}
	CAF_CM_EXIT;

	return rc;
}

CMS_POLICY EnumConvertersXml::convertStringToCmsPolicy(
	const std::string cmsPolicy) {
	CAF_CM_STATIC_FUNC("EnumConvertersXml", "convertStringToCmsPolicy");

	CMS_POLICY rc = CMS_POLICY_NONE;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(cmsPolicy);

		if (cmsPolicy.compare("None") == 0) {
			rc = CMS_POLICY_NONE;
		} else if (cmsPolicy.compare("CAF_Encrypted") == 0) {
			rc = CMS_POLICY_CAF_ENCRYPTED;
		} else if (cmsPolicy.compare("CAF_Signed") == 0) {
			rc = CMS_POLICY_CAF_SIGNED;
		} else if (cmsPolicy.compare("CAF_Encrypted_And_Signed") == 0) {
			rc = CMS_POLICY_CAF_ENCRYPTED_AND_SIGNED;
		} else if (cmsPolicy.compare("App_Encrypted") == 0) {
			rc = CMS_POLICY_APP_ENCRYPTED;
		} else if (cmsPolicy.compare("App_Signed") == 0) {
			rc = CMS_POLICY_APP_SIGNED;
		} else if (cmsPolicy.compare("App_Encrypted_And_Signed") == 0) {
			rc = CMS_POLICY_APP_ENCRYPTED_AND_SIGNED;
		} else {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Unknown CMS Policy - %s", cmsPolicy.c_str());
		}
	}
	CAF_CM_EXIT;

	return rc;
}
