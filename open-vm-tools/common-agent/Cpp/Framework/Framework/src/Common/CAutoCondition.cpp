/*
 *	 Author: bwilliams
 *  Created: Oct 29, 2014
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/CAutoMutex.h"
#include "Common/CAutoCondition.h"

using namespace Caf;

CAutoCondition::CAutoCondition() :
	_isInitialized (false),
	CAF_CM_INIT("CAutoCondition") {
	::g_cond_init(&_condition);
}

CAutoCondition::~CAutoCondition() {
	::g_cond_clear(&_condition);
}

void CAutoCondition::initialize(const std::string& name) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_name = name;
	_isInitialized = true;
}

bool CAutoCondition::isInitialized() const {
	return _isInitialized;
}

void CAutoCondition::close() {
	CAF_CM_FUNCNAME_VALIDATE("close");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	if (_isInitialized) {
		::g_cond_clear(&_condition);
		_name.clear();
		_isInitialized = false;
	}
}

std::string CAutoCondition::getName() const {
	CAF_CM_FUNCNAME_VALIDATE("getName");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _name;
}

void CAutoCondition::signal() {
	CAF_CM_FUNCNAME_VALIDATE("signal");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	::g_cond_signal(&_condition);
}

void CAutoCondition::wait(SmartPtrCAutoMutex& mutex) {
	CAF_CM_FUNCNAME_VALIDATE("wait");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(mutex);

	::g_cond_wait(&_condition, mutex->getNonConstPtr());
}

bool CAutoCondition::waitUntil(SmartPtrCAutoMutex& mutex, gint64 endTime) {
	CAF_CM_FUNCNAME_VALIDATE("waitUntil");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(mutex);

	gboolean isSignaled = ::g_cond_wait_until(&_condition, mutex->getNonConstPtr(), endTime);
	return (FALSE == isSignaled) ? false : true;
}
