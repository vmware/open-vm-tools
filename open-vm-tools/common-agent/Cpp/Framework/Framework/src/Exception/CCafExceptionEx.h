/*
 *  Created on: Oct 6, 2011
 *      Author: mdonahue
 *
 *  Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CCAFEXCEPTIONEX_H_
#define CCAFEXCEPTIONEX_H_


#define CAF_CM_DECLARE_EXCEPTION_CLASS(_exclass_) \
class EXCEPTION_LINKAGE _exclass_ : public Caf::CCafException { \
public: \
	_exclass_(); \
	virtual ~_exclass_(); \
	void throwSelf(); \
	void throwAddRefedSelf(); \
private: \
	_exclass_(const _exclass_ &); \
	_exclass_ & operator=(const _exclass_ &); \
}; \
typedef TCafSmartPtr<_exclass_, TCafObject<_exclass_> > SmartPtr##_exclass_

#define CAF_CM_DEFINE_EXCEPTION_CLASS(_exclass_) \
	_exclass_::_exclass_() : CCafException( #_exclass_ ) {} \
	_exclass_::~_exclass_() {} \
	void _exclass_::throwSelf() { throw this; } \
	void _exclass_::throwAddRefedSelf() { this->AddRef(); throw this; }

#include "Exception/CCafException.h"

namespace Caf {

// General Runtime Exceptions
CAF_CM_DECLARE_EXCEPTION_CLASS(AccessDeniedException);
CAF_CM_DECLARE_EXCEPTION_CLASS(NullPointerException);
CAF_CM_DECLARE_EXCEPTION_CLASS(BufferOverflowException);
CAF_CM_DECLARE_EXCEPTION_CLASS(BufferUnderflowException);
CAF_CM_DECLARE_EXCEPTION_CLASS(InvalidArgumentException);
CAF_CM_DECLARE_EXCEPTION_CLASS(IllegalStateException);
CAF_CM_DECLARE_EXCEPTION_CLASS(IndexOutOfBoundsException);
CAF_CM_DECLARE_EXCEPTION_CLASS(NoSuchElementException);
CAF_CM_DECLARE_EXCEPTION_CLASS(DuplicateElementException);
CAF_CM_DECLARE_EXCEPTION_CLASS(UnsupportedOperationException);
CAF_CM_DECLARE_EXCEPTION_CLASS(UnsupportedVersionException);
CAF_CM_DECLARE_EXCEPTION_CLASS(InvalidHandleException);
CAF_CM_DECLARE_EXCEPTION_CLASS(TimeoutException);
CAF_CM_DECLARE_EXCEPTION_CLASS(NoSuchInterfaceException);
CAF_CM_DECLARE_EXCEPTION_CLASS(ProcessFailedException);

// AppConfig Exceptions
CAF_CM_DECLARE_EXCEPTION_CLASS(NoSuchConfigSectionException);
CAF_CM_DECLARE_EXCEPTION_CLASS(NoSuchConfigValueException);

// Dynamic Library Exception
CAF_CM_DECLARE_EXCEPTION_CLASS(LibraryFailedToLoadException);
CAF_CM_DECLARE_EXCEPTION_CLASS(NoSuchMethodException);

// I/O Exceptions
CAF_CM_DECLARE_EXCEPTION_CLASS(IOException);
CAF_CM_DECLARE_EXCEPTION_CLASS(EOFException);
CAF_CM_DECLARE_EXCEPTION_CLASS(FileNotFoundException);
CAF_CM_DECLARE_EXCEPTION_CLASS(PathNotFoundException);
CAF_CM_DECLARE_EXCEPTION_CLASS(FileLockedException);

}

#endif

