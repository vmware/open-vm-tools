/*
 *  Created on: Aug 16, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CEventTopicCalculatorInstance_h
#define CEventTopicCalculatorInstance_h


#include "IBean.h"

#include "IVariant.h"
#include "Integration/IIntMessage.h"
#include "Integration/IExpressionInvoker.h"

namespace Caf {

class CEventTopicCalculatorInstance :
	public TCafSubSystemObjectRoot<CEventTopicCalculatorInstance>,
	public IBean,
	public IExpressionInvoker {
public:
	CEventTopicCalculatorInstance();
	virtual ~CEventTopicCalculatorInstance();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdCommIntegrationEventTopicCalculator)

	CAF_BEGIN_INTERFACE_MAP(CEventTopicCalculatorInstance)
		CAF_INTERFACE_ENTRY(IBean)
		CAF_INTERFACE_ENTRY(IExpressionInvoker)
	CAF_END_INTERFACE_MAP()

public: // IBean
	void initializeBean(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties);

	void terminateBean();

public: // IExpressionInvoker
	SmartPtrIVariant invokeExpression(
			const std::string& methodName,
			const Cdeqstr& methodParams,
			const SmartPtrIIntMessage& message);

private:
	SmartPtrIVariant getTopic(
			const SmartPtrIIntMessage& message);

private:
	bool _isInitialized;
	CAF_CM_CREATE;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(CEventTopicCalculatorInstance);
};

}

#endif /* CEventTopicCalculatorInstance_h */
