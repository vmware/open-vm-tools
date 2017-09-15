/*
 *  Created on: Jul 23, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CEXPRESSIONHANDLER_H_
#define CEXPRESSIONHANDLER_H_


#include "Common/IAppConfig.h"
#include "Common/IAppContext.h"
#include "IVariant.h"
#include "Integration/IExpressionInvoker.h"

namespace Caf {

class INTEGRATIONCORE_LINKAGE CExpressionHandler {
public:
	CExpressionHandler();
	virtual ~CExpressionHandler();

	void init(
			const SmartPtrIAppConfig& appConfig,
			const SmartPtrIAppContext& appContext,
			const std::string& expression);

	SmartPtrIVariant evaluate(const SmartPtrIIntMessage& message);

	std::string getBeanName() const;

	std::string getMethodName() const;

	Cdeqstr getMethodParameters() const;

	std::string toString() const;

private:
	bool _isInitialized;
	SmartPtrIExpressionInvoker _invoker;
	std::string _beanName;
	std::string _methodName;
	Cdeqstr _methodParams;

	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CExpressionHandler);
};
CAF_DECLARE_SMART_POINTER(CExpressionHandler);

}

#endif /* CEXPRESSIONHANDLER_H_ */
