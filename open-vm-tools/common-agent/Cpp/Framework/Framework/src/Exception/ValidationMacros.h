/*
 *	 Author: mdonahue
 *  Created: Jan 13, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef VALIDATIONMACROS_H_
#define VALIDATIONMACROS_H_

// String constants used in validation
const char _sPRECOND_ISCONSTRUCTED[] = "Pre-condition failed because object was not constructed successfully";
const char _sPRECOND_ISINITIALIZED[] = "Pre-condition failed because object was not initialized successfully";
const char _sPRECOND_ISNOTINITIALIZED[] = "Pre-condition failed because object has already been initialized";
const char _sPRECOND_ISNOTREADONLY[] = "Pre-condition failed because the object is read-only.";

const char _sVALIDATE_STRINGEMPTY[] ="Invalid Argument because a string is empty:";
const char _sVALIDATE_STRINGPTRNULL[] ="Invalid Argument because a string pointer is null:";
const char _sVALIDATE_STRINGPTREMPTY[] ="Invalid Argument because a string pointer is empty:";
const char _sVALIDATE_PTRARRAYNULL[] ="Invalid Argument because a pointer array is null:";
const char _sVALIDATE_PTRARRAYEMPTY[] ="Invalid Argument because a pointer array is empty:";
const char _sVALIDATE_STL[] ="Invalid Argument because an STL container is empty:";
const char _sVALIDATE_STL_EMPTY[] ="Invalid Argument because an STL container is not empty:";
const char _sVALIDATE_STL_ITERATOR[] ="Invalid Argument because an STL iterator is at the end of the containter:";
const char _sVALIDATE_INTERFACE[] ="Invalid Argument because an interface is NULL:";
const char _sVALIDATE_ZERO[] ="Invalid Argument because a number is not equal to zero:";
const char _sVALIDATE_ISNOTZERO[] ="Invalid Argument because a number is equal to zero:";
const char _sVALIDATE_POSITIVE[] ="Invalid Argument because a number is less-than or equal-to zero:";
const char _sVALIDATE_NEGATIVE[] ="Invalid Argument because a number is greater-than or equal-to zero:";
const char _sVALIDATE_NONNEGATIVE[] ="Invalid Argument because a number is less-than zero:";
const char _sVALIDATE_NOTNULL[] ="Invalid Argument is NULL:";
const char _sVALIDATE_NULL[] ="Invalid Argument must be NULL:";
const char _sVALIDATE_BOOL[] ="Invalid Argument because a bool is false:";
const char _sVALIDATE_GUID[] ="Invalid Argument because a GUID is NULL:";
const char _sASSERT_FAILED[] ="Assertion failed:";

// Used internally by some of these macros.
#define CAF_CM_EXCEPTION_VALIDATE( _valmsg_, _variable_text_ ) \
		CAF_CM_EXCEPTION_VA2(E_INVALIDARG, "%s %s", _valmsg_, _variable_text_)

//  Some helper macros that make it easy to validate that the object pre-conditions have been met.
#define CAF_CM_PRECOND_ISCONSTRUCTED( _bIsConstucted_ ) \
    CValidate::constructed(_bIsConstucted_, _cm_className_, _cm_funcName_)

#define CAF_CM_PRECOND_ISINITIALIZED( _bIsInitialized_ ) \
    CValidate::initialized(_bIsInitialized_, _cm_className_, _cm_funcName_)

#define CAF_CM_PRECOND_ISNOTINITIALIZED( _bIsInitialized_ ) \
    CValidate::notInitialized(_bIsInitialized_, _cm_className_, _cm_funcName_)

//  Some helper macros that make is easy to validate input arguments.
#define CAF_CM_VALIDATE_STRING( _validate_ ) \
    CValidate::notEmptyStr(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_STRINGPTRW( _validate_ ) \
    CValidate::notNullOrEmptyStr(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_STRINGPTRA( _validate_ ) \
    CValidate::notNullOrEmptyStr(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_ZERO( _validate_) \
    CValidate::zero(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_NOTZERO( _validate_) \
    CValidate::notZero(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_POSITIVE( _validate_ ) \
    CValidate::positive(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_NEGATIVE( _validate_ ) \
    CValidate::negative(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_NONNEGATIVE( _validate_ ) \
    CValidate::nonNegative(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_NONNEGATIVE_INT64( _validate_ ) \
    CValidate::nonNegative64(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_BOOL( _validate_ ) \
    CValidate::isTrue(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_GUID( _validate_ ) \
    CValidate::notEmptyUuid(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_INTERFACE( _validate_ ) \
    CValidate::notNullInterface(_validate_.GetNonAddRefedInterface(), #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_INTERFACEPTR( _validate_ ) \
    CValidate::notNullInterface(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_PTR( _validate_ ) \
    CValidate::notNullPtr(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_NULLPTR( _validate_ ) \
    CValidate::nullPtr(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_SMARTPTR( _validate_ ) \
    CValidate::notNullPtr(_validate_.GetNonAddRefedInterface(), #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_PTRARRAY( _validate_ ) \
    CValidate::notNullOrEmptyPtrArr(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_STL( _validate_ ) \
    CValidate::notEmptyStl(_validate_.size(), #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATE_STL_EMPTY( _validate_ ) \
    CValidate::emptyStl(_validate_.size(), #_validate_, _cm_className_, _cm_funcName_)

// Not included in CValidate either because these macros are not currently used or because
// the types are too varied to include in a class.
#define CAF_CM_VALIDATE_STL_ITERATOR( _stlContainer_, _stlIterator ) \
	if( _stlIterator == _stlContainer_.end() ) \
		CAF_CM_EXCEPTION_VALIDATE( _sVALIDATE_STL_ITERATOR, #_stlContainer_ )

#define CAF_CM_VALIDATE_SMARTSTL_NOTEMPTY( _validate_ ) \
    if( _validate_.IsNull() || _validate_->empty() ) \
        CAF_CM_EXCEPTION_VALIDATE( _sVALIDATE_STL, #_validate_ )

#define CAF_CM_VALIDATE_COND_VA1( _cond_, _fmt_, _arg1_ ) \
    if(! (_cond_) ) { \
    	const std::string _fullfmt_ = std::string(#_cond_) + " - " + std::string(_fmt_); \
    	CAF_CM_EXCEPTION_VA1(E_INVALIDARG, _fullfmt_.c_str(), _arg1_); \
    }

#define CAF_CM_VALIDATE_COND_VA2( _cond_, _fmt_, _arg1_, _arg2_ ) \
    if(! (_cond_) ) { \
    	const std::string _fullfmt_ = std::string(#_cond_) + " - " + std::string(_fmt_); \
    	CAF_CM_EXCEPTION_VA2(E_INVALIDARG, _fullfmt_.c_str(), _arg1_, _arg2_); \
    }

#define CAF_CM_VALIDATE_COND_VA3( _cond_, _fmt_, _arg1_, _arg2_, _arg3_ ) \
    if(! (_cond_) ) { \
    	const std::string _fullfmt_ = std::string(#_cond_) + " - " + std::string(_fmt_); \
    	CAF_CM_EXCEPTION_VA3(E_INVALIDARG, _fullfmt_.c_str(), _arg1_, _arg2_, _arg3_); \
    }

// Generic assert
#define CAF_CM_ASSERT(_exp_) \
	if (!(_exp_)) \
		CAF_CM_EXCEPTION_VA2(E_FAIL, "%s %s", _sASSERT_FAILED, #_exp_ )

#define CAF_CM_ASSERT_MSG(_exp_, _msg_) \
	if (!(_exp_)) \
		CAF_CM_EXCEPTION_VA1( E_FAIL, "%s", _msg_ );

#endif /* VALIDATIONMACROS_H_ */
