/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef ParameterUtils_h_
#define ParameterUtils_h_

namespace Caf {
class DOCUTILS_LINKAGE ParameterUtils {
public:
	static SmartPtrCRequestParameterDoc findOptionalParameter(
		const std::string& parameterName,
		const SmartPtrCParameterCollectionDoc& parameterCollection);

	static SmartPtrCRequestParameterDoc findRequiredParameter(
		const std::string& parameterName,
		const SmartPtrCParameterCollectionDoc& parameterCollection);

	static std::deque<std::string> findOptionalParameterAsStringCollection(
		const std::string& parameterName,
		const SmartPtrCParameterCollectionDoc& parameterCollection);

	static std::deque<std::string> findRequiredParameterAsStringCollection(
		const std::string& parameterName,
		const SmartPtrCParameterCollectionDoc& parameterCollection);

	static std::string findOptionalParameterAsString(
		const std::string& parameterName,
		const SmartPtrCParameterCollectionDoc& parameterCollection);

	static std::string findRequiredParameterAsString(
		const std::string& parameterName,
		const SmartPtrCParameterCollectionDoc& parameterCollection);

public:
	static SmartPtrCRequestInstanceParameterDoc findOptionalInstanceParameter(
		const std::string& parameterName,
		const SmartPtrCParameterCollectionDoc& parameterCollection);

	static SmartPtrCRequestInstanceParameterDoc findRequiredInstanceParameter(
		const std::string& parameterName,
		const SmartPtrCParameterCollectionDoc& parameterCollection);

	static std::deque<std::string> findOptionalInstanceParameterAsStringCollection(
		const std::string& parameterName,
		const SmartPtrCParameterCollectionDoc& parameterCollection);

	static std::deque<std::string> findRequiredInstanceParameterAsStringCollection(
		const std::string& parameterName,
		const SmartPtrCParameterCollectionDoc& parameterCollection);

	static std::string findOptionalInstanceParameterAsString(
		const std::string& parameterName,
		const SmartPtrCParameterCollectionDoc& parameterCollection);

	static std::string findRequiredInstanceParameterAsString(
		const std::string& parameterName,
		const SmartPtrCParameterCollectionDoc& parameterCollection);

public:
	static SmartPtrCRequestParameterDoc createParameter(
		const std::string& name,
		const std::string& value);

	static SmartPtrCRequestParameterDoc createParameter(
		const std::string& name,
		const std::deque<std::string>& valueCollection);

private:
	CAF_CM_DECLARE_NOCREATE(ParameterUtils);
};

}

#endif
