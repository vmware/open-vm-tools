/*
 *	 Author: mdonahue
 *  Created: Jan 13, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CLASSMACROS_H_
#define CLASSMACROS_H_

//  Prevents class creation
#define CAF_CM_DECLARE_NOCREATE(_classname_) \
	private:\
	_classname_();\
	~_classname_();\
	_classname_(const _classname_&);\
	_classname_& operator=(const _classname_&)

//  Prevents copying
#define CAF_CM_DECLARE_NOCOPY(_classname_) \
	private:\
	_classname_(const _classname_&);\
	_classname_& operator=(const _classname_&)

//  Initializes the members of the class
#define CAF_CM_INIT( _ClassName_ ) _cm_className_( _ClassName_ )

// Sets up a class method for parameter validation only
#define CAF_CM_FUNCNAME_VALIDATE( _szFuncNameIn_ ) \
	const char * _cm_funcName_ = _szFuncNameIn_

// Sets up a class method for exception handling/parameter validation
#define CAF_CM_FUNCNAME( _szFuncNameIn_ ) \
	CAF_CM_FUNCNAME_VALIDATE ( _szFuncNameIn_); \
	CCafException* _cm_exception_ = NULL

#define CAF_CM_CREATE \
	private: \
	const char * _cm_className_

#define CAF_CM_STATIC_FUNC_VALIDATE( _szScope_, _szFuncName_ ) \
	const char * _cm_className_ = _szScope_; \
	CAF_CM_FUNCNAME_VALIDATE( _szFuncName_ )

#define CAF_CM_STATIC_FUNC( _szScope_, _szFuncName_ ) \
	const char * _cm_className_ = _szScope_; \
	CAF_CM_FUNCNAME( _szFuncName_ )

#define CAF_CM_GET_FUNCNAME _cm_funcName_

#define CAF_CM_GET_CLASSNAME _cm_className_

#define CAF_CM_ENTER (void)0;

#define CAF_CM_EXIT (void)0

#define CAF_CM_UPTLINE (void)0

// Class-level thread safety macros
#define CAF_CM_CREATE_THREADSAFE \
	private: \
	mutable Caf::SmartPtrCAutoRecMutex _cm_mutex_

#define CAF_CM_INIT_THREADSAFE \
	_cm_mutex_.CreateInstance(); \
	_cm_mutex_->initialize()

#define CAF_CM_ENTER_AND_LOCK \
	Caf::CAutoMutexLockUnlock _auto_lock_unlock(_cm_mutex_); \
	CAF_CM_ENTER

#define CAF_CM_UNLOCK_AND_EXIT CAF_CM_EXIT

#define CAF_CM_LOCK _cm_mutex_->lock()

#define CAF_CM_UNLOCK _cm_mutex_->unlock()


#define CAF_CM_LOCK_UNLOCK Caf::CAutoMutexLockUnlock _auto_lock_unlock(_cm_mutex_)

#define CAF_CM_UNLOCK_LOCK Caf::CAutoMutexUnlockLock _auto_unlock_lock(_cm_mutex_)

#define CAF_CM_LOCK_UNLOCK_LOG Caf::CAutoMutexLockUnlock _auto_lock_unlock(_cm_mutex_, CAF_CM_GET_CLASSNAME, CAF_CM_GET_FUNCNAME, __LINE__)

#define CAF_CM_UNLOCK_LOCK_LOG Caf::CAutoMutexUnlockLock _auto_unlock_lock(_cm_mutex_, CAF_CM_GET_CLASSNAME, CAF_CM_GET_FUNCNAME, __LINE__)


#define CAF_CM_LOCK_UNLOCK1(_this_mutex_) Caf::CAutoMutexLockUnlock _auto_lock_unlock1(_this_mutex_)

#define CAF_CM_UNLOCK_LOCK1(_this_mutex_) Caf::CAutoMutexUnlockLock _auto_unlock_lock1(_this_mutex_)

#define CAF_CM_LOCK_UNLOCK1_LOG(_this_mutex_) Caf::CAutoMutexLockUnlock _auto_lock_unlock1(_this_mutex_, CAF_CM_GET_CLASSNAME, CAF_CM_GET_FUNCNAME, __LINE__)

#define CAF_CM_UNLOCK_LOCK1_LOG(_this_mutex_) Caf::CAutoMutexUnlockLock _auto_unlock_lock1(_this_mutex_, CAF_CM_GET_CLASSNAME, CAF_CM_GET_FUNCNAME, __LINE__)

#endif /* CLASSMACROS_H_ */
