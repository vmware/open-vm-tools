/*
 *	 Author: mdonahue
 *  Created: Jan 26, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef EXCEPTIONMACROS_H_
#define EXCEPTIONMACROS_H_

#define CAF_CM_EXCEPTION_EFAIL( _msg_ ) \
	{ \
		_cm_exception_ = new Caf::CCafException(); \
		_cm_exception_->populate(_msg_, E_FAIL, _cm_className_, _cm_funcName_); \
		_cm_exception_->AddRef(); \
		throw _cm_exception_; \
	}

#define CAF_CM_EXCEPTION_VA0(_code_, _msg_) \
		{ \
		_cm_exception_ = new Caf::CCafException(); \
		_cm_exception_->populate(_msg_, _code_, _cm_className_, _cm_funcName_); \
		_cm_exception_->AddRef(); \
		throw _cm_exception_; \
	}

#define CAF_CM_EXCEPTION_VA1(_code_, _fmt_, _arg1_) \
		{ \
		_cm_exception_ = new Caf::CCafException(); \
		_cm_exception_->populateVA(_code_, _cm_className_, _cm_funcName_, _fmt_, _arg1_); \
		_cm_exception_->AddRef(); \
		throw _cm_exception_; \
	}

#define CAF_CM_EXCEPTION_VA2(_code_, _fmt_, _arg1_, _arg2_) \
		{ \
		_cm_exception_ = new Caf::CCafException(); \
		_cm_exception_->populateVA(_code_, _cm_className_, _cm_funcName_, _fmt_, _arg1_, _arg2_); \
		_cm_exception_->AddRef(); \
		throw _cm_exception_; \
	}

#define CAF_CM_EXCEPTION_VA3(_code_, _fmt_, _arg1_, _arg2_, _arg3_) \
		{ \
		_cm_exception_ = new Caf::CCafException(); \
		_cm_exception_->populateVA(_code_, _cm_className_, _cm_funcName_, _fmt_, _arg1_, _arg2_, _arg3_); \
		_cm_exception_->AddRef(); \
		throw _cm_exception_; \
	}

#define CAF_CM_EXCEPTION_VA4(_code_, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_) \
		{ \
		_cm_exception_ = new Caf::CCafException(); \
		_cm_exception_->populateVA(_code_, _cm_className_, _cm_funcName_, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_); \
		_cm_exception_->AddRef(); \
		throw _cm_exception_; \
	}

#define CAF_CM_EXCEPTION_VA5(_code_, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_) \
		{ \
		_cm_exception_ = new Caf::CCafException(); \
		_cm_exception_->populateVA(_code_, _cm_className_, _cm_funcName_, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_); \
		_cm_exception_->AddRef(); \
		throw _cm_exception_; \
	}

#define CAF_CM_EXCEPTION_VA6(_code_, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_, _arg6_) \
		{ \
		_cm_exception_ = new Caf::CCafException(); \
		_cm_exception_->populateVA(_code_, _cm_className_, _cm_funcName_, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_, _arg6_); \
		_cm_exception_->AddRef(); \
		throw _cm_exception_; \
	}


#define CAF_CM_CLEAREXCEPTION \
	if(NULL != _cm_exception_) { _cm_exception_->Release(); _cm_exception_ = NULL; }

#define CAF_CM_ISEXCEPTION \
	((NULL == _cm_exception_) ? false : true)

#define CAF_CM_EXCEPTION_GET_FULLMSG \
	(NULL == _cm_exception_) ? std::string() : _cm_exception_->getFullMsg()

#define CAF_CM_EXCEPTION_GET_MSG \
	(NULL == _cm_exception_) ? std::string() : _cm_exception_->getMsg()

#define CAF_CM_EXCEPTION_GET_ERROR \
	(NULL == _cm_exception_) ? S_OK : _cm_exception_->getError()

#define CAF_CM_THROWEXCEPTION \
	if(NULL != _cm_exception_) { _cm_exception_->throwSelf(); }

#define CAF_CM_GETEXCEPTION \
	_cm_exception_

// Catch exceptions and turn them into CafException
#define CAF_CM_CATCH_CAF \
	catch (CCafException* _catchException_) { \
		if(NULL == _cm_exception_) { \
			_cm_exception_ = _catchException_; \
		} \
	}

#define CAF_CM_CATCH_HRESULT \
	catch (HRESULT& _catchException_) { \
		_cm_exception_ = new Caf::CCafException(); \
		_cm_exception_->populate("HRESULT Exception", _catchException_, _cm_className_, _cm_funcName_); \
		_cm_exception_->AddRef(); \
	}

#define CAF_CM_CATCH_STL \
	catch (exception& _catchException_) { \
		const std::string _msg_ = std::string("STL Exception: " ) + std::string( _catchException_.what() ); \
		_cm_exception_ = new Caf::CCafException(); \
		_cm_exception_->populate(_msg_, E_FAIL, _cm_className_, _cm_funcName_); \
		_cm_exception_->AddRef(); \
	}

#define CAF_CM_CATCH_STD \
	catch (const std::exception& _catchException_) { \
		const std::string _msg_ = std::string("STD Exception: " ) + std::string( _catchException_.what() ); \
		_cm_exception_ = new Caf::CCafException(); \
		_cm_exception_->populate(_msg_, E_FAIL, _cm_className_, _cm_funcName_); \
		_cm_exception_->AddRef(); \
	}

#define CAF_CM_CATCH_DEFAULT \
	catch (...) { \
		_cm_exception_ = new Caf::CCafException(); \
		_cm_exception_->populate("Default Exception", E_FAIL, _cm_className_, _cm_funcName_); \
		_cm_exception_->AddRef(); \
	}

#define CAF_CM_CATCH_GERROR \
	catch (GError* _catchException_) { \
		_cm_exception_ = new Caf::CCafException(); \
		_cm_exception_->populate(_catchException_->message, _catchException_->code, _cm_className_, _cm_funcName_); \
		_cm_exception_->AddRef(); \
		g_error_free(_catchException_); \
	}

#define CAF_CM_THROW_GERROR(_err_) { \
		_cm_exception_ = new Caf::CCafException(); \
		_cm_exception_->populate(_err_->message, _err_->code, _cm_className_, _cm_funcName_); \
		_cm_exception_->AddRef(); \
		g_error_free(_err_); \
		_cm_exception_->throwSelf(); }

#define CAF_CM_CATCH_ALL \
	CAF_CM_CATCH_CAF \
	CAF_CM_CATCH_HRESULT \
	CAF_CM_CATCH_STD \
	CAF_CM_CATCH_GERROR \
	CAF_CM_CATCH_DEFAULT

#endif /* EXCEPTIONMACROS_H_ */
