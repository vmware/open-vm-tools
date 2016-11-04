/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CIntException_h_
#define CIntException_h_

#include "Exception/CCafException.h"

#include "Integration/IThrowable.h"
#include "Integration/Core/CIntException.h"

namespace Caf {

class INTEGRATIONCORE_LINKAGE CIntException :
	public IThrowable {
public:
	CIntException();
	virtual ~CIntException();

public:
	void initialize(const CCafException* cafException);

public: // IThrowable
	std::string getExceptionClassName() const;
	std::string getMsg() const;
	std::string getClassName() const;
	std::string getFuncName() const;
	HRESULT getError() const;
	std::deque<std::string>* getBacktrace() const;
	std::string getFullMsg() const;

private:
	bool _isInitialized;
	std::string _exceptionClassName;
	HRESULT _errorCode;
	std::string _message;
	std::string _className;
	std::string _funcName;
	std::deque<std::string>* _backtrace;
	std::string _fullMsg;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CIntException);
};

CAF_DECLARE_SMART_POINTER(CIntException);

#define CAF_CM_DECLARE_INTEGRATION_EXCEPTION_CLASS(_exclass_) \
class INTEGRATIONCORE_LINKAGE _exclass_ : public Caf::CCafException { \
public: \
	_exclass_(); \
	virtual ~_exclass_(); \
	void throwSelf(); \
private: \
	_exclass_(const _exclass_ &); \
	_exclass_ & operator=(const _exclass_ &); \
};

// Exceptions specific to integration
CAF_CM_DECLARE_INTEGRATION_EXCEPTION_CLASS(FatalListenerStartupException)

CAF_CM_DECLARE_INTEGRATION_EXCEPTION_CLASS(ListenerExecutionFailedException)

CAF_CM_DECLARE_INTEGRATION_EXCEPTION_CLASS(MessageDeliveryException)

}

#endif // #ifndef CIntException_h_
