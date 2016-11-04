/*
 *  Created on: Oct 6, 2011
 *      Author: mdonahue
 *
 *  Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef EXEXCEPTIONEXMACROS_H_
#define EXEXCEPTIONEXMACROS_H_

#define CAF_CM_EXCEPTIONEX_VA0(_exclass_, _code_, _msg_) \
		{ \
		_cm_exception_ = new _exclass_(); \
		_cm_exception_->populate(_msg_, _code_, _cm_className_, _cm_funcName_); \
		_cm_exception_->AddRef(); \
		throw static_cast<_exclass_*>(_cm_exception_); \
	}

#define CAF_CM_EXCEPTIONEX_VA1(_exclass_, _code_, _fmt_, _arg1_) \
		{ \
		_cm_exception_ = new _exclass_(); \
		_cm_exception_->populateVA(_code_, _cm_className_, _cm_funcName_, _fmt_, _arg1_); \
		_cm_exception_->AddRef(); \
		throw static_cast<_exclass_*>(_cm_exception_); \
	}

#define CAF_CM_EXCEPTIONEX_VA2(_exclass_, _code_, _fmt_, _arg1_, _arg2_) \
		{ \
		_cm_exception_ = new _exclass_(); \
		_cm_exception_->populateVA(_code_, _cm_className_, _cm_funcName_, _fmt_, _arg1_, _arg2_); \
		_cm_exception_->AddRef(); \
		throw static_cast<_exclass_*>(_cm_exception_); \
	}

#define CAF_CM_EXCEPTIONEX_VA3(_exclass_, _code_, _fmt_, _arg1_, _arg2_, _arg3_) \
		{ \
		_cm_exception_ = new _exclass_(); \
		_cm_exception_->populateVA(_code_, _cm_className_, _cm_funcName_, _fmt_, _arg1_, _arg2_, _arg3_); \
		_cm_exception_->AddRef(); \
		throw static_cast<_exclass_*>(_cm_exception_); \
	}

#define CAF_CM_EXCEPTIONEX_VA4(_exclass_, _code_, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_) \
		{ \
		_cm_exception_ = new _exclass_(); \
		_cm_exception_->populateVA(_code_, _cm_className_, _cm_funcName_, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_); \
		_cm_exception_->AddRef(); \
		throw static_cast<_exclass_*>(_cm_exception_); \
	}

#define CAF_CM_EXCEPTIONEX_VA5(_exclass_, _code_, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_) \
		{ \
		_cm_exception_ = new _exclass_(); \
		_cm_exception_->populateVA(_code_, _cm_className_, _cm_funcName_, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_); \
		_cm_exception_->AddRef(); \
		throw static_cast<_exclass_*>(_cm_exception_); \
	}

#define CAF_CM_EXCEPTIONEX_VA6(_exclass_, _code_, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_, _arg6_) \
		{ \
		_cm_exception_ = new _exclass_(); \
		_cm_exception_->populateVA(_code_, _cm_className_, _cm_funcName_, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_, _arg6_); \
		_cm_exception_->AddRef(); \
		throw static_cast<_exclass_*>(_cm_exception_); \
	}

#endif /* EXEXCEPTIONEXMACROS_H_ */
