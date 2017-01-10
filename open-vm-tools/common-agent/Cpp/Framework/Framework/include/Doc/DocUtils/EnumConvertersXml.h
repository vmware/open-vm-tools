/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef EnumConvertersXml_h_
#define EnumConvertersXml_h_

#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"
#include "Doc/CafInstallRequestDoc/CafInstallRequestDocTypes.h"
#include "Doc/SchemaTypesDoc/SchemaTypesDocTypes.h"

namespace Caf {

	namespace EnumConvertersXml {
		/// Converts the parameter type to a string
		std::string DOCUTILS_LINKAGE convertParameterTypeToString(
			const PARAMETER_TYPE parameterType);

		/// Converts the string to a parameter type
		PARAMETER_TYPE DOCUTILS_LINKAGE convertStringToParameterType(
			const std::string parameterType);

		/// Converts the property type to a string
		std::string DOCUTILS_LINKAGE convertPropertyTypeToString(
			const PROPERTY_TYPE propertyType);

		/// Converts the string to a property type
		PROPERTY_TYPE DOCUTILS_LINKAGE convertStringToPropertyType(
			const std::string propertyType);

		/// Converts the validator type to a string
		std::string DOCUTILS_LINKAGE convertValidatorTypeToString(
			const VALIDATOR_TYPE validatorType);

		/// Converts the string to a validator type
		VALIDATOR_TYPE DOCUTILS_LINKAGE convertStringToValidatorType(
			const std::string validatorType);

		/// Converts the operator type to a string
		std::string DOCUTILS_LINKAGE convertOperatorTypeToString(
			const OPERATOR_TYPE operatorType);

		/// Converts the string to a operator type
		OPERATOR_TYPE DOCUTILS_LINKAGE convertStringToOperatorType(
			const std::string operatorType);

		/// Converts the arity type to a string
		std::string DOCUTILS_LINKAGE convertArityTypeToString(
			const ARITY_TYPE arityType);

		/// Converts the string to an arity type
		ARITY_TYPE DOCUTILS_LINKAGE convertStringToArityType(
			const std::string arityType);

		/// Converts the parameter type to a string
		std::string DOCUTILS_LINKAGE convertPackageOSTypeToString(
			const PACKAGE_OS_TYPE packageOSType);

		/// Converts the string to a parameter type
		PACKAGE_OS_TYPE DOCUTILS_LINKAGE convertStringToPackageOSType(
			const std::string packageOSType);

		/// Converts the logging level enum type to a string
		std::string DOCUTILS_LINKAGE convertLoggingLevelTypeToString(
			const LOGGINGLEVEL_TYPE loggingLevelType);

		/// Converts the string to a logging level enum
		LOGGINGLEVEL_TYPE DOCUTILS_LINKAGE convertStringToLoggingLevelType(
			const std::string loggingLevelType);

		/// Converts the logging component enum type to a string
		std::string DOCUTILS_LINKAGE convertLoggingComponentTypeToString(
			const LOGGINGCOMPONENT_TYPE loggingComponentType);

		/// Converts the string to a logging component enum type
		LOGGINGCOMPONENT_TYPE DOCUTILS_LINKAGE convertStringToLoggingComponentType(
			const std::string loggingComponentType);

		std::string DOCUTILS_LINKAGE convertCmsPolicyToString(
			const CMS_POLICY cmsPolicy);

		CMS_POLICY DOCUTILS_LINKAGE convertStringToCmsPolicy(
			const std::string cmsPolicy);
	}
}

#endif
