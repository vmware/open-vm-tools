/*
 *	 Author: mdonahue
 *  Created: Jan 13, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CCAFEXCEPTION_H_
#define CCAFEXCEPTION_H_


#include "ICafObject.h"

namespace Caf {

class EXCEPTION_LINKAGE CCafException : public ICafObject {
public:
	CCafException();

	CCafException(const std::string& exceptionClassName);

	virtual ~CCafException();

	virtual void throwSelf();

	virtual void throwAddRefedSelf();

	void AddRef();

	void Release();

	void QueryInterface(const IID&, void**);

	void populate(
					const std::string& message,
					const HRESULT errorCode,
					const std::string& className,
					const std::string& funcName);

	void populateVA(const HRESULT errorCode,
					const std::string& className,
					const std::string& funcName,
					const char* format,
					...);

	bool isPopulated() const;

	std::string getExceptionClassName() const;

	virtual std::string getMsg() const;

	std::string getClassName() const;

	std::string getFuncName() const;

	HRESULT getError() const;

    std::deque<std::string>* getBacktrace() const;

	virtual std::string getFullMsg() const;

private:
	gint _refCnt;
	bool _isPopulated;

protected:
	std::string _exceptionClassName;
	HRESULT _errorCode;
	std::string _message;
	std::string _className;
	std::string _funcName;
	std::deque<std::string>* _backtrace;

	CCafException(const CCafException&);
	CCafException& operator=(const CCafException&);
};
CAF_DECLARE_SMART_POINTER(CCafException);

}

#endif /* CCAFEXCEPTION_H_ */
