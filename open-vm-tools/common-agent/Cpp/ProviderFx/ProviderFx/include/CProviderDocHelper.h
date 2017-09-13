/*
 *	 Author: brets
 *  Created: Dec 3, 2015
 *
 *	Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CProviderDocHelper_H_
#define CProviderDocHelper_H_

namespace Caf {

class PROVIDERFX_LINKAGE CProviderDocHelper {

public:
	static SmartPtrCClassPropertyDoc createClassProperty(
			const std::string name,
			const PROPERTY_TYPE type,
			const bool required = false,
			const bool key = false,
			const bool list = false);

	static SmartPtrCClassIdentifierDoc createClassIdentifier(
			const std::string namespaceVal,
			const std::string name,
			const std::string version);

	static SmartPtrCClassInstancePropertyDoc createClassInstanceProperty(
			const std::string name,
			const SmartPtrCClassIdentifierDoc type,
			const bool required = false,
			const bool transientVal = false,
			const bool list = false);

	static SmartPtrCDataClassDoc createDataClass(
			const std::string namespaceVal,
			const std::string name,
			const std::string version,
			const std::deque<SmartPtrCClassPropertyDoc> properties,
			const std::deque<SmartPtrCClassInstancePropertyDoc> instanceProperties = std::deque<SmartPtrCClassInstancePropertyDoc>());

	static SmartPtrCMethodParameterDoc createMethodParameter(
			const std::string name,
			const PARAMETER_TYPE type,
			const bool isOptional = false,
			const bool isList = false);

	static SmartPtrCInstanceParameterDoc createInstanceParameter(
			const std::string name,
			const std::string instanceNamespace,
			const std::string instanceName,
			const std::string instanceVersion,
			const bool isOptional = false,
			const bool isList = false);

	static SmartPtrCCollectMethodDoc createCollectMethod(
			const std::string name,
			const std::deque<SmartPtrCMethodParameterDoc> parameters = std::deque<SmartPtrCMethodParameterDoc>(),
			const std::deque<SmartPtrCInstanceParameterDoc> instanceParameters = std::deque<SmartPtrCInstanceParameterDoc>());

	static SmartPtrCMethodDoc createMethod(
			const std::string name,
			const std::deque<SmartPtrCMethodParameterDoc> parameters = std::deque<SmartPtrCMethodParameterDoc>(),
			const std::deque<SmartPtrCInstanceParameterDoc> instanceParameters = std::deque<SmartPtrCInstanceParameterDoc>());

	static SmartPtrCActionClassDoc createActionClass(
			const std::string namespaceVal,
			const std::string name,
			const std::string version,
			const SmartPtrCCollectMethodDoc collectMethod,
			const std::deque<SmartPtrCMethodDoc> methodCollection);

	static SmartPtrCSchemaDoc createSchema(
			const std::deque<SmartPtrCDataClassDoc> dataClasses,
			const std::deque<SmartPtrCActionClassDoc> actionClasses);

	static SmartPtrCDataClassPropertyDoc createDataClassProperty(
			const std::string name,
			const std::string value);

	static SmartPtrCDataClassSubInstanceDoc createDataClassSubInstance(
			const std::string name,
			const std::deque<SmartPtrCDataClassPropertyDoc> properties);

	static SmartPtrCDataClassInstanceDoc createDataClassInstance(
			const std::string namespaceVal,
			const std::string name,
			const std::string version,
			const std::deque<SmartPtrCDataClassPropertyDoc> properties,
			const std::deque<SmartPtrCDataClassSubInstanceDoc> instanceProperties = std::deque<SmartPtrCDataClassSubInstanceDoc>());

private:
	CProviderDocHelper();
	CAF_CM_DECLARE_NOCOPY(CProviderDocHelper);
};

}

#endif /* CProviderDocHelper_H_ */
