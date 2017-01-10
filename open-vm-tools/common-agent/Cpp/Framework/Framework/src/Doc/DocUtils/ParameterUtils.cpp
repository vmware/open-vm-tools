/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CParameterCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestInstanceParameterDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestParameterDoc.h"
#include "Exception/CCafException.h"
#include "ParameterUtils.h"

using namespace Caf;

SmartPtrCRequestParameterDoc ParameterUtils::findOptionalParameter(
	const std::string& parameterName,
	const SmartPtrCParameterCollectionDoc& parameterCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("ParameterUtils", "findOptionalParameter");

	SmartPtrCRequestParameterDoc parameterRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(parameterName);
		CAF_CM_VALIDATE_SMARTPTR(parameterCollection);

		const std::deque<SmartPtrCRequestParameterDoc> parameterCollectionInner =
			parameterCollection->getParameter();
		for (TConstIterator<std::deque<SmartPtrCRequestParameterDoc> > parameterIter(parameterCollectionInner);
			parameterIter; parameterIter++) {
			const SmartPtrCRequestParameterDoc parameterTmp = *parameterIter;
			const std::string parameterNameTmp = parameterTmp->getName();
			if (parameterNameTmp.compare(parameterName) == 0) {
				parameterRc = parameterTmp;
			}
		}
	}
	CAF_CM_EXIT;

	return parameterRc;
}

SmartPtrCRequestParameterDoc ParameterUtils::findRequiredParameter(
	const std::string& parameterName,
	const SmartPtrCParameterCollectionDoc& parameterCollection) {
	CAF_CM_STATIC_FUNC_LOG("ParameterUtils", "findRequiredParameter");

	SmartPtrCRequestParameterDoc parameterRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(parameterName);
		CAF_CM_VALIDATE_SMARTPTR(parameterCollection);

		parameterRc = findOptionalParameter(parameterName, parameterCollection);
		if (parameterRc.IsNull()) {
			CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_NOT_FOUND,
				"Required parameter not found - %s", parameterName.c_str());
		}
	}
	CAF_CM_EXIT;

	return parameterRc;
}

std::deque<std::string> ParameterUtils::findOptionalParameterAsStringCollection(
	const std::string& parameterName,
	const SmartPtrCParameterCollectionDoc& parameterCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("ParameterUtils", "findOptionalParameterAsStringCollection");

	std::deque<std::string> parameterValueCollectionRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(parameterName);
		CAF_CM_VALIDATE_SMARTPTR(parameterCollection);

		const SmartPtrCRequestParameterDoc parameter = findOptionalParameter(parameterName, parameterCollection);
		if (! parameter.IsNull()) {
			parameterValueCollectionRc = parameter->getValue();
		}
	}
	CAF_CM_EXIT;

	return parameterValueCollectionRc;
}

std::deque<std::string> ParameterUtils::findRequiredParameterAsStringCollection(
	const std::string& parameterName,
	const SmartPtrCParameterCollectionDoc& parameterCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("ParameterUtils", "findRequiredParameterAsStringCollection");

	std::deque<std::string> parameterValueCollectionRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(parameterName);
		CAF_CM_VALIDATE_SMARTPTR(parameterCollection);

		const SmartPtrCRequestParameterDoc parameter = findRequiredParameter(parameterName, parameterCollection);
		parameterValueCollectionRc = parameter->getValue();
	}
	CAF_CM_EXIT;

	return parameterValueCollectionRc;
}

std::string ParameterUtils::findOptionalParameterAsString(
	const std::string& parameterName,
	const SmartPtrCParameterCollectionDoc& parameterCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("ParameterUtils", "findOptionalParameterAsString");

	std::string parameterValueRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(parameterName);
		CAF_CM_VALIDATE_SMARTPTR(parameterCollection);

		const std::deque<std::string> parameterValueCollection =
			findOptionalParameterAsStringCollection(parameterName, parameterCollection);
		if (parameterValueCollection.size() == 1) {
			parameterValueRc = parameterValueCollection.front();
		}
	}
	CAF_CM_EXIT;

	return parameterValueRc;
}

std::string ParameterUtils::findRequiredParameterAsString(
	const std::string& parameterName,
	const SmartPtrCParameterCollectionDoc& parameterCollection) {
	CAF_CM_STATIC_FUNC_LOG("ParameterUtils", "findRequiredParameterAsString");

	std::string parameterValueRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(parameterName);
		CAF_CM_VALIDATE_SMARTPTR(parameterCollection);

		const std::deque<std::string> parameterValueCollection =
			findOptionalParameterAsStringCollection(parameterName, parameterCollection);
		if (parameterValueCollection.size() != 1) {
			CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_NOT_FOUND,
				"Required string parameter not found - %s", parameterName.c_str());
		}

		parameterValueRc = parameterValueCollection.front();
	}
	CAF_CM_EXIT;

	return parameterValueRc;
}

SmartPtrCRequestInstanceParameterDoc ParameterUtils::findOptionalInstanceParameter(
	const std::string& parameterName,
	const SmartPtrCParameterCollectionDoc& parameterCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("ParameterUtils", "findOptionalInstanceParameter");

	SmartPtrCRequestInstanceParameterDoc parameterRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(parameterName);
		CAF_CM_VALIDATE_SMARTPTR(parameterCollection);

		const std::deque<SmartPtrCRequestInstanceParameterDoc> parameterCollectionInner =
			parameterCollection->getInstanceParameter();
		for (TConstIterator<std::deque<SmartPtrCRequestInstanceParameterDoc> > parameterIter(parameterCollectionInner);
			parameterIter; parameterIter++) {
			const SmartPtrCRequestInstanceParameterDoc parameterTmp = *parameterIter;
			const std::string parameterNameTmp = parameterTmp->getName();
			if (parameterNameTmp.compare(parameterName) == 0) {
				parameterRc = parameterTmp;
			}
		}
	}
	CAF_CM_EXIT;

	return parameterRc;
}

SmartPtrCRequestInstanceParameterDoc ParameterUtils::findRequiredInstanceParameter(
	const std::string& parameterName,
	const SmartPtrCParameterCollectionDoc& parameterCollection) {
	CAF_CM_STATIC_FUNC_LOG("ParameterUtils", "findRequiredInstanceParameter");

	SmartPtrCRequestInstanceParameterDoc parameterRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(parameterName);
		CAF_CM_VALIDATE_SMARTPTR(parameterCollection);

		parameterRc = findOptionalInstanceParameter(parameterName, parameterCollection);
		if (parameterRc.IsNull()) {
			CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_NOT_FOUND,
				"Required parameter not found - %s", parameterName.c_str());
		}
	}
	CAF_CM_EXIT;

	return parameterRc;
}

std::deque<std::string> ParameterUtils::findOptionalInstanceParameterAsStringCollection(
	const std::string& parameterName,
	const SmartPtrCParameterCollectionDoc& parameterCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("ParameterUtils", "findOptionalInstanceParameterAsStringCollection");

	std::deque<std::string> parameterValueCollectionRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(parameterName);
		CAF_CM_VALIDATE_SMARTPTR(parameterCollection);

		const SmartPtrCRequestInstanceParameterDoc parameter = findOptionalInstanceParameter(parameterName, parameterCollection);
		if (! parameter.IsNull()) {
			parameterValueCollectionRc = parameter->getValue();
		}
	}
	CAF_CM_EXIT;

	return parameterValueCollectionRc;
}

std::deque<std::string> ParameterUtils::findRequiredInstanceParameterAsStringCollection(
	const std::string& parameterName,
	const SmartPtrCParameterCollectionDoc& parameterCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("ParameterUtils", "findRequiredInstanceParameterAsStringCollection");

	std::deque<std::string> parameterValueCollectionRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(parameterName);
		CAF_CM_VALIDATE_SMARTPTR(parameterCollection);

		const SmartPtrCRequestInstanceParameterDoc parameter = findRequiredInstanceParameter(parameterName, parameterCollection);
		parameterValueCollectionRc = parameter->getValue();
	}
	CAF_CM_EXIT;

	return parameterValueCollectionRc;
}

std::string ParameterUtils::findOptionalInstanceParameterAsString(
	const std::string& parameterName,
	const SmartPtrCParameterCollectionDoc& parameterCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("ParameterUtils", "findOptionalInstanceParameterAsString");

	std::string parameterValueRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(parameterName);
		CAF_CM_VALIDATE_SMARTPTR(parameterCollection);

		const std::deque<std::string> parameterValueCollection =
			findOptionalInstanceParameterAsStringCollection(parameterName, parameterCollection);
		if (parameterValueCollection.size() == 1) {
			parameterValueRc = parameterValueCollection.front();
		}
	}
	CAF_CM_EXIT;

	return parameterValueRc;
}

std::string ParameterUtils::findRequiredInstanceParameterAsString(
	const std::string& parameterName,
	const SmartPtrCParameterCollectionDoc& parameterCollection) {
	CAF_CM_STATIC_FUNC_LOG("ParameterUtils", "findRequiredInstanceParameterAsString");

	std::string parameterValueRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(parameterName);
		CAF_CM_VALIDATE_SMARTPTR(parameterCollection);

		const std::deque<std::string> parameterValueCollection =
			findOptionalInstanceParameterAsStringCollection(parameterName, parameterCollection);
		if (parameterValueCollection.size() != 1) {
			CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_NOT_FOUND,
				"Required string parameter not found - %s", parameterName.c_str());
		}

		parameterValueRc = parameterValueCollection.front();
	}
	CAF_CM_EXIT;

	return parameterValueRc;
}

SmartPtrCRequestParameterDoc ParameterUtils::createParameter(
	const std::string& name,
	const std::string& value) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("ParameterUtils", "createParameter");

	SmartPtrCRequestParameterDoc parameter;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(name);
		CAF_CM_VALIDATE_STRING(value);

		std::deque<std::string> valueCollection;
		valueCollection.push_back(value);

		parameter.CreateInstance();
		parameter->initialize(
			name,
			PARAMETER_STRING,
			valueCollection);
	}
	CAF_CM_EXIT;

	return parameter;
}

SmartPtrCRequestParameterDoc ParameterUtils::createParameter(
	const std::string& name,
	const std::deque<std::string>& valueCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("ParameterUtils", "createParameter");

	SmartPtrCRequestParameterDoc parameter;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(name);
		CAF_CM_VALIDATE_STL(valueCollection);

		parameter.CreateInstance();
		parameter->initialize(
			name,
			PARAMETER_STRING,
			valueCollection);
	}
	CAF_CM_EXIT;

	return parameter;
}
