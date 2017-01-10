/*
 *	 Author: mdonahue
 *  Created: Jan 13, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef VALIDATIONMACROSRAW_H_
#define VALIDATIONMACROSRAW_H_

//  Some helper macros that make it easy to validate that the object pre-conditions have been met.
#define CAF_CM_PRECONDRAW_ISCONSTRUCTED( _cm_className_, _cm_funcName_, _bIsConstucted_ ) \
    CValidate::constructed(_bIsConstucted_, _cm_className_, _cm_funcName_)

#define CAF_CM_PRECONDRAW_ISINITIALIZED( _cm_className_, _cm_funcName_, _bIsInitialized_ ) \
    CValidate::initialized(_bIsInitialized_, _cm_className_, _cm_funcName_)

#define CAF_CM_PRECONDRAW_ISNOTINITIALIZED( _cm_className_, _cm_funcName_, _bIsInitialized_ ) \
    CValidate::notInitialized(_bIsInitialized_, _cm_className_, _cm_funcName_)

//  Some helper macros that make is easy to validate input arguments.
#define CAF_CM_VALIDATERAW_STRING( _cm_className_, _cm_funcName_, _validate_ ) \
    CValidate::notEmptyStr(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_STRINGPTRW( _cm_className_, _cm_funcName_, _validate_ ) \
    CValidate::notNullOrEmptyStr(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_STRINGPTRA( _cm_className_, _cm_funcName_, _validate_ ) \
    CValidate::notNullOrEmptyStr(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_ZERO( _cm_className_, _cm_funcName_, _validate_) \
    CValidate::zero(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_NOTZERO( _cm_className_, _cm_funcName_, _validate_) \
    CValidate::notZero(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_POSITIVE( _cm_className_, _cm_funcName_, _validate_ ) \
    CValidate::positive(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_NEGATIVE( _cm_className_, _cm_funcName_, _validate_ ) \
    CValidate::negative(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_NONNEGATIVE( _cm_className_, _cm_funcName_, _validate_ ) \
    CValidate::nonNegative(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_BOOL( _cm_className_, _cm_funcName_, _validate_ ) \
    CValidate::isTrue(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_GUID( _cm_className_, _cm_funcName_, _validate_ ) \
    CValidate::notEmptyUuid(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_INTERFACE( _cm_className_, _cm_funcName_, _validate_ ) \
    CValidate::notNullInterface(_validate_.GetNonAddRefedInterface(), #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_INTERFACEPTR( _cm_className_, _cm_funcName_, _validate_ ) \
    CValidate::notNullInterface(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_PTR( _cm_className_, _cm_funcName_, _validate_ ) \
    CValidate::notNullPtr(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_NULLPTR( _cm_className_, _cm_funcName_, _validate_ ) \
    CValidate::nullPtr(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_SMARTPTR( _cm_className_, _cm_funcName_, _validate_ ) \
    CValidate::notNullPtr(_validate_.GetNonAddRefedInterface(), #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_PTRARRAY( _cm_className_, _cm_funcName_, _validate_ ) \
    CValidate::notNullOrEmptyPtrArr(_validate_, #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_STL( _cm_className_, _cm_funcName_, _validate_ ) \
    CValidate::notEmptyStl(_validate_.size(), #_validate_, _cm_className_, _cm_funcName_)

#define CAF_CM_VALIDATERAW_STL_EMPTY( _cm_className_, _cm_funcName_, _validate_ ) \
    CValidate::emptyStl(_validate_.size(), #_validate_, _cm_className_, _cm_funcName_)

#endif /* VALIDATIONMACROSRAW_H_ */
